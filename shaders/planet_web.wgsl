// Procedural Planet Renderer — WebGPU WGSL
// Ported from planet_vk.frag (Vulkan GLSL 450)
// Based on Julien Sulpis "Procedural Blue Planet"
// Extended with ridged noise, craters, continents, volumetric clouds, black holes,
// 7 noise types, emissive star rendering, and improved cloud system

// ── Constants ────────────────────────────────────────────────────────────────
const EPSILON: f32 = 1e-3;
const INFINITY_DIST: f32 = 1e10;
const PI: f32 = 3.14159265;

// ── Uniform buffer ───────────────────────────────────────────────────────────
// Layout matches PlanetUniformsVk (std140 rules, 544 bytes).
// mat3 in std140 is stored as 3 x vec4 (xyz used, w padding).

struct PlanetUniforms {
    invView:            mat4x4<f32>,    // 64 bytes
    planetRot_col0:     vec4f,          // 16 bytes (xyz used)
    planetRot_col1:     vec4f,          // 16 bytes
    planetRot_col2:     vec4f,          // 16 bytes
    cameraPos_time:     vec4f,
    planetPos_radius:   vec4f,
    resolution_rot:     vec4f,
    noiseParams:        vec4f,
    fbmParams:          vec4f,
    terrainFeatures:    vec4f,
    bandingPolar:       vec4f,
    cloudParams1:       vec4f,
    cloudParams2:       vec4f,
    atmosphereColorPad: vec4f,
    sunDirectionPad:    vec4f,
    sunColorPad:        vec4f,
    deepSpaceColorPad:  vec4f,
    waterColorDeepPad:  vec4f,
    waterColorSurfPad:  vec4f,
    sandColorPad:       vec4f,
    treeColorPad:       vec4f,
    rockColorPad:       vec4f,
    iceColorPad:        vec4f,
    cloudColorPad:      vec4f,
    biomeLevels:        vec4f,
    transitionPad:      vec4f,
    bhParams1:          vec4f,
    bhParams2:          vec4f,
    bhParams3:          vec4f,
    bhDiskTintPad:      vec4f,
    extraParams:        vec4f,
};

// ── Bindings ─────────────────────────────────────────────────────────────────
@group(0) @binding(0) var<uniform> u: PlanetUniforms;
@group(0) @binding(1) var t_noise:   texture_3d<f32>;
@group(0) @binding(2) var s_noise:   sampler;
@group(0) @binding(3) var t_starmap: texture_cube<f32>;
@group(0) @binding(4) var s_starmap: sampler;

// ── Vertex shader ────────────────────────────────────────────────────────────

struct VertexInput {
    @location(0) pos: vec2f,
    @location(1) uv:  vec2f,
};

struct VertexOutput {
    @builtin(position) pos: vec4f,
    @location(0) uv: vec2f,
};

@vertex
fn vs_main(input: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.uv = input.pos;  // -1 to 1 for ray direction
    out.pos = vec4f(input.pos, 0.0, 1.0);
    return out;
}

// ── Structs ──────────────────────────────────────────────────────────────────

struct Material {
    color:    vec3f,
    diffuse:  f32,
    specular: f32,
};

struct Hit {
    len:      f32,
    normal:   vec3f,
    material: Material,
};

struct Sphere {
    position: vec3f,
    radius:   f32,
};

// Result struct for renderStar (replaces GLSL out parameter)
struct StarResult {
    color: vec3f,
    alpha: f32,
};

// Result struct for marchCloudSegment (replaces GLSL inout parameters)
struct CloudMarchResult {
    scattered:     vec3f,
    transmittance: f32,
};

fn makeMiss() -> Hit {
    return Hit(INFINITY_DIST, vec3f(0.0), Material(vec3f(0.0), -1.0, -1.0));
}

// ── mat3 reconstruction ─────────────────────────────────────────────────────
fn getPlanetRotation() -> mat3x3<f32> {
    return mat3x3<f32>(u.planetRot_col0.xyz, u.planetRot_col1.xyz, u.planetRot_col2.xyz);
}

fn getPlanet() -> Sphere {
    return Sphere(u.planetPos_radius.xyz, u.planetPos_radius.w);
}

// ── Utility ──────────────────────────────────────────────────────────────────

fn inverseLerp(v: f32, a: f32, b: f32) -> f32 { return (v - a) / (b - a); }

fn remap(v: f32, inMin: f32, inMax: f32, outMin: f32, outMax: f32) -> f32 {
    return mix(outMin, outMax, inverseLerp(v, inMin, inMax));
}

@diagnostic(off, derivative_uniformity)
fn noise(p: vec3f) -> f32 { return textureSample(t_noise, s_noise, p * 0.05).r; }

fn sphIntersect(ro: vec3f, rd: vec3f, s: Sphere) -> f32 {
    let oc = ro - s.position;
    let b = dot(oc, rd);
    let c = dot(oc, oc) - s.radius * s.radius;
    let h = b * b - c;
    if (h < 0.0) { return -1.0; }
    return -b - sqrt(h);
}

// Returns (near, far) intersection distances; (-1,-1) on miss
fn sphIntersect2(ro: vec3f, rd: vec3f, s: Sphere) -> vec2f {
    let oc = ro - s.position;
    let b = dot(oc, rd);
    let c = dot(oc, oc) - s.radius * s.radius;
    let h = b * b - c;
    if (h < 0.0) { return vec2f(-1.0); }
    let sq = sqrt(h);
    return vec2f(-b - sq, -b + sq);
}

fn rotateY(angle: f32) -> mat3x3<f32> {
    let c = cos(angle);
    let s = sin(angle);
    return mat3x3<f32>(vec3f(c, 0.0, s), vec3f(0.0, 1.0, 0.0), vec3f(-s, 0.0, c));
}

// ── Noise functions ──────────────────────────────────────────────────────────

fn fbm(p: vec3f, octaves: i32, persistence: f32, lacunarity: f32, exponentiation: f32) -> f32 {
    var amplitude = 0.5;
    var frequency = 3.0;
    var total = 0.0;
    var normalization = 0.0;
    let qualityDegradation = 2 - i32(floor(u.noiseParams.y));
    let oct = max(octaves - qualityDegradation, 1);

    for (var i = 0; i < oct; i = i + 1) {
        total += noise(p * frequency) * amplitude;
        normalization += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }

    total /= normalization;
    total = total * 0.8 + 0.1;
    total = pow(total, exponentiation);
    return total;
}

// Ridged FBM — sharp mountain ridges and valleys
fn ridgedFBM(p: vec3f, octaves: i32, persistence: f32, lacunarity: f32) -> f32 {
    var amplitude = 0.5;
    var frequency = 3.0;
    var total = 0.0;
    var normalization = 0.0;
    var weight = 1.0;
    let qualityDegradation = 2 - i32(floor(u.noiseParams.y));
    let oct = max(octaves - qualityDegradation, 1);

    for (var i = 0; i < oct; i = i + 1) {
        var n = noise(p * frequency);
        n = 1.0 - abs(n * 2.0 - 1.0);
        n = n * n;
        n *= weight;
        weight = clamp(n, 0.0, 1.0);
        total += n * amplitude;
        normalization += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }

    return total / normalization;
}

// Voronoi/cellular noise for crater placement
fn craterNoise(p: vec3f) -> f32 {
    let cell = floor(p);
    let frac = fract(p);
    var minDist: f32 = 1e10;

    for (var x = -1; x <= 1; x = x + 1) {
    for (var y = -1; y <= 1; y = y + 1) {
    for (var z = -1; z <= 1; z = z + 1) {
        let neighbor = vec3f(f32(x), f32(y), f32(z));
        let cellId = cell + neighbor;
        let offset = vec3f(
            noise(cellId * 0.37),
            noise(cellId * 0.37 + vec3f(17.3, 0.0, 0.0)),
            noise(cellId * 0.37 + vec3f(0.0, 43.7, 0.0)));
        let dist = length(frac - neighbor - offset);
        minDist = min(minDist, dist);
    }
    }
    }

    let bowl = smoothstep(0.35, 0.0, minDist) * -1.0;
    let rim = smoothstep(0.25, 0.38, minDist) * smoothstep(0.55, 0.38, minDist) * 0.4;
    return bowl + rim;
}

// ── Additional Noise Types (from blackhole branch) ──────────────────────────

// Worley/cellular noise for puffy cloud base shapes
fn worleyNoise(p: vec3f) -> f32 {
    let cell = floor(p);
    let frac = fract(p);
    var minDist: f32 = 1.0;

    for (var x = -1; x <= 1; x = x + 1) {
    for (var y = -1; y <= 1; y = y + 1) {
    for (var z = -1; z <= 1; z = z + 1) {
        let neighbor = vec3f(f32(x), f32(y), f32(z));
        let cellId = cell + neighbor;
        let randomOffset = vec3f(
            noise(cellId * 0.31),
            noise(cellId * 0.31 + vec3f(127.1, 0.0, 0.0)),
            noise(cellId * 0.31 + vec3f(0.0, 269.5, 0.0))
        );
        let point = neighbor + randomOffset;
        let dist = length(frac - point);
        minDist = min(minDist, dist);
    }
    }
    }
    return 1.0 - minDist;
}

// Billowy noise - soft, rounded terrain
fn billowyFBM(p: vec3f, octaves: i32, persistence: f32, lacunarity: f32) -> f32 {
    var amplitude = 0.5;
    var frequency = 3.0;
    var total = 0.0;
    var normalization = 0.0;
    let qualityDegradation = 2 - i32(floor(u.noiseParams.y));
    let oct = max(octaves - qualityDegradation, 1);

    for (var i = 0; i < oct; i = i + 1) {
        var n = noise(p * frequency);
        n = n * n;
        total += n * amplitude;
        normalization += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }

    return total / normalization;
}

// Swiss noise - eroded, holey terrain
fn swissFBM(p: vec3f, octaves: i32, persistence: f32, lacunarity: f32) -> f32 {
    var amplitude = 0.5;
    var frequency = 3.0;
    var total = 0.0;
    var normalization = 0.0;
    var warp = 0.0;
    let qualityDegradation = 2 - i32(floor(u.noiseParams.y));
    let oct = max(octaves - qualityDegradation, 1);

    for (var i = 0; i < oct; i = i + 1) {
        var n = noise((p + warp * 0.15) * frequency);
        n = 1.0 - abs(n * 2.0 - 1.0);
        n = n * n;
        total += n * amplitude;
        normalization += amplitude;
        warp += n * amplitude;
        amplitude *= persistence * (1.0 - n * 0.3);
        frequency *= lacunarity;
    }

    return total / normalization;
}

// Voronoi terrain noise
fn voronoiTerrain(p: vec3f) -> f32 {
    let cell = floor(p);
    let frac = fract(p);
    var minDist: f32 = 1e10;
    var secondDist: f32 = 1e10;

    for (var x = -1; x <= 1; x = x + 1) {
    for (var y = -1; y <= 1; y = y + 1) {
    for (var z = -1; z <= 1; z = z + 1) {
        let neighbor = vec3f(f32(x), f32(y), f32(z));
        let cellId = cell + neighbor;
        let offset = vec3f(
            noise(cellId * 0.37),
            noise(cellId * 0.37 + vec3f(17.3, 0.0, 0.0)),
            noise(cellId * 0.37 + vec3f(0.0, 43.7, 0.0)));
        let dist = length(frac - neighbor - offset);
        if (dist < minDist) {
            secondDist = minDist;
            minDist = dist;
        } else if (dist < secondDist) {
            secondDist = dist;
        }
    }
    }
    }

    return secondDist - minDist;
}

// Hybrid noise - combines multiple types
fn hybridFBM(p: vec3f, octaves: i32, persistence: f32, lacunarity: f32, exponentiation: f32) -> f32 {
    let standard = fbm(p, octaves, persistence, lacunarity, exponentiation);
    let ridged = ridgedFBM(p * 0.8, octaves, persistence, lacunarity);
    let billowy = billowyFBM(p * 1.2, octaves, persistence, lacunarity);

    let blend = noise(p * 0.5);
    return mix(mix(standard, ridged, smoothstep(0.3, 0.7, blend)),
               billowy, smoothstep(0.6, 0.9, blend));
}

// ── Improved Cloud System (from blackhole branch) ───────────────────────────

// Smooth low-frequency noise for large cloud shapes
fn cloudBaseFBM(p: vec3f) -> f32 {
    var total = 0.0;
    var amplitude = 0.6;
    var frequency = 0.4;
    var normalization = 0.0;
    let octaves = 2 + i32(floor(u.noiseParams.y));

    for (var i = 0; i < octaves; i = i + 1) {
        total += noise(p * frequency) * amplitude;
        normalization += amplitude;
        amplitude *= 0.45;
        frequency *= 2.0;
    }
    return total / normalization;
}

// High-frequency detail for cloud edges
fn cloudDetailFBM(p: vec3f) -> f32 {
    var total = 0.0;
    var amplitude = 0.5;
    var frequency = 2.0;
    var normalization = 0.0;

    for (var i = 0; i < 3; i = i + 1) {
        total += noise(p * frequency) * amplitude;
        normalization += amplitude;
        amplitude *= 0.5;
        frequency *= 2.2;
    }
    return total / normalization;
}

// Main cloud density function - creates billowy cumulus shapes
fn cloudNoise(p: vec3f) -> f32 {
    let cloudP = p * 0.3;

    // Base shape from Worley noise (puffy cellular structure)
    let worley = worleyNoise(cloudP * 1.5);

    // Large-scale variation from low-freq FBM
    let baseShape = cloudBaseFBM(cloudP);

    // Domain warping for billowing effect
    let warpOffset = vec3f(
        cloudBaseFBM(cloudP + vec3f(0.0)),
        cloudBaseFBM(cloudP + vec3f(43.2, 17.8, 0.0)),
        cloudBaseFBM(cloudP + vec3f(0.0, 93.1, 27.3))
    );
    let warped = cloudBaseFBM(cloudP + warpOffset * 0.8);

    // Combine: Worley for structure, FBM for shape, warping for billows
    var density = worley * 0.4 + baseShape * 0.35 + warped * 0.25;

    // Add fine detail at edges
    let detail = cloudDetailFBM(p * 0.8);
    density += detail * 0.15 * smoothstep(0.3, 0.6, density);

    return density;
}

// Cheap version for shadow rays
fn cloudNoiseCheap(p: vec3f) -> f32 {
    let cloudP = p * 0.3;
    return cloudBaseFBM(cloudP);
}

// ── Terrain ──────────────────────────────────────────────────────────────────

fn planetNoise(p: vec3f) -> f32 {
    var tp = p * u.noiseParams.z;  // uTerrainScale

    let uNoiseType = i32(u.extraParams.x);
    let uDomainWarpStrength = u.noiseParams.w;
    let uFbmPersistence = u.fbmParams.x;
    let uFbmLacunarity = u.fbmParams.y;
    let uFbmExponentiation = u.fbmParams.z;
    let uFbmOctaves = i32(u.fbmParams.w);
    let uNoiseStrength = u.noiseParams.x;
    let uCraterStrength = u.terrainFeatures.y;
    let uContinentScale = u.terrainFeatures.z;
    let uWaterLevel = u.terrainFeatures.w;
    let uContinentBlend = u.extraParams.y;
    let uSandLevel = u.biomeLevels.x;
    let uTransition = u.transitionPad.x;
    let uRidgedStrength = u.terrainFeatures.x;

    // Apply domain warping for Warped type or if strength > 0
    let applyWarp = (uNoiseType == 3) || (uDomainWarpStrength > 0.0);
    var warpStrength: f32;
    if (uNoiseType == 3) {
        warpStrength = max(0.5, uDomainWarpStrength);
    } else {
        warpStrength = uDomainWarpStrength;
    }

    if (applyWarp && warpStrength > 0.0) {
        let warpOct = max(uFbmOctaves / 2, 2);
        let warpOffset = vec3f(
            fbm(tp, warpOct, uFbmPersistence, uFbmLacunarity, uFbmExponentiation),
            fbm(tp + vec3f(43.235, 23.112, 0.0), warpOct, uFbmPersistence, uFbmLacunarity, uFbmExponentiation),
            fbm(tp + vec3f(0.0, 71.823, 37.156), warpOct, uFbmPersistence, uFbmLacunarity, uFbmExponentiation));
        tp = tp + uDomainWarpStrength * warpOffset;
    }

    // Select noise type based on uniform
    var baseFBM: f32;
    if (uNoiseType == 0) {
        baseFBM = fbm(tp, uFbmOctaves, uFbmPersistence, uFbmLacunarity, uFbmExponentiation);
    } else if (uNoiseType == 1) {
        baseFBM = ridgedFBM(tp, uFbmOctaves, uFbmPersistence, uFbmLacunarity);
    } else if (uNoiseType == 2) {
        baseFBM = billowyFBM(tp, uFbmOctaves, uFbmPersistence, uFbmLacunarity);
        baseFBM = pow(baseFBM, uFbmExponentiation * 0.5);
    } else if (uNoiseType == 3) {
        baseFBM = fbm(tp, uFbmOctaves, uFbmPersistence, uFbmLacunarity, uFbmExponentiation);
    } else if (uNoiseType == 4) {
        baseFBM = voronoiTerrain(tp * 2.0);
        baseFBM = pow(baseFBM, uFbmExponentiation * 0.3);
    } else if (uNoiseType == 5) {
        baseFBM = swissFBM(tp, uFbmOctaves, uFbmPersistence, uFbmLacunarity);
        baseFBM = pow(baseFBM, uFbmExponentiation * 0.5);
    } else if (uNoiseType == 6) {
        baseFBM = hybridFBM(tp, uFbmOctaves, uFbmPersistence, uFbmLacunarity, uFbmExponentiation);
    } else {
        baseFBM = fbm(tp, uFbmOctaves, uFbmPersistence, uFbmLacunarity, uFbmExponentiation);
    }

    // Additional ridged mountains blend (for non-ridged types)
    if (uNoiseType != 1 && uRidgedStrength > 0.0) {
        let ridged = ridgedFBM(tp, uFbmOctaves, uFbmPersistence, uFbmLacunarity);
        baseFBM = mix(baseFBM, ridged, uRidgedStrength);
    }

    var f = baseFBM * uNoiseStrength;

    // Craters
    if (uCraterStrength > 0.0) {
        let craters = craterNoise(tp * 3.0);
        f += craters * uCraterStrength * uNoiseStrength * 0.5;
    }

    // Continental shaping — large-scale land/ocean mask
    if (uContinentScale > 0.0) {
        var continent = noise(p * uContinentScale * 0.15);
        continent += noise(p * uContinentScale * 0.4) * uContinentBlend;

        let landThreshold = 0.35 + uWaterLevel * 2.0;
        let continentMask = smoothstep(landThreshold, landThreshold + uContinentBlend, continent);

        let coastline = (uSandLevel + uWaterLevel) / 5.0;
        let landBase = coastline + 0.001;

        let oceanHeight = f * 0.15;
        let landHeight = landBase + f * continentMask;

        f = mix(oceanHeight, landHeight, continentMask);
    }

    return mix(
        f / 3.0 + uNoiseStrength / 50.0,
        f,
        smoothstep(uSandLevel, uSandLevel + uTransition / 2.0, f * 5.0)
    );
}

fn planetDist(ro: vec3f, rd: vec3f) -> f32 {
    let smoothSphereDist = sphIntersect(ro, rd, getPlanet());
    let intersection = ro + smoothSphereDist * rd;
    let PLANET_ROTATION = getPlanetRotation();
    let intersectionWithRotation = PLANET_ROTATION * (intersection - u.planetPos_radius.xyz) + u.planetPos_radius.xyz;
    let n = planetNoise(intersectionWithRotation);
    let coastline = (u.biomeLevels.x + u.terrainFeatures.w) / 5.0;
    let displacement = max(n, coastline);
    return sphIntersect(ro, rd, Sphere(u.planetPos_radius.xyz, u.planetPos_radius.w + displacement));
}

// Compute normal by finite-differencing planetNoise on the sphere surface
fn planetNormal(hitPos: vec3f) -> vec3f {
    let PLANET_ROTATION = getPlanetRotation();
    let localDir = normalize(hitPos - u.planetPos_radius.xyz);
    let e = max(0.01, 0.03 * smoothstep(1300.0, 300.0, u.resolution_rot.x));

    var tangent: vec3f;
    if (localDir.y < 0.99) {
        tangent = normalize(cross(localDir, vec3f(0.0, 1.0, 0.0)));
    } else {
        tangent = normalize(cross(localDir, vec3f(1.0, 0.0, 0.0)));
    }
    let bitangent = cross(localDir, tangent);

    let c  = PLANET_ROTATION * (hitPos - u.planetPos_radius.xyz) + u.planetPos_radius.xyz;
    let px = PLANET_ROTATION * (localDir * u.planetPos_radius.w + tangent   * e) + u.planetPos_radius.xyz;
    let nx = PLANET_ROTATION * (localDir * u.planetPos_radius.w - tangent   * e) + u.planetPos_radius.xyz;
    let py = PLANET_ROTATION * (localDir * u.planetPos_radius.w + bitangent * e) + u.planetPos_radius.xyz;
    let ny = PLANET_ROTATION * (localDir * u.planetPos_radius.w - bitangent * e) + u.planetPos_radius.xyz;

    let coastline = (u.biomeLevels.x + u.terrainFeatures.w) / 5.0;
    let hc = max(planetNoise(c),  coastline);
    let hpx = max(planetNoise(px), coastline);
    let hnx = max(planetNoise(nx), coastline);
    let hpy = max(planetNoise(py), coastline);
    let hny = max(planetNoise(ny), coastline);

    let dhdx = (hpx - hnx) / (2.0 * e);
    let dhdy = (hpy - hny) / (2.0 * e);

    return normalize(localDir - dhdx * tangent - dhdy * bitangent);
}

// ── Stars & space ────────────────────────────────────────────────────────────

@diagnostic(off, derivative_uniformity)
fn spaceColor(direction: vec3f) -> vec3f {
    // Sample real star positions from cubemap
    var starColor = textureSample(t_starmap, s_starmap, direction).rgb;
    starColor *= 3.0;

    return starColor + u.deepSpaceColorPad.xyz;
}

// ── Star/Emissive Body Rendering ─────────────────────────────────────────────

// Animated stellar surface noise
fn stellarNoise(p: vec3f, time: f32) -> f32 {
    let n1 = noise(p * 3.0 + vec3f(time * 0.1));
    let n2 = noise(p * 6.0 + vec3f(0.0, time * 0.15, 0.0));
    let n3 = noise(p * 12.0 + vec3f(time * 0.2, 0.0, time * 0.1));

    let granulation = n1 * 0.5 + n2 * 0.3 + n3 * 0.2;
    let largeCells = noise(p * 1.5 + vec3f(time * 0.03));

    return mix(granulation, largeCells, 0.3);
}

// Render an emissive star (returns StarResult instead of GLSL out parameter)
fn renderStar(ro: vec3f, rd: vec3f) -> StarResult {
    let star = getPlanet();
    let t = sphIntersect(ro, rd, star);

    if (t < 0.0) {
        return StarResult(vec3f(0.0), 0.0);
    }

    let hitPos = ro + rd * t;
    let localPos = hitPos - star.position;
    let normal = normalize(localPos);

    // Limb darkening
    let viewAngle = abs(dot(normal, -rd));
    let limb = 0.4 + 0.6 * pow(viewAngle, 0.3);

    // Animated surface variation
    let rot = rotateY(u.cameraPos_time.w * 0.1);
    let surfaceCoord = rot * normal * 5.0;
    let variation = stellarNoise(surfaceCoord, u.cameraPos_time.w) * 0.2 + 0.9;

    // Base star color
    let baseColor = vec3f(1.0, 0.9, 0.5);

    let surfaceColor = baseColor * limb * variation;

    return StarResult(surfaceColor, 1.0);
}

// ── Black Hole ───────────────────────────────────────────────────────────────

fn blackbodyColor(tempK: f32) -> vec3f {
    let t = tempK / 100.0;
    var c: vec3f;

    if (t <= 66.0) {
        c.x = 1.0;
    } else {
        c.x = 1.2929 * pow(t - 60.0, -0.1332);
    }

    if (t <= 66.0) {
        c.y = 0.3901 * log(t) - 0.6318;
    } else {
        c.y = 1.1299 * pow(t - 60.0, -0.0755);
    }

    if (t >= 66.0) {
        c.z = 1.0;
    } else if (t <= 19.0) {
        c.z = 0.0;
    } else {
        c.z = 0.5432 * log(t - 10.0) - 1.1963;
    }

    return clamp(c, vec3f(0.0), vec3f(1.0));
}

// Core black hole ray tracer using Schwarzschild geodesic integration
fn traceBlackHole(ro: vec3f, rd: vec3f) -> vec3f {
    let Rs = u.bhParams1.y * u.planetPos_radius.w * 0.5;  // uBhMass * uPlanetRadius

    var pos = ro - u.planetPos_radius.xyz;
    var vel = rd;

    let h = length(cross(pos, vel));

    let rInner = u.bhParams1.z * Rs;  // uBhAccretionInner
    let rOuter = u.bhParams1.w * Rs;  // uBhAccretionOuter

    var diskColor = vec3f(0.0);
    var diskAlpha = 0.0;

    var prevY = pos.y;

    let uBhRaySteps = i32(u.bhParams3.z);

    for (var i = 0; i < uBhRaySteps; i = i + 1) {
        let r = length(pos);

        if (r < Rs) {
            return diskColor;
        }

        if (r > 100.0 * Rs) {
            let bg = spaceColor(normalize(vel));
            return diskColor + (1.0 - diskAlpha) * bg;
        }

        let r2 = r * r;
        let r5 = r2 * r2 * r;
        let accel = -1.5 * h * h * Rs / r5 * pos;

        var dt = 0.3 * r / (1.0 + 2.0 * Rs / max(r - Rs, 0.01));
        dt = clamp(dt, 0.01 * Rs, 2.0 * Rs);

        let newPos = pos + vel * dt + 0.5 * accel * dt * dt;
        let newR = length(newPos);
        let newR5 = newR * newR * newR * newR * newR;
        let newAccel = -1.5 * h * h * Rs / newR5 * newPos;
        let newVel = vel + 0.5 * (accel + newAccel) * dt;

        if (prevY * newPos.y < 0.0) {
            let t_cross = abs(prevY) / max(abs(prevY) + abs(newPos.y), 1e-6);
            let crossPos = mix(pos, newPos, t_cross);
            let crossR = length(crossPos);

            if (crossR >= rInner && crossR <= rOuter) {
                let radialT = (crossR - rInner) / (rOuter - rInner);

                let temp = mix(u.bhParams2.w, u.bhParams3.x, radialT);  // uBhTempInner, uBhTempOuter
                let bbColor = blackbodyColor(temp);

                var noiseCoord = crossPos * 0.5 / Rs;
                var angle = atan2(crossPos.z, crossPos.x);
                angle += u.cameraPos_time.w * u.bhParams2.x * sqrt(Rs / max(crossR, Rs)) * 0.5;  // uTime * uBhDiskSpeed
                noiseCoord.x = crossR * cos(angle) * 0.5 / Rs;
                noiseCoord.z = crossR * sin(angle) * 0.5 / Rs;
                let turb_raw = noise(noiseCoord);
                let turb = mix(1.0, turb_raw, u.bhParams2.y);  // uBhDiskTurbulence

                let edgeFade = smoothstep(0.0, 0.15, radialT)
                             * smoothstep(1.0, 0.85, radialT);

                let orbitDir = normalize(cross(vec3f(0.0, 1.0, 0.0), normalize(crossPos)));
                let orbitalV = u.bhParams2.x * sqrt(Rs / (2.0 * max(crossR, Rs)));  // uBhDiskSpeed
                let doppler = max(1.0 + u.bhParams3.y * orbitalV * dot(normalize(vel), orbitDir) * 4.0, 0.1);  // uBhDopplerStrength

                var luminosity = (rInner / max(crossR, rInner));
                luminosity *= luminosity;

                let sampleColor = bbColor * u.bhDiskTintPad.xyz * turb * edgeFade
                                * luminosity * doppler * u.bhParams2.z;  // uBhDiskBrightness
                let sampleAlpha = edgeFade * turb * 0.8;

                diskColor += (1.0 - diskAlpha) * sampleAlpha * sampleColor;
                diskAlpha += (1.0 - diskAlpha) * sampleAlpha;
                diskAlpha = min(diskAlpha, 1.0);
            }
        }

        prevY = newPos.y;
        pos = newPos;
        vel = newVel;
    }

    let bg = spaceColor(normalize(vel));
    return diskColor + (1.0 - diskAlpha) * bg;
}

fn simpleReinhardToneMapping(color_in: vec3f) -> vec3f {
    let exposure = 1.5;
    var color = color_in * exposure / (1.0 + color_in / exposure);
    color = pow(color, vec3f(1.0 / 2.4));
    return color;
}

// ── Atmosphere ───────────────────────────────────────────────────────────────

fn atmosphereColor(ro: vec3f, rd: vec3f, spaceMask: f32) -> vec3f {
    let uPlanetRadius = u.planetPos_radius.w;
    let uPlanetPosition = u.planetPos_radius.xyz;
    let uCameraPosition = u.cameraPos_time.xyz;
    let uSunDirection = u.sunDirectionPad.xyz;
    let uAtmosphereDensity = u.cloudParams2.w;
    let uSunIntensity = u.cloudParams2.y;
    let uAtmosphereColorVal = u.atmosphereColorPad.xyz;

    let distCameraToPlanetOrigin = length(uPlanetPosition - uCameraPosition);
    let distCameraToPlanetEdge = sqrt(distCameraToPlanetOrigin * distCameraToPlanetOrigin - uPlanetRadius * uPlanetRadius);

    let planetMask = 1.0 - spaceMask;

    let coordFromCenter = (ro + rd * distCameraToPlanetEdge) - uPlanetPosition;
    let distFromEdge = abs(length(coordFromCenter) - uPlanetRadius);
    let planetEdge = max(uPlanetRadius - distFromEdge, 0.0) / uPlanetRadius;
    let atmosphereMaskRaw = pow(clamp(remap(dot(uSunDirection, coordFromCenter), -uPlanetRadius, uPlanetRadius / 2.0, 0.0, 1.0), 0.0, 1.0), 5.0);
    let atmosphereMask = atmosphereMaskRaw * uAtmosphereDensity * uPlanetRadius * uSunIntensity;

    var atmosphere = vec3f(pow(planetEdge, 120.0)) * 0.5;
    atmosphere += pow(planetEdge, 50.0) * 0.3 * (1.5 - planetMask);
    atmosphere += pow(planetEdge, 15.0) * 0.03;
    atmosphere += pow(planetEdge, 5.0) * 0.04 * planetMask;

    return atmosphere * uAtmosphereColorVal * atmosphereMask;
}

// ── Volumetric clouds ────────────────────────────────────────────────────────

fn marchCloudSegment(ro: vec3f, rd: vec3f, tStart: f32, tEnd: f32, numSteps: i32,
                     scattered_in: vec3f, transmittance_in: f32) -> CloudMarchResult {
    var scattered = scattered_in;
    var transmittance = transmittance_in;

    if (tStart >= tEnd || transmittance < 0.01) {
        return CloudMarchResult(scattered, transmittance);
    }
    let stepSize = (tEnd - tStart) / f32(numSteps);

    let PLANET_ROTATION = getPlanetRotation();
    let uCloudsDensity = u.cloudParams1.x;
    let uCloudsScale = u.cloudParams1.y;
    let uCloudsSpeed = u.cloudParams1.z;
    let uCloudAltitude = u.cloudParams1.w;
    let uCloudThickness = u.cloudParams2.x;
    let uSunIntensity = u.cloudParams2.y;
    let uSunDirection = u.sunDirectionPad.xyz;
    let uSunColor = u.sunColorPad.xyz;
    let uCloudColor = u.cloudColorPad.xyz;
    let uAtmosphereColorVal = u.atmosphereColorPad.xyz;
    let uPlanetPosition = u.planetPos_radius.xyz;
    let uPlanetRadius = u.planetPos_radius.w;
    let uTime = u.cameraPos_time.w;
    let uQuality = u.noiseParams.y;

    // Coverage threshold: higher coverage = lower threshold = more clouds
    let coverageThreshold = 0.7 - uCloudsDensity * 0.5;

    for (var i = 0; i < numSteps; i = i + 1) {
        if (transmittance < 0.01) { break; }

        let t = tStart + (f32(i) + 0.5) * stepSize;
        let pos = ro + rd * t;
        let localPos = pos - uPlanetPosition;

        let alt = length(localPos) - uPlanetRadius;
        let heightFrac = clamp((alt - uCloudAltitude) / uCloudThickness, 0.0, 1.0);

        let rotatedPos = PLANET_ROTATION * localPos + uPlanetPosition;
        let cloudCoord = (rotatedPos + vec3f(uTime * 0.005 * uCloudsSpeed)) * uCloudsScale * 0.5;
        let rawDensity = cloudNoise(cloudCoord);

        // Height-based shape: cumulus clouds are flat-bottomed, rounded on top
        let bottomFalloff = smoothstep(0.0, 0.15, heightFrac);
        let topFalloff = 1.0 - pow(heightFrac, 2.0);
        let heightShape = bottomFalloff * topFalloff;

        // Apply coverage threshold with soft edge
        let softEdge = 0.15 + 0.1 * (1.0 - uCloudsDensity);
        var density = smoothstep(coverageThreshold, coverageThreshold + softEdge, rawDensity);
        density *= heightShape;

        if (density < 0.001) { continue; }

        // Lighting
        let normal = normalize(localPos);
        let NdotL = clamp(dot(normal, uSunDirection), 0.0, 1.0);

        // Henyey-Greenstein phase function
        let cosTheta = dot(rd, uSunDirection);
        let g = 0.7;
        let hg = (1.0 - g * g) / pow(1.0 + g * g - 2.0 * g * cosTheta, 1.5) / (4.0 * PI);
        let phase = mix(0.25, hg * 3.0, 0.8);

        // Multi-step shadow sampling
        var shadowDensity = 0.0;
        if (uQuality >= 1.0) {
            for (var s = 1; s <= 2; s = s + 1) {
                let sp = pos + uSunDirection * uCloudThickness * f32(s) * 0.4;
                let sLocal = sp - uPlanetPosition;
                let sAlt = length(sLocal) - uPlanetRadius;
                let sH = (sAlt - uCloudAltitude) / uCloudThickness;
                if (sH >= 0.0 && sH <= 1.0) {
                    let sRot = PLANET_ROTATION * sLocal + uPlanetPosition;
                    let sCoord = (sRot + vec3f(uTime * 0.005 * uCloudsSpeed)) * uCloudsScale * 0.5;
                    var sd = cloudNoiseCheap(sCoord);
                    let sBottom = smoothstep(0.0, 0.15, sH);
                    let sTop = 1.0 - pow(sH, 2.0);
                    sd = smoothstep(coverageThreshold, coverageThreshold + softEdge, sd) * sBottom * sTop;
                    shadowDensity += sd * 0.5;
                }
            }
        }
        let sunTransmittance = exp(-shadowDensity * 4.0);

        // Beer-Lambert absorption
        let absorption = density * stepSize * 12.0;

        // Lighting: direct sun + ambient + powder effect
        let sunLight = uSunColor * uSunIntensity * NdotL * phase * sunTransmittance;
        let ambient = uAtmosphereColorVal * 0.15;

        let powder = 1.0 - exp(-density * 4.0);
        let powderLight = uSunColor * powder * 0.2 * sunTransmittance;

        var luminance = uCloudColor * (sunLight + ambient + powderLight);

        // Silver lining
        let silverLining = pow(1.0 - density, 2.0) * max(cosTheta, 0.0);
        luminance += uSunColor * silverLining * 0.5 * uSunIntensity * sunTransmittance;

        // Accumulate with energy-conserving blend
        let alpha = 1.0 - exp(-absorption);
        scattered += luminance * transmittance * alpha;
        transmittance *= (1.0 - alpha);
    }

    return CloudMarchResult(scattered, transmittance);
}

fn volumetricClouds(ro: vec3f, rd: vec3f, surfaceDist: f32) -> vec4f {
    if (u.cloudParams1.x <= 0.0) { return vec4f(0.0); }  // uCloudsDensity

    let uPlanetPosition = u.planetPos_radius.xyz;
    let uPlanetRadius = u.planetPos_radius.w;
    let uCloudAltitude = u.cloudParams1.w;
    let uCloudThickness = u.cloudParams2.x;
    let uQuality = u.noiseParams.y;

    let cloudInnerR = uPlanetRadius + uCloudAltitude;
    let cloudOuterR = cloudInnerR + uCloudThickness;

    let tOuter = sphIntersect2(ro, rd, Sphere(uPlanetPosition, cloudOuterR));
    if (tOuter.x < 0.0) { return vec4f(0.0); }

    let tInner = sphIntersect2(ro, rd, Sphere(uPlanetPosition, cloudInnerR));

    let frontStart = max(tOuter.x, 0.0);
    var frontEnd: f32;
    if (tInner.x > 0.0) {
        frontEnd = tInner.x;
    } else {
        frontEnd = tOuter.y;
    }
    frontEnd = min(frontEnd, surfaceDist);

    var backStart = -1.0;
    var backEnd = -1.0;
    let hitPlanet = (surfaceDist < 1e9);

    if (!hitPlanet && tInner.y > 0.0 && tInner.y < tOuter.y) {
        backStart = tInner.y;
        backEnd = tOuter.y;
    }

    let numSteps = 8 + i32(uQuality) * 12;
    var scattered = vec3f(0.0);
    var transmittance = 1.0;

    let result1 = marchCloudSegment(ro, rd, frontStart, frontEnd, numSteps, scattered, transmittance);
    scattered = result1.scattered;
    transmittance = result1.transmittance;

    if (backStart > 0.0) {
        let result2 = marchCloudSegment(ro, rd, backStart, backEnd, numSteps / 2, scattered, transmittance);
        scattered = result2.scattered;
        transmittance = result2.transmittance;
    }

    return vec4f(scattered, 1.0 - transmittance);
}

// ── Surface intersection ─────────────────────────────────────────────────────

fn intersectPlanet(ro: vec3f, rd: vec3f) -> Hit {
    let PLANET_ROTATION = getPlanetRotation();
    let uPlanetPosition = u.planetPos_radius.xyz;
    let uPlanetRadius = u.planetPos_radius.w;
    let uSandLevel = u.biomeLevels.x;
    let uWaterLevel = u.terrainFeatures.w;
    let uTreeLevel = u.biomeLevels.y;
    let uRockLevel = u.biomeLevels.z;
    let uIceLevel = u.biomeLevels.w;
    let uTransition = u.transitionPad.x;
    let uBandingStrength = u.bandingPolar.x;
    let uBandingFrequency = u.bandingPolar.y;
    let uPolarCapSize = u.bandingPolar.z;
    let uTime = u.cameraPos_time.w;

    let len = sphIntersect(ro, rd, getPlanet());
    if (len < 0.0) { return makeMiss(); }
    let position = ro + len * rd;
    let rotatedCoord = PLANET_ROTATION * (position - uPlanetPosition) + uPlanetPosition;
    let rawNoise = planetNoise(rotatedCoord);
    let normal = planetNormal(position);

    // Coastline threshold
    let coastline = (uSandLevel + uWaterLevel) / 5.0;
    let landMask = smoothstep(coastline - 0.001, coastline + 0.001, rawNoise);

    // Water: smooth depth gradient
    let waterDepth = clamp(rawNoise / max(coastline, 0.001), 0.0, 1.0);
    let waterColor = mix(u.waterColorDeepPad.xyz, u.waterColorSurfPad.xyz, waterDepth);

    // Land: altitude-based biome coloring (relative to water level)
    let relativeHeight = rawNoise - coastline;
    let altitude = 5.0 * max(relativeHeight, 0.0);
    var landColor = u.sandColorPad.xyz;
    landColor = mix(landColor, u.treeColorPad.xyz, smoothstep(uTreeLevel, uTreeLevel + uTransition, altitude));
    landColor = mix(landColor, u.rockColorPad.xyz, smoothstep(uRockLevel, uRockLevel + uTransition, altitude));
    landColor = mix(landColor, u.iceColorPad.xyz, smoothstep(uIceLevel, uIceLevel + uTransition, altitude));

    var color = mix(waterColor, landColor, landMask);

    // Latitude effects
    let localDir = normalize(rotatedCoord - uPlanetPosition);
    let latitude = abs(localDir.y);

    if (uBandingStrength > 0.0) {
        let lat = localDir.y;

        // Warp latitude with flowing turbulence
        let nc = rotatedCoord * 2.0;
        let warp1 = noise(nc + vec3f(uTime * 0.02, 0.0, 0.0)) - 0.5;
        let warp2 = noise(nc * 1.5 + vec3f(0.0, uTime * 0.015, 17.0)) - 0.5;
        let warpedLat = lat + (warp1 * 0.06 + warp2 * 0.04) * uBandingStrength;

        // Multiple band frequencies for varied width bands
        let b1 = sin(warpedLat * uBandingFrequency) * 0.5 + 0.5;
        let b2 = sin(warpedLat * uBandingFrequency * 0.4 + 0.5) * 0.5 + 0.5;
        let b3 = sin(warpedLat * uBandingFrequency * 1.7 - 0.3) * 0.5 + 0.5;

        var band = b1 * 0.5 + b2 * 0.3 + b3 * 0.2;

        // Flowing streaks within bands
        let streakCoord = localDir * 8.0 + vec3f(0.0, lat * uBandingFrequency, uTime * 0.05);
        let streak = noise(streakCoord);
        band += (streak - 0.5) * 0.15;

        // Small turbulent eddies
        let eddy = noise(rotatedCoord * 12.0 + vec3f(uTime * 0.03));
        var eddyMask = noise(rotatedCoord * 3.0);
        eddyMask = smoothstep(0.55, 0.7, eddyMask);
        band += (eddy - 0.5) * 0.2 * eddyMask;

        band = clamp(band, 0.0, 1.0);

        // Three-tone color palette
        let darkBand = u.treeColorPad.xyz;
        let midBand = u.rockColorPad.xyz;
        let lightBand = u.sandColorPad.xyz;

        var bandColor: vec3f;
        if (band < 0.5) {
            bandColor = mix(darkBand, midBand, band * 2.0);
        } else {
            bandColor = mix(midBand, lightBand, (band - 0.5) * 2.0);
        }

        color = mix(color, bandColor, uBandingStrength);
    }

    if (uPolarCapSize > 0.0) {
        let polarEdge = 1.0 - uPolarCapSize;
        let polarMask = smoothstep(polarEdge, polarEdge + 0.1, latitude);
        color = mix(color, u.iceColorPad.xyz, polarMask);
    }

    let specular = 1.0 - landMask;
    return Hit(len, normal, Material(color, 1.0, specular));
}

// ── Radiance ─────────────────────────────────────────────────────────────────

fn radiance(ro: vec3f, rd: vec3f) -> vec3f {
    let uIsEmissive = (u.extraParams.z > 0.5);
    let uIsBlackHole = (u.bhParams1.x > 0.5);
    let uSunDirection = u.sunDirectionPad.xyz;
    let uSunColor = u.sunColorPad.xyz;
    let uSunIntensity = u.cloudParams2.y;
    let uAmbientLight = u.cloudParams2.z;

    // Emissive body (star) rendering
    if (uIsEmissive) {
        let starResult = renderStar(ro, rd);
        if (starResult.alpha > 0.0) {
            return starResult.color;
        }
        return spaceColor(rd);
    }

    if (uIsBlackHole) { return traceBlackHole(ro, rd); }

    var color = vec3f(0.0);
    var spaceMask = 1.0;
    let hit = intersectPlanet(ro, rd);

    if (hit.len < INFINITY_DIST) {
        spaceMask = 0.0;
        let hitPosition = ro + hit.len * rd;

        // Diffuse
        let directLightIntensity = pow(clamp(dot(hit.normal, uSunDirection), 0.0, 1.0), 2.0) * uSunIntensity;
        let diffuseLight = directLightIntensity * uSunColor;
        let diffuseColor = hit.material.color * (uAmbientLight + diffuseLight);

        // Phong specular
        let reflected = normalize(reflect(-uSunDirection, hit.normal));
        let phongValue = pow(max(0.0, dot(rd, reflected)), 10.0) * 0.2 * uSunIntensity;
        let specularColor = hit.material.specular * vec3f(phongValue);

        color = diffuseColor + specularColor;
    } else {
        color = spaceColor(rd);
    }

    // Volumetric clouds
    let clouds = volumetricClouds(ro, rd, hit.len);
    color = color * (1.0 - clouds.a) + clouds.rgb;

    return color + atmosphereColor(ro, rd, spaceMask);
}

// ── Main (Fragment shader) ───────────────────────────────────────────────────

@fragment
fn fs_main(@location(0) uv: vec2f) -> @location(0) vec4f {
    var screenUV = uv;
    screenUV.x *= u.resolution_rot.x / u.resolution_rot.y;  // uResolution.x / uResolution.y

    let ro = u.cameraPos_time.xyz;  // uCameraPosition
    var rd = normalize(vec3f(screenUV, -1.0));
    rd = (u.invView * vec4f(rd, 0.0)).xyz;  // uInvView

    var color = radiance(ro, rd);

    color = simpleReinhardToneMapping(color);

    return vec4f(color, 1.0);
}
