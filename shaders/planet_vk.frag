#version 450

// Procedural Planet Renderer — Vulkan GLSL
// Based on Julien Sulpis "Procedural Blue Planet"
// Extended with ridged noise, craters, continents, volumetric clouds, and black holes

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 fragColor;

// ── Uniform buffer (set=0, binding=0) ───────────────────────────────────────
// Layout matches PlanetUniformsVk in VulkanTypes.hpp (std140 rules).

layout(set = 0, binding = 0) uniform PlanetUniforms {
    mat4  invView;              // 64 bytes
    mat3  planetRotation;       // 48 bytes (3 x vec4, std140)

    vec4  cameraPos_time;       // xyz = camera pos, w = time
    vec4  planetPos_radius;     // xyz = planet pos, w = radius
    vec4  resolution_rot;       // xy = resolution, z = rotOffset, w = rotSpeed
    vec4  noiseParams;          // x = noiseStr, y = quality, z = terrainScale, w = domainWarp
    vec4  fbmParams;            // x = persistence, y = lacunarity, z = exponentiation, w = octaves
    vec4  terrainFeatures;      // x = ridged, y = crater, z = continent, w = waterLevel
    vec4  bandingPolar;         // x = bandStr, y = bandFreq, z = polarCap, w = pad
    vec4  cloudParams1;         // x = density, y = scale, z = speed, w = altitude
    vec4  cloudParams2;         // x = thickness, y = sunIntensity, z = ambientLight, w = atmoDensity
    vec4  atmosphereColorPad;   // xyz = color
    vec4  sunDirectionPad;      // xyz = direction
    vec4  sunColorPad;          // xyz = color
    vec4  deepSpaceColorPad;    // xyz = color
    vec4  waterColorDeepPad;    // xyz
    vec4  waterColorSurfPad;    // xyz
    vec4  sandColorPad;         // xyz
    vec4  treeColorPad;         // xyz
    vec4  rockColorPad;         // xyz
    vec4  iceColorPad;          // xyz
    vec4  cloudColorPad;        // xyz
    vec4  biomeLevels;          // x = sand, y = tree, z = rock, w = ice
    vec4  transitionPad;        // x = transition

    vec4  bhParams1;            // x = isBlackHole, y = mass, z = accretionInner, w = accretionOuter
    vec4  bhParams2;            // x = diskSpeed, y = turbulence, z = brightness, w = tempInner
    vec4  bhParams3;            // x = tempOuter, y = dopplerStr, z = raySteps, w = pad
    vec4  bhDiskTintPad;        // xyz = tint
} u;

// ── Textures ────────────────────────────────────────────────────────────────
layout(set = 0, binding = 1) uniform sampler3D uNoiseTexture;
layout(set = 0, binding = 2) uniform samplerCube uStarmap;

// ── Accessor macros — map old uniform names to UBO fields ───────────────────
#define uTime              u.cameraPos_time.w
#define uCameraPosition    u.cameraPos_time.xyz
#define uPlanetPosition    u.planetPos_radius.xyz
#define uPlanetRadius      u.planetPos_radius.w
#define uResolution        u.resolution_rot.xy
#define uRotationOffset    u.resolution_rot.z
#define uRotationSpeed     u.resolution_rot.w
#define uNoiseStrength     u.noiseParams.x
#define uQuality           u.noiseParams.y
#define uTerrainScale      u.noiseParams.z
#define uDomainWarpStrength u.noiseParams.w
#define uFbmPersistence    u.fbmParams.x
#define uFbmLacunarity     u.fbmParams.y
#define uFbmExponentiation u.fbmParams.z
#define uFbmOctaves        int(u.fbmParams.w)
#define uRidgedStrength    u.terrainFeatures.x
#define uCraterStrength    u.terrainFeatures.y
#define uContinentScale    u.terrainFeatures.z
#define uWaterLevel        u.terrainFeatures.w
#define uBandingStrength   u.bandingPolar.x
#define uBandingFrequency  u.bandingPolar.y
#define uPolarCapSize      u.bandingPolar.z
#define uCloudsDensity     u.cloudParams1.x
#define uCloudsScale       u.cloudParams1.y
#define uCloudsSpeed       u.cloudParams1.z
#define uCloudAltitude     u.cloudParams1.w
#define uCloudThickness    u.cloudParams2.x
#define uSunIntensity      u.cloudParams2.y
#define uAmbientLight      u.cloudParams2.z
#define uAtmosphereDensity u.cloudParams2.w
#define uAtmosphereColor   u.atmosphereColorPad.xyz
#define uSunDirection      u.sunDirectionPad.xyz
#define uSunColor          u.sunColorPad.xyz
#define uDeepSpaceColor    u.deepSpaceColorPad.xyz
#define uWaterColorDeep    u.waterColorDeepPad.xyz
#define uWaterColorSurface u.waterColorSurfPad.xyz
#define uSandColor         u.sandColorPad.xyz
#define uTreeColor         u.treeColorPad.xyz
#define uRockColor         u.rockColorPad.xyz
#define uIceColor          u.iceColorPad.xyz
#define uCloudColor        u.cloudColorPad.xyz
#define uSandLevel         u.biomeLevels.x
#define uTreeLevel         u.biomeLevels.y
#define uRockLevel         u.biomeLevels.z
#define uIceLevel          u.biomeLevels.w
#define uTransition        u.transitionPad.x
#define uInvView           u.invView
#define PLANET_ROTATION    u.planetRotation

#define uIsBlackHole       (u.bhParams1.x > 0.5)
#define uBhMass            u.bhParams1.y
#define uBhAccretionInner  u.bhParams1.z
#define uBhAccretionOuter  u.bhParams1.w
#define uBhDiskSpeed       u.bhParams2.x
#define uBhDiskTurbulence  u.bhParams2.y
#define uBhDiskBrightness  u.bhParams2.z
#define uBhTempInner       u.bhParams2.w
#define uBhTempOuter       u.bhParams3.x
#define uBhDopplerStrength u.bhParams3.y
#define uBhRaySteps        int(u.bhParams3.z)
#define uBhDiskTint        u.bhDiskTintPad.xyz

// Constants
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

// Cloud-specific FBM — low base frequency, smooth
float cloudFBM(vec3 p) {
    float amplitude = 0.5;
    float frequency = 1.0;
    float total = 0.0;
    float normalization = 0.0;
    int octaves = 2 + int(floor(uQuality));

    for (int i = 0; i < octaves; ++i) {
        total += noise(p * frequency) * amplitude;
        normalization += amplitude;
        amplitude *= 0.5;
        frequency *= 2.0;
    }

    total /= normalization;
    return total;
}

// Single-pass domain warping for billowy cloud shapes
float cloudNoise(vec3 p) {
    vec3 warp = vec3(
        cloudFBM(p),
        cloudFBM(p + vec3(5.2, 1.3, 3.7)),
        cloudFBM(p + vec3(1.7, 9.2, 4.1)));

    return cloudFBM(p + 2.5 * warp);
}

// Cheap cloud density for shadow estimation (skip domain warping)
float cloudNoiseCheap(vec3 p) {
    return cloudFBM(p);
}

// ── Terrain ──────────────────────────────────────────────────────────────────

float planetNoise(vec3 p) {
    vec3 tp = p * uTerrainScale;

    // Domain warping for organic/alien shapes (use fewer octaves for warp)
    if (uDomainWarpStrength > 0.0) {
        int warpOct = max(uFbmOctaves / 2, 2);
        vec3 warpOffset = vec3(
            fbm(tp, warpOct, uFbmPersistence, uFbmLacunarity, uFbmExponentiation),
            fbm(tp + vec3(43.235, 23.112, 0.0), warpOct, uFbmPersistence, uFbmLacunarity, uFbmExponentiation),
            fbm(tp + vec3(0.0, 71.823, 37.156), warpOct, uFbmPersistence, uFbmLacunarity, uFbmExponentiation));
        tp = tp + uDomainWarpStrength * warpOffset;
    }

    // Base terrain FBM
    float baseFBM = fbm(tp, uFbmOctaves, uFbmPersistence, uFbmLacunarity, uFbmExponentiation);

    // Ridged mountains
    if (uRidgedStrength > 0.0) {
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
        float continent = noise(p * uContinentScale * 0.15);
        float continentMask = smoothstep(0.38, 0.58, continent);
        f = f * mix(0.15, 1.0, continentMask);
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

// Compute normal by finite-differencing planetNoise on the sphere surface directly,
// avoiding 4 redundant ray-sphere intersections + full planetDist evaluations.
vec3 planetNormal(vec3 hitPos) {
    vec3 localDir = normalize(hitPos - uPlanetPosition);
    float e = max(.01, .03 * smoothstep(1300., 300., uResolution.x));

    // Build a tangent frame on the sphere surface
    vec3 tangent = normalize(cross(localDir, localDir.y < 0.99 ? vec3(0,1,0) : vec3(1,0,0)));
    vec3 bitangent = cross(localDir, tangent);

    // Sample planetNoise at 4 offset points on the sphere surface (rotated space)
    vec3 c  = PLANET_ROTATION * (hitPos - uPlanetPosition) + uPlanetPosition;
    vec3 px = PLANET_ROTATION * (localDir * uPlanetRadius + tangent   * e) + uPlanetPosition;
    vec3 nx = PLANET_ROTATION * (localDir * uPlanetRadius - tangent   * e) + uPlanetPosition;
    vec3 py = PLANET_ROTATION * (localDir * uPlanetRadius + bitangent * e) + uPlanetPosition;
    vec3 ny = PLANET_ROTATION * (localDir * uPlanetRadius - bitangent * e) + uPlanetPosition;

    float coastline = (uSandLevel + uWaterLevel) / 5.0;
    float hc = max(planetNoise(c),  coastline);
    float hpx = max(planetNoise(px), coastline);
    float hnx = max(planetNoise(nx), coastline);
    float hpy = max(planetNoise(py), coastline);
    float hny = max(planetNoise(ny), coastline);

    // Central differences → surface gradient
    float dhdx = (hpx - hnx) / (2.0 * e);
    float dhdy = (hpy - hny) / (2.0 * e);

    // Perturb sphere normal by the gradient
    return normalize(localDir - dhdx * tangent - dhdy * bitangent);
}

// ── Stars & space ────────────────────────────────────────────────────────────

vec3 spaceColor(vec3 direction) {
    // Sample real star positions from cubemap
    vec3 starColor = texture(uStarmap, direction).rgb;
    starColor *= 3.0;

    return starColor + uDeepSpaceColor;
}

// ── Black Hole ───────────────────────────────────────────────────────────────

// Tanner Helland's fitted curves for CIE 1931 → sRGB.
// Input: temperature in Kelvin (1000–40000 K).
vec3 blackbodyColor(float tempK) {
    float t = tempK / 100.0;
    vec3 c;

    // Red
    if (t <= 66.0)
        c.r = 1.0;
    else
        c.r = 1.2929 * pow(t - 60.0, -0.1332);

    // Green
    if (t <= 66.0)
        c.g = 0.3901 * log(t) - 0.6318;
    else
        c.g = 1.1299 * pow(t - 60.0, -0.0755);

    // Blue
    if (t >= 66.0)
        c.b = 1.0;
    else if (t <= 19.0)
        c.b = 0.0;
    else
        c.b = 0.5432 * log(t - 10.0) - 1.1963;

    return clamp(c, 0.0, 1.0);
}

// Core black hole ray tracer using Schwarzschild geodesic integration.
vec3 traceBlackHole(vec3 ro, vec3 rd) {
    // Schwarzschild radius
    float Rs = uBhMass * uPlanetRadius * 0.5;

    // Transform ray into BH-local coordinates (origin at BH center)
    vec3 pos = ro - uPlanetPosition;
    vec3 vel = rd;

    // Conserved specific angular momentum magnitude
    float h = length(cross(pos, vel));

    // Accretion disk bounds
    float rInner = uBhAccretionInner * Rs;
    float rOuter = uBhAccretionOuter * Rs;

    // Front-to-back compositing state for accretion disk
    vec3  diskColor = vec3(0.0);
    float diskAlpha = 0.0;

    float prevY = pos.y; // track equatorial plane crossings

    for (int i = 0; i < uBhRaySteps; ++i) {
        float r = length(pos);

        // Captured by event horizon
        if (r < Rs) {
            return diskColor; // black — absorbed
        }

        // Escaped far enough — sample lensed background
        if (r > 100.0 * Rs) {
            vec3 bg = spaceColor(normalize(vel));
            return diskColor + (1.0 - diskAlpha) * bg;
        }

        // Geodesic acceleration: Schwarzschild effective potential
        float r2 = r * r;
        float r5 = r2 * r2 * r;
        vec3 accel = -1.5 * h * h * Rs / r5 * pos;

        // Adaptive step size: small near BH, larger far away
        float dt = 0.3 * r / (1.0 + 2.0 * Rs / max(r - Rs, 0.01));
        dt = clamp(dt, 0.01 * Rs, 2.0 * Rs);

        // Velocity Verlet integration
        vec3 newPos = pos + vel * dt + 0.5 * accel * dt * dt;
        float newR = length(newPos);
        float newR5 = newR * newR * newR * newR * newR;
        vec3 newAccel = -1.5 * h * h * Rs / newR5 * newPos;
        vec3 newVel = vel + 0.5 * (accel + newAccel) * dt;

        // Check equatorial plane crossing (y sign flip)
        if (prevY * newPos.y < 0.0) {
            // Interpolate to find crossing point
            float t_cross = abs(prevY) / max(abs(prevY) + abs(newPos.y), 1e-6);
            vec3 crossPos = mix(pos, newPos, t_cross);
            float crossR = length(crossPos);

            // Is crossing within the accretion disk annulus?
            if (crossR >= rInner && crossR <= rOuter) {
                // Radial parameter [0,1] from inner to outer edge
                float radialT = (crossR - rInner) / (rOuter - rInner);

                // Temperature gradient: hot inner, cool outer
                float temp = mix(uBhTempInner, uBhTempOuter, radialT);
                vec3 bbColor = blackbodyColor(temp);

                // Procedural turbulence using existing noise texture
                vec3 noiseCoord = crossPos * 0.5 / Rs;
                // Add time-based rotation for disk orbital motion
                float angle = atan(crossPos.z, crossPos.x);
                angle += uTime * uBhDiskSpeed * sqrt(Rs / max(crossR, Rs)) * 0.5;
                noiseCoord.x = crossR * cos(angle) * 0.5 / Rs;
                noiseCoord.z = crossR * sin(angle) * 0.5 / Rs;
                float turb = noise(noiseCoord);
                turb = mix(1.0, turb, uBhDiskTurbulence);

                // Density falls off at inner and outer edges
                float edgeFade = smoothstep(0.0, 0.15, radialT)
                               * smoothstep(1.0, 0.85, radialT);

                // Doppler beaming
                vec3 orbitDir = normalize(cross(vec3(0.0, 1.0, 0.0), normalize(crossPos)));
                float orbitalV = uBhDiskSpeed * sqrt(Rs / (2.0 * max(crossR, Rs)));
                float doppler = 1.0 + uBhDopplerStrength * orbitalV * dot(normalize(vel), orbitDir) * 4.0;
                doppler = max(doppler, 0.1);

                // Luminosity: brighter at inner edge (1/r² falloff)
                float luminosity = (rInner / max(crossR, rInner));
                luminosity *= luminosity;

                vec3 sampleColor = bbColor * uBhDiskTint * turb * edgeFade
                                 * luminosity * doppler * uBhDiskBrightness;
                float sampleAlpha = edgeFade * turb * 0.8;

                // Front-to-back alpha compositing
                diskColor += (1.0 - diskAlpha) * sampleAlpha * sampleColor;
                diskAlpha += (1.0 - diskAlpha) * sampleAlpha;
                diskAlpha = min(diskAlpha, 1.0);
            }
        }

        prevY = newPos.y;
        pos = newPos;
        vel = newVel;
    }

    // Ray didn't escape or get captured within step limit — treat as escaped
    vec3 bg = spaceColor(normalize(vel));
    return diskColor + (1.0 - diskAlpha) * bg;
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

// ── Volumetric clouds ────────────────────────────────────────────────────────

void marchCloudSegment(vec3 ro, vec3 rd, float tStart, float tEnd, int numSteps,
                       inout vec3 scattered, inout float transmittance) {
    if (tStart >= tEnd || transmittance < 0.01) return;
    float stepSize = (tEnd - tStart) / float(numSteps);

    for (int i = 0; i < numSteps; ++i) {
        if (transmittance < 0.01) break;

        float t = tStart + (float(i) + 0.5) * stepSize;
        vec3 pos = ro + rd * t;
        vec3 localPos = pos - uPlanetPosition;

        // Height within cloud layer
        float alt = length(localPos) - uPlanetRadius;
        float heightFrac = (alt - uCloudAltitude) / uCloudThickness;

        // Sample cloud density in rotated planet space
        vec3 rotatedPos = PLANET_ROTATION * localPos + uPlanetPosition;
        vec3 cloudCoord = (rotatedPos + vec3(uTime * .008 * uCloudsSpeed)) * uCloudsScale;
        float density = cloudNoise(cloudCoord);

        // Height-based shape: less dense at top and bottom of layer
        density *= smoothstep(0.0, 0.25, heightFrac) * smoothstep(1.0, 0.75, heightFrac);

        // Threshold
        float threshold = 1.0 - uCloudsDensity * 0.5;
        density = smoothstep(threshold, threshold + 0.1, density);

        if (density < 0.001) continue;

        // Lighting
        vec3 normal = normalize(localPos);
        float NdotL = clamp(dot(normal, uSunDirection), 0.0, 1.0);

        // Forward scattering phase — bright when looking toward sun
        float cosTheta = dot(rd, uSunDirection);
        float phase = mix(0.5, 2.5, pow(clamp(cosTheta * 0.5 + 0.5, 0.0, 1.0), 3.0));

        // Single shadow step toward sun using cheap noise
        float shadowDensity = 0.0;
        if (uQuality >= 1.0) {
            vec3 sp = pos + uSunDirection * uCloudThickness * 0.5;
            vec3 sLocal = sp - uPlanetPosition;
            float sAlt = length(sLocal) - uPlanetRadius;
            float sH = (sAlt - uCloudAltitude) / uCloudThickness;
            if (sH >= 0.0 && sH <= 1.0) {
                vec3 sRot = PLANET_ROTATION * sLocal + uPlanetPosition;
                vec3 sCoord = (sRot + vec3(uTime * .008 * uCloudsSpeed)) * uCloudsScale;
                float sd = cloudNoiseCheap(sCoord);
                sd *= smoothstep(0.0, 0.25, sH) * smoothstep(1.0, 0.75, sH);
                sd = smoothstep(threshold, threshold + 0.1, sd);
                shadowDensity = sd;
            }
        }
        float sunTransmittance = exp(-shadowDensity * 3.0);

        // Accumulate
        float absorption = density * stepSize * 18.0;

        vec3 sunLight = uSunColor * uSunIntensity * NdotL * phase * sunTransmittance;
        vec3 ambient = uAtmosphereColor * 0.08;
        vec3 luminance = uCloudColor * (sunLight + ambient);

        // Silver lining: bright edges where transmittance is still high but there's cloud
        luminance += uSunColor * phase * 0.3 * uSunIntensity * sunTransmittance;

        scattered += luminance * transmittance * (1.0 - exp(-absorption));
        transmittance *= exp(-absorption);
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

    // Back segment: inner shell exit → outer shell exit (limb clouds behind planet)
    float backStart = -1.0, backEnd = -1.0;
    if (tInner.y > 0.0 && tInner.y < tOuter.y) {
        backStart = max(tInner.y, surfaceDist);
        backEnd = tOuter.y;
    }

    int numSteps = 4 + int(uQuality) * 6; // 4-16 steps per segment
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

    // Land: altitude-based biome coloring
    float altitude = 5.0 * rawNoise;
    vec3 landColor = uSandColor;
    landColor = mix(landColor, uTreeColor, smoothstep(uTreeLevel, uTreeLevel + uTransition, altitude));
    landColor = mix(landColor, uRockColor, smoothstep(uRockLevel, uRockLevel + uTransition, altitude));
    landColor = mix(landColor, uIceColor, smoothstep(uIceLevel, uIceLevel + uTransition, altitude));

    vec3 color = mix(waterColor, landColor, landMask);

    // Latitude effects
    vec3 localDir = normalize(rotatedCoord - uPlanetPosition);
    float latitude = abs(localDir.y);

    if (uBandingStrength > 0.0) {
        float band = sin(localDir.y * uBandingFrequency) * 0.5 + 0.5;
        float bandNoise = noise(rotatedCoord * 2.0) * 0.3;
        band = clamp(band + bandNoise, 0.0, 1.0);
        color = mix(color, color * (0.6 + 0.8 * band), uBandingStrength);
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
    if (uIsBlackHole) return traceBlackHole(ro, rd);

    vec3 color = vec3(0.);
    float spaceMask = 1.;
    Hit hit = intersectPlanet(ro, rd);

    if (hit.len < INFINITY) {
        spaceMask = 0.;
        vec3 hitPosition = ro + hit.len * rd;

        // Diffuse
        float directLightIntensity = pow(clamp(dot(hit.normal, uSunDirection), 0.0, 1.0), 2.) * uSunIntensity;
        vec3 diffuseLight = directLightIntensity * uSunColor;
        vec3 diffuseColor = hit.material.color.rgb * (uAmbientLight + diffuseLight);

        // Phong specular
        vec3 reflected = normalize(reflect(-uSunDirection, hit.normal));
        float phongValue = pow(max(0.0, dot(rd, reflected)), 10.) * .2 * uSunIntensity;
        vec3 specularColor = hit.material.specular * vec3(phongValue);

        color = diffuseColor + specularColor;
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
    vec2 screenUV = uv;
    screenUV.x *= uResolution.x / uResolution.y;

    vec3 ro = uCameraPosition;
    vec3 rd = normalize(vec3(screenUV, -1));
    rd = (uInvView * vec4(rd, 0.0)).xyz;

    vec3 color = radiance(ro, rd);

    color = simpleReinhardToneMapping(color);

    fragColor = vec4(color, 1.0);
}
