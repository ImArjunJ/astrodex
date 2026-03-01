#version 450 core

// Procedural Planet Renderer — based on Julien Sulpis "Procedural Blue Planet"
// Extended with ridged noise, craters, continents, and volumetric clouds

in vec2 uv;
out vec4 fragColor;

// Global uniforms
uniform float uTime;
uniform float uRotationOffset;
uniform float uRotationSpeed;
uniform vec2 uResolution;
uniform sampler3D uNoiseTexture;

// Controllable uniforms
uniform float uQuality;
uniform vec3 uPlanetPosition;
uniform float uPlanetRadius;
uniform float uNoiseStrength;
uniform float uCloudsDensity;
uniform float uCloudsScale;
uniform float uCloudsSpeed;
uniform float uCloudAltitude;
uniform float uCloudThickness;
uniform float uTerrainScale;
uniform vec3 uAtmosphereColor;
uniform float uAtmosphereDensity;
uniform float uSunIntensity;
uniform float uAmbientLight;
uniform vec3 uSunDirection;

// Color/level uniforms
uniform vec3 uWaterColorDeep;
uniform vec3 uWaterColorSurface;
uniform vec3 uSandColor;
uniform vec3 uTreeColor;
uniform vec3 uRockColor;
uniform vec3 uIceColor;
uniform float uSandLevel;
uniform float uTreeLevel;
uniform float uRockLevel;
uniform float uIceLevel;
uniform float uTransition;

// Camera
uniform vec3 uCameraPosition;
uniform mat4 uInvView;
uniform mat4 uInvProjection;
uniform mat4 uViewProjection;

// Extra color uniforms
uniform vec3 uCloudColor;
uniform vec3 uSunColor;
uniform vec3 uDeepSpaceColor;

// Emissive body flag (for stars)
uniform int uIsEmissive;

// Noise type (0=Standard, 1=Ridged, 2=Billowy, 3=Warped, 4=Voronoi, 5=Swiss, 6=Hybrid)
uniform int uNoiseType;

// Terrain diversity
uniform int uFbmOctaves;
uniform float uFbmPersistence;
uniform float uFbmLacunarity;
uniform float uFbmExponentiation;
uniform float uDomainWarpStrength;
uniform float uRidgedStrength;
uniform float uCraterStrength;
uniform float uContinentScale;
uniform float uContinentBlend;
uniform float uWaterLevel;
uniform float uPolarCapSize;
uniform float uBandingStrength;
uniform float uBandingFrequency;

// Constants
#define PLANET_ROTATION rotateY(uTime * uRotationSpeed + uRotationOffset)
#define EPSILON 1e-3
#define INFINITY 1e10
#define PI 3.14159265

// ── Structs ──────────────────────────────────────────────────────────────────

struct Material { vec3 color; float diffuse; float specular; };
struct Hit { float len; vec3 normal; Material material; };
struct Sphere { vec3 position; float radius; };

Hit miss = Hit(INFINITY, vec3(0.), Material(vec3(0.), -1., -1.));

Sphere getPlanet() { return Sphere(uPlanetPosition, uPlanetRadius); }

// ── Utility ──────────────────────────────────────────────────────────────────

float inverseLerp(float v, float a, float b) { return (v - a) / (b - a); }

float remap(float v, float inMin, float inMax, float outMin, float outMax) {
    return mix(outMin, outMax, inverseLerp(v, inMin, inMax));
}

float noise(vec3 p) { return texture(uNoiseTexture, p * .05).r; }

float sphIntersect(in vec3 ro, in vec3 rd, in Sphere s) {
    vec3 oc = ro - s.position;
    float b = dot(oc, rd);
    float c = dot(oc, oc) - s.radius * s.radius;
    float h = b * b - c;
    if (h < 0.0) return -1.;
    return -b - sqrt(h);
}

// Returns (near, far) intersection distances; (-1,-1) on miss
vec2 sphIntersect2(vec3 ro, vec3 rd, Sphere s) {
    vec3 oc = ro - s.position;
    float b = dot(oc, rd);
    float c = dot(oc, oc) - s.radius * s.radius;
    float h = b * b - c;
    if (h < 0.0) return vec2(-1.0);
    float sq = sqrt(h);
    return vec2(-b - sq, -b + sq);
}

mat3 rotateY(float angle) {
    float c = cos(angle), s = sin(angle);
    return mat3(vec3(c, 0, s), vec3(0, 1, 0), vec3(-s, 0, c));
}

// ── Noise functions ──────────────────────────────────────────────────────────

float fbm(vec3 p, int octaves, float persistence, float lacunarity, float exponentiation) {
    float amplitude = 0.5;
    float frequency = 3.0;
    float total = 0.0;
    float normalization = 0.0;
    int qualityDegradation = 2 - int(floor(uQuality));
    int oct = max(octaves - qualityDegradation, 1);

    for (int i = 0; i < oct; ++i) {
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
float ridgedFBM(vec3 p, int octaves, float persistence, float lacunarity) {
    float amplitude = 0.5;
    float frequency = 3.0;
    float total = 0.0;
    float normalization = 0.0;
    float weight = 1.0;
    int qualityDegradation = 2 - int(floor(uQuality));
    int oct = max(octaves - qualityDegradation, 1);

    for (int i = 0; i < oct; ++i) {
        float n = noise(p * frequency);
        n = 1.0 - abs(n * 2.0 - 1.0); // ridge: fold at 0.5
        n = n * n;                       // sharpen ridges
        n *= weight;
        weight = clamp(n, 0.0, 1.0);    // successive octaves weighted by previous
        total += n * amplitude;
        normalization += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }

    return total / normalization;
}

// Voronoi/cellular noise for crater placement
float craterNoise(vec3 p) {
    vec3 cell = floor(p);
    vec3 frac = fract(p);
    float minDist = 1e10;

    for (int x = -1; x <= 1; x++)
    for (int y = -1; y <= 1; y++)
    for (int z = -1; z <= 1; z++) {
        vec3 neighbor = vec3(x, y, z);
        vec3 cellId = cell + neighbor;
        // Use noise texture as hash for random cell center offset
        vec3 offset = vec3(
            noise(cellId * 0.37),
            noise(cellId * 0.37 + vec3(17.3, 0.0, 0.0)),
            noise(cellId * 0.37 + vec3(0.0, 43.7, 0.0)));
        float dist = length(frac - neighbor - offset);
        minDist = min(minDist, dist);
    }

    // Crater profile: bowl depression + raised rim
    float bowl = smoothstep(0.35, 0.0, minDist) * -1.0;
    float rim = smoothstep(0.25, 0.38, minDist) * smoothstep(0.55, 0.38, minDist) * 0.4;
    return bowl + rim;
}

// ── Improved Cloud System ────────────────────────────────────────────────────

// Worley/cellular noise for puffy cloud base shapes
float worleyNoise(vec3 p) {
    vec3 cell = floor(p);
    vec3 frac = fract(p);
    float minDist = 1.0;

    for (int x = -1; x <= 1; x++)
    for (int y = -1; y <= 1; y++)
    for (int z = -1; z <= 1; z++) {
        vec3 neighbor = vec3(x, y, z);
        vec3 cellId = cell + neighbor;
        // Hash for random point in cell
        vec3 randomOffset = vec3(
            noise(cellId * 0.31),
            noise(cellId * 0.31 + vec3(127.1, 0.0, 0.0)),
            noise(cellId * 0.31 + vec3(0.0, 269.5, 0.0))
        );
        vec3 point = neighbor + randomOffset;
        float dist = length(frac - point);
        minDist = min(minDist, dist);
    }
    return 1.0 - minDist;  // Invert: high value = inside cloud cell
}

// Smooth low-frequency noise for large cloud shapes
float cloudBaseFBM(vec3 p) {
    float total = 0.0;
    float amplitude = 0.6;
    float frequency = 0.4;  // MUCH lower base frequency for large shapes
    float normalization = 0.0;
    int octaves = 2 + int(floor(uQuality));

    for (int i = 0; i < octaves; ++i) {
        total += noise(p * frequency) * amplitude;
        normalization += amplitude;
        amplitude *= 0.45;  // Faster falloff for smoother shapes
        frequency *= 2.0;
    }
    return total / normalization;
}

// High-frequency detail for cloud edges
float cloudDetailFBM(vec3 p) {
    float total = 0.0;
    float amplitude = 0.5;
    float frequency = 2.0;
    float normalization = 0.0;

    for (int i = 0; i < 3; ++i) {
        total += noise(p * frequency) * amplitude;
        normalization += amplitude;
        amplitude *= 0.5;
        frequency *= 2.2;
    }
    return total / normalization;
}

// Main cloud density function - creates billowy cumulus shapes
float cloudNoise(vec3 p) {
    // Scale down for larger cloud formations
    vec3 cloudP = p * 0.3;

    // 1. Base shape from Worley noise (puffy cellular structure)
    float worley = worleyNoise(cloudP * 1.5);

    // 2. Large-scale variation from low-freq FBM
    float baseShape = cloudBaseFBM(cloudP);

    // 3. Domain warping for billowing effect
    vec3 warpOffset = vec3(
        cloudBaseFBM(cloudP + vec3(0.0)),
        cloudBaseFBM(cloudP + vec3(43.2, 17.8, 0.0)),
        cloudBaseFBM(cloudP + vec3(0.0, 93.1, 27.3))
    );
    float warped = cloudBaseFBM(cloudP + warpOffset * 0.8);

    // 4. Combine: Worley for structure, FBM for shape, warping for billows
    float density = worley * 0.4 + baseShape * 0.35 + warped * 0.25;

    // 5. Add fine detail at edges (not throughout)
    float detail = cloudDetailFBM(p * 0.8);
    density += detail * 0.15 * smoothstep(0.3, 0.6, density);

    return density;
}

// Cheap version for shadow rays (skip Worley and warping)
float cloudNoiseCheap(vec3 p) {
    vec3 cloudP = p * 0.3;
    return cloudBaseFBM(cloudP);
}

// ── Additional Noise Types ──────────────────────────────────────────────────

// Billowy noise - soft, rounded terrain
float billowyFBM(vec3 p, int octaves, float persistence, float lacunarity) {
    float amplitude = 0.5;
    float frequency = 3.0;
    float total = 0.0;
    float normalization = 0.0;
    int qualityDegradation = 2 - int(floor(uQuality));
    int oct = max(octaves - qualityDegradation, 1);

    for (int i = 0; i < oct; ++i) {
        float n = noise(p * frequency);
        n = n * n;  // Square for soft, rounded shapes
        total += n * amplitude;
        normalization += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }

    return total / normalization;
}

// Swiss noise - eroded, holey terrain
float swissFBM(vec3 p, int octaves, float persistence, float lacunarity) {
    float amplitude = 0.5;
    float frequency = 3.0;
    float total = 0.0;
    float normalization = 0.0;
    float warp = 0.0;
    int qualityDegradation = 2 - int(floor(uQuality));
    int oct = max(octaves - qualityDegradation, 1);

    for (int i = 0; i < oct; ++i) {
        float n = noise((p + warp * 0.15) * frequency);
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
float voronoiTerrain(vec3 p) {
    vec3 cell = floor(p);
    vec3 frac = fract(p);
    float minDist = 1e10;
    float secondDist = 1e10;

    for (int x = -1; x <= 1; x++)
    for (int y = -1; y <= 1; y++)
    for (int z = -1; z <= 1; z++) {
        vec3 neighbor = vec3(x, y, z);
        vec3 cellId = cell + neighbor;
        vec3 offset = vec3(
            noise(cellId * 0.37),
            noise(cellId * 0.37 + vec3(17.3, 0.0, 0.0)),
            noise(cellId * 0.37 + vec3(0.0, 43.7, 0.0)));
        float dist = length(frac - neighbor - offset);
        if (dist < minDist) {
            secondDist = minDist;
            minDist = dist;
        } else if (dist < secondDist) {
            secondDist = dist;
        }
    }

    // Edge-based terrain (interesting ridges between cells)
    return secondDist - minDist;
}

// Hybrid noise - combines multiple types
float hybridFBM(vec3 p, int octaves, float persistence, float lacunarity, float exponentiation) {
    float standard = fbm(p, octaves, persistence, lacunarity, exponentiation);
    float ridged = ridgedFBM(p * 0.8, octaves, persistence, lacunarity);
    float billowy = billowyFBM(p * 1.2, octaves, persistence, lacunarity);

    // Blend based on position
    float blend = noise(p * 0.5);
    return mix(mix(standard, ridged, smoothstep(0.3, 0.7, blend)),
               billowy, smoothstep(0.6, 0.9, blend));
}

// ── Terrain ──────────────────────────────────────────────────────────────────

float planetNoise(vec3 p) {
    vec3 tp = p * uTerrainScale;

    // Apply domain warping for Warped type or if strength > 0
    bool applyWarp = (uNoiseType == 3) || (uDomainWarpStrength > 0.0);
    float warpStrength = (uNoiseType == 3) ? max(0.5, uDomainWarpStrength) : uDomainWarpStrength;

    if (applyWarp && warpStrength > 0.0) {
        int warpOct = max(uFbmOctaves / 2, 2);
        vec3 warpOffset = vec3(
            fbm(tp, warpOct, uFbmPersistence, uFbmLacunarity, uFbmExponentiation),
            fbm(tp + vec3(43.235, 23.112, 0.0), warpOct, uFbmPersistence, uFbmLacunarity, uFbmExponentiation),
            fbm(tp + vec3(0.0, 71.823, 37.156), warpOct, uFbmPersistence, uFbmLacunarity, uFbmExponentiation));
        tp = tp + uDomainWarpStrength * warpOffset;
    }

    // Select noise type based on uniform
    float baseFBM;
    if (uNoiseType == 0) {
        // Standard FBM
        baseFBM = fbm(tp, uFbmOctaves, uFbmPersistence, uFbmLacunarity, uFbmExponentiation);
    } else if (uNoiseType == 1) {
        // Ridged
        baseFBM = ridgedFBM(tp, uFbmOctaves, uFbmPersistence, uFbmLacunarity);
    } else if (uNoiseType == 2) {
        // Billowy
        baseFBM = billowyFBM(tp, uFbmOctaves, uFbmPersistence, uFbmLacunarity);
        baseFBM = pow(baseFBM, uFbmExponentiation * 0.5);
    } else if (uNoiseType == 3) {
        // Warped (domain warp already applied above)
        baseFBM = fbm(tp, uFbmOctaves, uFbmPersistence, uFbmLacunarity, uFbmExponentiation);
    } else if (uNoiseType == 4) {
        // Voronoi
        baseFBM = voronoiTerrain(tp * 2.0);
        baseFBM = pow(baseFBM, uFbmExponentiation * 0.3);
    } else if (uNoiseType == 5) {
        // Swiss
        baseFBM = swissFBM(tp, uFbmOctaves, uFbmPersistence, uFbmLacunarity);
        baseFBM = pow(baseFBM, uFbmExponentiation * 0.5);
    } else if (uNoiseType == 6) {
        // Hybrid
        baseFBM = hybridFBM(tp, uFbmOctaves, uFbmPersistence, uFbmLacunarity, uFbmExponentiation);
    } else {
        baseFBM = fbm(tp, uFbmOctaves, uFbmPersistence, uFbmLacunarity, uFbmExponentiation);
    }

    // Additional ridged mountains blend (for non-ridged types)
    if (uNoiseType != 1 && uRidgedStrength > 0.0) {
        float ridged = ridgedFBM(tp, uFbmOctaves, uFbmPersistence, uFbmLacunarity);
        baseFBM = mix(baseFBM, ridged, uRidgedStrength);
    }

    float f = baseFBM * uNoiseStrength;

    // Craters (single layer)
    if (uCraterStrength > 0.0) {
        float craters = craterNoise(tp * 3.0);
        f += craters * uCraterStrength * uNoiseStrength * 0.5;
    }

    // Continental shaping — large-scale land/ocean mask
    if (uContinentScale > 0.0) {
        // Smooth continent base shape (low frequency, no holes)
        float continent = noise(p * uContinentScale * 0.15);
        // Add slight variation to continent edges
        continent += noise(p * uContinentScale * 0.4) * uContinentBlend;

        // Continent threshold based on water level (higher water = less land)
        float landThreshold = 0.35 + uWaterLevel * 2.0;
        float continentMask = smoothstep(landThreshold, landThreshold + uContinentBlend, continent);

        // On continents: floor terrain at coastline level, preventing inland seas
        float coastline = (uSandLevel + uWaterLevel) / 5.0;
        float landBase = coastline + 0.001;  // Just above water

        // Blend: ocean areas keep original height, land areas get floored then add detail
        float oceanHeight = f * 0.15;  // Reduce terrain in ocean
        float landHeight = landBase + f * continentMask;  // Land base + terrain detail

        f = mix(oceanHeight, landHeight, continentMask);
    }

    return mix(
        f / 3. + uNoiseStrength / 50.,
        f,
        smoothstep(uSandLevel, uSandLevel + uTransition / 2., f * 5.)
    );
}

float planetDist(in vec3 ro, in vec3 rd) {
    float smoothSphereDist = sphIntersect(ro, rd, getPlanet());
    vec3 intersection = ro + smoothSphereDist * rd;
    vec3 intersectionWithRotation = PLANET_ROTATION * (intersection - uPlanetPosition) + uPlanetPosition;
    float n = planetNoise(intersectionWithRotation);
    float coastline = (uSandLevel + uWaterLevel) / 5.0;
    float displacement = max(n, coastline);
    return sphIntersect(ro, rd, Sphere(uPlanetPosition, uPlanetRadius + displacement));
}

vec3 planetNormal(vec3 p) {
    vec3 rd = uPlanetPosition - p;
    float dist = planetDist(p, rd);
    vec2 e = vec2(max(.01, .03 * smoothstep(1300., 300., uResolution.x)), 0);
    vec3 normal = dist - vec3(planetDist(p - e.xyy, rd), planetDist(p - e.yxy, rd), planetDist(p + e.yyx, rd));
    return normalize(normal);
}

// ── Stars & space ────────────────────────────────────────────────────────────

vec3 stars(in vec3 p) {
    vec3 c = vec3(0.);
    float res = uResolution.x * uQuality * 0.8;
    for (float i = 0.; i < 3.; i++) {
        vec3 q = fract(p * (.15 * res)) - 0.5;
        vec3 id = floor(p * (.15 * res));
        vec2 rn = vec2(noise(id / 2.), noise(id.zyx * 2.)) * .03;
        float c2 = 1. - smoothstep(0., .6, length(q));
        c2 *= step(rn.x, .003 + i * 0.0005);
        c += c2 * (mix(vec3(1.0, 0.49, 0.1), vec3(0.75, 0.9, 1.), rn.y) * 0.25 + 1.2);
        p *= 1.8;
    }
    return c * c;
}

vec3 spaceColor(vec3 direction) {
    mat3 backgroundRotation = rotateY(uTime * uRotationSpeed / 4.);
    vec3 backgroundCoord = direction * backgroundRotation;
    float spaceNoise = fbm(backgroundCoord * 3., 4, .5, 2., 6.);
    return stars(backgroundCoord) + mix(uDeepSpaceColor, uAtmosphereColor / 12., spaceNoise);
}

vec3 simpleReinhardToneMapping(vec3 color) {
    float exposure = 1.5;
    color *= exposure / (1. + color / exposure);
    color = pow(color, vec3(1. / 2.4));
    return color;
}

// ── Atmosphere ───────────────────────────────────────────────────────────────

vec3 atmosphereColor(vec3 ro, vec3 rd, float spaceMask) {
    float distCameraToPlanetOrigin = length(uPlanetPosition - uCameraPosition);
    float distCameraToPlanetEdge = sqrt(distCameraToPlanetOrigin * distCameraToPlanetOrigin - uPlanetRadius * uPlanetRadius);

    float planetMask = 1.0 - spaceMask;

    vec3 coordFromCenter = (ro + rd * distCameraToPlanetEdge) - uPlanetPosition;
    float distFromEdge = abs(length(coordFromCenter) - uPlanetRadius);
    float planetEdge = max(uPlanetRadius - distFromEdge, 0.) / uPlanetRadius;
    float atmosphereMask = pow(clamp(remap(dot(uSunDirection, coordFromCenter), -uPlanetRadius, uPlanetRadius / 2., 0., 1.), 0., 1.), 5.);
    atmosphereMask *= uAtmosphereDensity * uPlanetRadius * uSunIntensity;

    vec3 atmosphere = vec3(pow(planetEdge, 120.)) * .5;
    atmosphere += pow(planetEdge, 50.) * .3 * (1.5 - planetMask);
    atmosphere += pow(planetEdge, 15.) * .03;
    atmosphere += pow(planetEdge, 5.) * .04 * planetMask;

    return atmosphere * uAtmosphereColor * atmosphereMask;
}

// ── Star/Emissive Body Rendering ─────────────────────────────────────────────

// Animated stellar surface noise - creates granulation/convection patterns
float stellarNoise(vec3 p, float time) {
    // Multiple scales of convection cells
    float n1 = noise(p * 3.0 + vec3(time * 0.1));
    float n2 = noise(p * 6.0 + vec3(0.0, time * 0.15, 0.0));
    float n3 = noise(p * 12.0 + vec3(time * 0.2, 0.0, time * 0.1));

    // Combine for granulation pattern
    float granulation = n1 * 0.5 + n2 * 0.3 + n3 * 0.2;

    // Add slower large-scale convection
    float largeCells = noise(p * 1.5 + vec3(time * 0.03));

    return mix(granulation, largeCells, 0.3);
}

// Render an emissive star - simple glowing sphere
vec3 renderStar(vec3 ro, vec3 rd, out float alpha, out float depth) {
    Sphere star = getPlanet();
    float t = sphIntersect(ro, rd, star);

    alpha = 0.0;
    depth = 1.0;

    if (t < 0.0) {
        return vec3(0.0);  // Miss star
    }

    // Hit the star surface
    vec3 hitPos = ro + rd * t;
    vec3 localPos = hitPos - star.position;
    vec3 normal = normalize(localPos);

    // Limb darkening - edges dimmer than center
    float viewAngle = abs(dot(normal, -rd));
    float limb = 0.4 + 0.6 * pow(viewAngle, 0.3);

    // Simple animated surface variation using noise
    mat3 rot = rotateY(uTime * 0.1);
    vec3 surfaceCoord = rot * normal * 5.0;
    float variation = noise(surfaceCoord + vec3(uTime * 0.05)) * 0.2 + 0.9;

    // Base star color - bright yellow/orange
    vec3 baseColor = vec3(1.0, 0.9, 0.5);

    // Final color with limb darkening and variation
    vec3 surfaceColor = baseColor * limb * variation;

    alpha = 1.0;

    // Depth calculation
    vec4 clipPos = uViewProjection * vec4(hitPos, 1.0);
    depth = (clipPos.z / clipPos.w) * 0.5 + 0.5;

    return surfaceColor;
}

// ── Volumetric clouds ────────────────────────────────────────────────────────

void marchCloudSegment(vec3 ro, vec3 rd, float tStart, float tEnd, int numSteps,
                       inout vec3 scattered, inout float transmittance) {
    if (tStart >= tEnd || transmittance < 0.01) return;
    float stepSize = (tEnd - tStart) / float(numSteps);

    // Coverage threshold: higher coverage = lower threshold = more clouds
    // Map coverage 0-1 to threshold 0.7-0.2 (inverted and shifted)
    float coverageThreshold = 0.7 - uCloudsDensity * 0.5;

    for (int i = 0; i < numSteps; ++i) {
        if (transmittance < 0.01) break;

        float t = tStart + (float(i) + 0.5) * stepSize;
        vec3 pos = ro + rd * t;
        vec3 localPos = pos - uPlanetPosition;

        // Height within cloud layer (0 = bottom, 1 = top)
        float alt = length(localPos) - uPlanetRadius;
        float heightFrac = clamp((alt - uCloudAltitude) / uCloudThickness, 0.0, 1.0);

        // Sample cloud density in rotated planet space
        // Use lower scale multiplier for larger cloud shapes
        vec3 rotatedPos = PLANET_ROTATION * localPos + uPlanetPosition;
        vec3 cloudCoord = (rotatedPos + vec3(uTime * 0.005 * uCloudsSpeed)) * uCloudsScale * 0.5;
        float rawDensity = cloudNoise(cloudCoord);

        // Height-based shape: cumulus clouds are flat-bottomed, rounded on top
        // Flat bottom (sharp cutoff), rounded top (gradual falloff)
        float bottomFalloff = smoothstep(0.0, 0.15, heightFrac);  // Sharp bottom
        float topFalloff = 1.0 - pow(heightFrac, 2.0);            // Rounded top (quadratic)
        float heightShape = bottomFalloff * topFalloff;

        // Apply coverage threshold with soft edge
        float softEdge = 0.15 + 0.1 * (1.0 - uCloudsDensity);  // Softer edges at low coverage
        float density = smoothstep(coverageThreshold, coverageThreshold + softEdge, rawDensity);
        density *= heightShape;

        if (density < 0.001) continue;

        // Lighting
        vec3 normal = normalize(localPos);
        float NdotL = clamp(dot(normal, uSunDirection), 0.0, 1.0);

        // Henyey-Greenstein phase function for realistic cloud scattering
        float cosTheta = dot(rd, uSunDirection);
        float g = 0.7;  // Forward scattering bias
        float hg = (1.0 - g*g) / pow(1.0 + g*g - 2.0*g*cosTheta, 1.5) / (4.0 * PI);
        float phase = mix(0.25, hg * 3.0, 0.8);  // Blend with isotropic

        // Multi-step shadow sampling for softer self-shadowing
        float shadowDensity = 0.0;
        if (uQuality >= 1.0) {
            // Two shadow samples at different distances
            for (int s = 1; s <= 2; s++) {
                vec3 sp = pos + uSunDirection * uCloudThickness * float(s) * 0.4;
                vec3 sLocal = sp - uPlanetPosition;
                float sAlt = length(sLocal) - uPlanetRadius;
                float sH = (sAlt - uCloudAltitude) / uCloudThickness;
                if (sH >= 0.0 && sH <= 1.0) {
                    vec3 sRot = PLANET_ROTATION * sLocal + uPlanetPosition;
                    vec3 sCoord = (sRot + vec3(uTime * 0.005 * uCloudsSpeed)) * uCloudsScale * 0.5;
                    float sd = cloudNoiseCheap(sCoord);
                    float sBottom = smoothstep(0.0, 0.15, sH);
                    float sTop = 1.0 - pow(sH, 2.0);
                    sd = smoothstep(coverageThreshold, coverageThreshold + softEdge, sd) * sBottom * sTop;
                    shadowDensity += sd * 0.5;
                }
            }
        }
        float sunTransmittance = exp(-shadowDensity * 4.0);

        // Beer-Lambert absorption (less aggressive for fluffier look)
        float absorption = density * stepSize * 12.0;

        // Lighting: direct sun + ambient + powder effect
        vec3 sunLight = uSunColor * uSunIntensity * NdotL * phase * sunTransmittance;
        vec3 ambient = uAtmosphereColor * 0.15;  // More ambient for softer shadows

        // Powder effect: light penetrating thin cloud edges
        float powder = 1.0 - exp(-density * 4.0);
        vec3 powderLight = uSunColor * powder * 0.2 * sunTransmittance;

        vec3 luminance = uCloudColor * (sunLight + ambient + powderLight);

        // Silver lining: bright rim when looking toward sun through thin cloud
        float silverLining = pow(1.0 - density, 2.0) * max(cosTheta, 0.0);
        luminance += uSunColor * silverLining * 0.5 * uSunIntensity * sunTransmittance;

        // Accumulate with energy-conserving blend
        float alpha = 1.0 - exp(-absorption);
        scattered += luminance * transmittance * alpha;
        transmittance *= (1.0 - alpha);
    }
}

vec4 volumetricClouds(vec3 ro, vec3 rd, float surfaceDist) {
    if (uCloudsDensity <= 0.0) return vec4(0.0);

    float cloudInnerR = uPlanetRadius + uCloudAltitude;
    float cloudOuterR = cloudInnerR + uCloudThickness;

    vec2 tOuter = sphIntersect2(ro, rd, Sphere(uPlanetPosition, cloudOuterR));
    if (tOuter.x < 0.0) return vec4(0.0);

    vec2 tInner = sphIntersect2(ro, rd, Sphere(uPlanetPosition, cloudInnerR));

    // Front segment: outer shell entry → inner shell entry (or outer exit if grazing)
    float frontStart = max(tOuter.x, 0.0);
    float frontEnd = (tInner.x > 0.0) ? tInner.x : tOuter.y;
    frontEnd = min(frontEnd, surfaceDist);

    // Back segment: only for limb clouds (when ray MISSES the planet surface)
    // If we hit the planet, don't render clouds behind it
    float backStart = -1.0, backEnd = -1.0;
    bool hitPlanet = (surfaceDist < 1e9);  // Did we hit the solid surface?

    if (!hitPlanet && tInner.y > 0.0 && tInner.y < tOuter.y) {
        // Ray passed through cloud shell without hitting planet - render far side
        backStart = tInner.y;
        backEnd = tOuter.y;
    }

    int numSteps = 8 + int(uQuality) * 12; // 8-32 steps per segment for better quality
    vec3 scattered = vec3(0.0);
    float transmittance = 1.0;

    marchCloudSegment(ro, rd, frontStart, frontEnd, numSteps, scattered, transmittance);
    if (backStart > 0.0)
        marchCloudSegment(ro, rd, backStart, backEnd, numSteps / 2, scattered, transmittance);

    return vec4(scattered, 1.0 - transmittance);
}

// ── Surface intersection ─────────────────────────────────────────────────────

Hit intersectPlanet(vec3 ro, vec3 rd) {
    float len = sphIntersect(ro, rd, getPlanet());
    if (len < 0.) { return miss; }
    vec3 position = ro + len * rd;
    vec3 rotatedCoord = PLANET_ROTATION * (position - uPlanetPosition) + uPlanetPosition;
    float rawNoise = planetNoise(rotatedCoord);
    vec3 normal = planetNormal(position);

    // Coastline threshold
    float coastline = (uSandLevel + uWaterLevel) / 5.0;
    float landMask = smoothstep(coastline - 0.001, coastline + 0.001, rawNoise);

    // Water: smooth depth gradient
    float waterDepth = clamp(rawNoise / max(coastline, 0.001), 0.0, 1.0);
    vec3 waterColor = mix(uWaterColorDeep, uWaterColorSurface, waterDepth);

    // Land: altitude-based biome coloring (relative to water level)
    float relativeHeight = rawNoise - coastline;  // Height above water
    float altitude = 5.0 * max(relativeHeight, 0.0);  // Only positive (above water)
    vec3 landColor = uSandColor;
    landColor = mix(landColor, uTreeColor, smoothstep(uTreeLevel, uTreeLevel + uTransition, altitude));
    landColor = mix(landColor, uRockColor, smoothstep(uRockLevel, uRockLevel + uTransition, altitude));
    landColor = mix(landColor, uIceColor, smoothstep(uIceLevel, uIceLevel + uTransition, altitude));

    vec3 color = mix(waterColor, landColor, landMask);

    // Latitude effects
    vec3 localDir = normalize(rotatedCoord - uPlanetPosition);
    float latitude = abs(localDir.y);

    if (uBandingStrength > 0.0) {
        // Jupiter-style bands with turbulent flow
        float lat = localDir.y;

        // Warp latitude with flowing turbulence
        vec3 nc = rotatedCoord * 2.0;
        float warp1 = noise(nc + vec3(uTime * 0.02, 0.0, 0.0)) - 0.5;
        float warp2 = noise(nc * 1.5 + vec3(0.0, uTime * 0.015, 17.0)) - 0.5;
        float warpedLat = lat + (warp1 * 0.06 + warp2 * 0.04) * uBandingStrength;

        // Multiple band frequencies for varied width bands
        float b1 = sin(warpedLat * uBandingFrequency) * 0.5 + 0.5;
        float b2 = sin(warpedLat * uBandingFrequency * 0.4 + 0.5) * 0.5 + 0.5;
        float b3 = sin(warpedLat * uBandingFrequency * 1.7 - 0.3) * 0.5 + 0.5;

        // Combine for complex band structure
        float band = b1 * 0.5 + b2 * 0.3 + b3 * 0.2;

        // Flowing streaks within bands - use 3D position instead of atan to avoid seam
        vec3 streakCoord = localDir * 8.0 + vec3(0.0, lat * uBandingFrequency, uTime * 0.05);
        float streak = noise(streakCoord);
        band += (streak - 0.5) * 0.15;

        // Small turbulent eddies
        float eddy = noise(rotatedCoord * 12.0 + vec3(uTime * 0.03));
        float eddyMask = noise(rotatedCoord * 3.0);  // Where eddies appear
        eddyMask = smoothstep(0.55, 0.7, eddyMask);
        band += (eddy - 0.5) * 0.2 * eddyMask;

        band = clamp(band, 0.0, 1.0);

        // Three-tone color palette
        vec3 darkBand = uTreeColor;
        vec3 midBand = uRockColor;
        vec3 lightBand = uSandColor;

        vec3 bandColor;
        if (band < 0.5) {
            bandColor = mix(darkBand, midBand, band * 2.0);
        } else {
            bandColor = mix(midBand, lightBand, (band - 0.5) * 2.0);
        }

        color = mix(color, bandColor, uBandingStrength);
    }

    if (uPolarCapSize > 0.0) {
        float polarEdge = 1.0 - uPolarCapSize;
        float polarMask = smoothstep(polarEdge, polarEdge + 0.1, latitude);
        color = mix(color, uIceColor, polarMask);
    }

    float specular = 1.0 - landMask;
    return Hit(len, normal, Material(color, 1., specular));
}

// ── Radiance ─────────────────────────────────────────────────────────────────

vec3 radiance(vec3 ro, vec3 rd) {
    vec3 color = vec3(0.);
    float spaceMask = 1.;
    Hit hit = intersectPlanet(ro, rd);

    if (hit.len < INFINITY) {
        spaceMask = 0.;
        vec3 hitPosition = ro + hit.len * rd;

        // Diffuse lighting
        float NdotL = dot(hit.normal, uSunDirection);
        float directLightIntensity = pow(clamp(NdotL, 0.0, 1.0), 2.) * uSunIntensity;
        vec3 diffuseLight = directLightIntensity * uSunColor;
        vec3 diffuseColor = hit.material.color.rgb * (uAmbientLight + diffuseLight);

        color = diffuseColor;
    } else {
        color = spaceColor(rd);
    }

    // Volumetric clouds — composited between surface and atmosphere
    vec4 clouds = volumetricClouds(ro, rd, hit.len);
    color = color * (1.0 - clouds.a) + clouds.rgb;

    return color + atmosphereColor(ro, rd, spaceMask);
}

// ── Main ─────────────────────────────────────────────────────────────────────

void main() {
    // Construct ray direction using inverse projection for correct FOV
    // uv is in [-1, 1] clip space
    vec4 clipNear = vec4(uv, -1.0, 1.0);
    vec4 clipFar = vec4(uv, 1.0, 1.0);

    // Unproject to view space
    vec4 viewNear = uInvProjection * clipNear;
    vec4 viewFar = uInvProjection * clipFar;
    viewNear /= viewNear.w;
    viewFar /= viewFar.w;

    // Ray direction in view space
    vec3 viewDir = normalize(viewFar.xyz - viewNear.xyz);

    // Transform to world space
    vec3 ro = uCameraPosition;
    vec3 rd = normalize((uInvView * vec4(viewDir, 0.0)).xyz);

    // Special path for emissive bodies (stars)
    if (uIsEmissive != 0) {
        float starAlpha;
        float starDepth;
        vec3 starColor = renderStar(ro, rd, starAlpha, starDepth);

        if (starAlpha <= 0.0) {
            discard;
        }

        // Tone map the star
        starColor = simpleReinhardToneMapping(starColor);
        fragColor = vec4(starColor, starAlpha);
        gl_FragDepth = starDepth;
        return;
    }

    // Check if ray hits this planet surface
    Hit hit = intersectPlanet(ro, rd);

    // Check atmosphere bounds (larger than planet)
    float atmosRadius = uPlanetRadius * (1.0 + uAtmosphereDensity * 0.5);
    Sphere atmosSphere = Sphere(uPlanetPosition, atmosRadius);
    float atmosHit = sphIntersect(ro, rd, atmosSphere);

    // Discard if we don't hit planet or atmosphere
    if (hit.len >= INFINITY && atmosHit < 0.0) {
        discard;
    }

    float alpha = 1.0;

    if (hit.len >= INFINITY) {
        // Ray is in atmosphere but didn't hit surface - render atmosphere only with alpha
        float spaceMask = 1.0;
        vec3 color = atmosphereColor(ro, rd, spaceMask);
        color = simpleReinhardToneMapping(color);
        // Use atmosphere intensity as alpha for blending with background
        alpha = clamp(length(color) * 2.0, 0.0, 0.8);
        fragColor = vec4(color, alpha);
        // Write depth at atmosphere edge
        vec3 atmosHitPos = ro + rd * atmosHit;
        vec4 clipPos = uViewProjection * vec4(atmosHitPos, 1.0);
        gl_FragDepth = (clipPos.z / clipPos.w) * 0.5 + 0.5;
    } else {
        // Hit the planet - render full planet with clouds and atmosphere
        vec3 color = radiance(ro, rd);
        color = simpleReinhardToneMapping(color);
        fragColor = vec4(color, 1.0);
        // Write proper depth for planet surface
        vec3 hitPos = ro + rd * hit.len;
        vec4 clipPos = uViewProjection * vec4(hitPos, 1.0);
        gl_FragDepth = (clipPos.z / clipPos.w) * 0.5 + 0.5;
    }
}
