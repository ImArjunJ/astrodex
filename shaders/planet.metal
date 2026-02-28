// Procedural Planet Renderer — Metal Shading Language
// Translated from planet.frag (GLSL 450)
// Based on Julien Sulpis "Procedural Blue Planet", extended with ridged noise,
// craters, continents, and volumetric clouds.

#include <metal_stdlib>
using namespace metal;

// ── Uniform buffer ────────────────────────────────────────────────────────────
// Layout must match PlanetUniformsMetal in MetalRenderer.mm exactly.
// All vec3 fields padded to float4 (16 bytes) for alignment.

struct PlanetUniforms {
    float4x4 invView;           // 64 bytes
    float3x3 planetRotation;    // 48 bytes (stored as 3×float4)

    float4 cameraPos_time;      // xyz = camera pos, w = time
    float4 planetPos_radius;    // xyz = planet pos, w = radius
    float4 resolution_rot;      // xy = resolution, z = rotOffset, w = rotSpeed
    float4 noiseParams;         // x = noiseStrength, y = quality, z = terrainScale, w = domainWarp
    float4 fbmParams;           // x = persistence, y = lacunarity, z = exponentiation, w = octaves(float)
    float4 terrainFeatures;     // x = ridged, y = crater, z = continent, w = waterLevel
    float4 bandingPolar;        // x = bandStr, y = bandFreq, z = polarCap, w = 0
    float4 cloudParams1;        // x = density, y = scale, z = speed, w = altitude
    float4 cloudParams2;        // x = thickness, y = sunIntensity, z = ambientLight, w = atmosphereDensity
    float4 atmosphereColor;     // xyz = color, w = 0
    float4 sunDirection;        // xyz, w = 0
    float4 sunColor;            // xyz, w = 0
    float4 deepSpaceColor;      // xyz, w = 0
    float4 waterColorDeep;      // xyz, w = 0
    float4 waterColorSurface;   // xyz, w = 0
    float4 sandColor;           // xyz, w = 0
    float4 treeColor;           // xyz, w = 0
    float4 rockColor;           // xyz, w = 0
    float4 iceColor;            // xyz, w = 0
    float4 cloudColor;          // xyz, w = 0
    float4 biomeLevels;         // x = sand, y = tree, z = rock, w = ice
    float4 transitionPad;       // x = transition, yzw = 0

    // Black hole parameters
    float4 bhParams1;           // x = isBlackHole, y = mass, z = accretionInner, w = accretionOuter
    float4 bhParams2;           // x = diskSpeed, y = turbulence, z = brightness, w = tempInner
    float4 bhParams3;           // x = tempOuter, y = dopplerStrength, z = raySteps, w = 0
    float4 bhDiskTint;          // xyz = tint, w = 0
};

// ── Vertex stage ──────────────────────────────────────────────────────────────

struct VertexIn {
    float2 aPos [[attribute(0)]];
    float2 aUV  [[attribute(1)]];
};

struct VertexOut {
    float4 position [[position]];
    float2 uv;
};

vertex VertexOut planetVertex(VertexIn in [[stage_in]]) {
    VertexOut out;
    out.uv       = in.aPos;   // [-1,1] range, same as GLSL (uv = aPos)
    out.position = float4(in.aPos, 0.0f, 1.0f);
    return out;
}

// ── Constants ─────────────────────────────────────────────────────────────────

constant float PLANET_INFINITY = 1e10f;
constant float EPSILON         = 1e-3f;
constant float PI              = 3.14159265f;

// ── Structs ───────────────────────────────────────────────────────────────────

struct Material { float3 color; float diffuse; float specular; };
struct Hit      { float len; float3 normal; Material material; };
struct Sphere   { float3 position; float radius; };

inline Hit makeMiss() {
    Hit h;
    h.len              = PLANET_INFINITY;
    h.normal           = float3(0.0f);
    h.material.color   = float3(0.0f);
    h.material.diffuse  = -1.0f;
    h.material.specular = -1.0f;
    return h;
}

// ── Utility ───────────────────────────────────────────────────────────────────

inline float inverseLerp(float v, float a, float b) { return (v - a) / (b - a); }

inline float remap(float v, float inMin, float inMax, float outMin, float outMax) {
    return mix(outMin, outMax, inverseLerp(v, inMin, inMax));
}

inline float sampleNoise(texture3d<float> tex, sampler samp, float3 p) {
    return tex.sample(samp, p * 0.05f).r;
}

inline float sphIntersect(float3 ro, float3 rd, Sphere s) {
    float3 oc = ro - s.position;
    float b   = dot(oc, rd);
    float c   = dot(oc, oc) - s.radius * s.radius;
    float h   = b * b - c;
    if (h < 0.0f) return -1.0f;
    return -b - sqrt(h);
}

inline float2 sphIntersect2(float3 ro, float3 rd, Sphere s) {
    float3 oc = ro - s.position;
    float b   = dot(oc, rd);
    float c   = dot(oc, oc) - s.radius * s.radius;
    float h   = b * b - c;
    if (h < 0.0f) return float2(-1.0f);
    float sq  = sqrt(h);
    return float2(-b - sq, -b + sq);
}

// Column-major Y-rotation, identical to GLSL mat3(col0, col1, col2)
inline float3x3 rotateY(float angle) {
    float c = cos(angle), s = sin(angle);
    return float3x3(float3(c, 0.0f, s), float3(0.0f, 1.0f, 0.0f), float3(-s, 0.0f, c));
}

inline Sphere getPlanet(constant PlanetUniforms& u) {
    Sphere s;
    s.position = u.planetPos_radius.xyz;
    s.radius   = u.planetPos_radius.w;
    return s;
}

inline float3x3 planetRotation(constant PlanetUniforms& u) {
    return u.planetRotation;
}

// ── Noise functions ───────────────────────────────────────────────────────────

float fbm(float3 p, int octaves, float persistence, float lacunarity, float exponentiation,
          float quality, texture3d<float> noiseTexture, sampler noiseSampler)
{
    float amplitude     = 0.5f;
    float frequency     = 3.0f;
    float total         = 0.0f;
    float normalization = 0.0f;
    int qualityDeg = 2 - int(floor(quality));
    int oct = max(octaves - qualityDeg, 1);

    for (int i = 0; i < oct; ++i) {
        total         += sampleNoise(noiseTexture, noiseSampler, p * frequency) * amplitude;
        normalization += amplitude;
        amplitude     *= persistence;
        frequency     *= lacunarity;
    }

    total /= normalization;
    total  = total * 0.8f + 0.1f;
    total  = pow(total, exponentiation);
    return total;
}

float ridgedFBM(float3 p, int octaves, float persistence, float lacunarity,
                float quality, texture3d<float> noiseTexture, sampler noiseSampler)
{
    float amplitude     = 0.5f;
    float frequency     = 3.0f;
    float total         = 0.0f;
    float normalization = 0.0f;
    float weight        = 1.0f;
    int qualityDeg = 2 - int(floor(quality));
    int oct = max(octaves - qualityDeg, 1);

    for (int i = 0; i < oct; ++i) {
        float n = sampleNoise(noiseTexture, noiseSampler, p * frequency);
        n = 1.0f - abs(n * 2.0f - 1.0f);
        n = n * n;
        n *= weight;
        weight        = clamp(n, 0.0f, 1.0f);
        total         += n * amplitude;
        normalization += amplitude;
        amplitude     *= persistence;
        frequency     *= lacunarity;
    }
    return total / normalization;
}

float craterNoise(float3 p, texture3d<float> noiseTexture, sampler noiseSampler) {
    float3 cell    = floor(p);
    float3 frac_p  = fract(p);
    float minDist  = 1e10f;

    for (int x = -1; x <= 1; x++)
    for (int y = -1; y <= 1; y++)
    for (int z = -1; z <= 1; z++) {
        float3 neighbor = float3(x, y, z);
        float3 cellId   = cell + neighbor;
        float3 offset   = float3(
            sampleNoise(noiseTexture, noiseSampler, cellId * 0.37f),
            sampleNoise(noiseTexture, noiseSampler, cellId * 0.37f + float3(17.3f, 0.0f, 0.0f)),
            sampleNoise(noiseTexture, noiseSampler, cellId * 0.37f + float3(0.0f, 43.7f, 0.0f)));
        float dist = length(frac_p - neighbor - offset);
        minDist    = min(minDist, dist);
    }

    float bowl = smoothstep(0.35f, 0.0f, minDist) * -1.0f;
    float rim  = smoothstep(0.25f, 0.38f, minDist) * smoothstep(0.55f, 0.38f, minDist) * 0.4f;
    return bowl + rim;
}

float cloudFBM(float3 p, float quality,
               texture3d<float> noiseTexture, sampler noiseSampler)
{
    float amplitude     = 0.5f;
    float frequency     = 1.0f;
    float total         = 0.0f;
    float normalization = 0.0f;
    int octaves = 2 + int(floor(quality));

    for (int i = 0; i < octaves; ++i) {
        total         += sampleNoise(noiseTexture, noiseSampler, p * frequency) * amplitude;
        normalization += amplitude;
        amplitude     *= 0.5f;
        frequency     *= 2.0f;
    }
    return total / normalization;
}

float cloudNoise(float3 p, float quality,
                 texture3d<float> noiseTexture, sampler noiseSampler)
{
    float3 warp = float3(
        cloudFBM(p,                              quality, noiseTexture, noiseSampler),
        cloudFBM(p + float3(5.2f, 1.3f, 3.7f), quality, noiseTexture, noiseSampler),
        cloudFBM(p + float3(1.7f, 9.2f, 4.1f), quality, noiseTexture, noiseSampler));
    return cloudFBM(p + 2.5f * warp, quality, noiseTexture, noiseSampler);
}

float cloudNoiseCheap(float3 p, float quality,
                      texture3d<float> noiseTexture, sampler noiseSampler)
{
    return cloudFBM(p, quality, noiseTexture, noiseSampler);
}

// ── Terrain ───────────────────────────────────────────────────────────────────

float planetNoise(float3 p, constant PlanetUniforms& u,
                  texture3d<float> noiseTexture, sampler noiseSampler)
{
    float3 tp = p * u.noiseParams.z;  // terrainScale

    float quality     = u.noiseParams.y;
    int   fbmOct      = int(u.fbmParams.w);
    float fbmPersist  = u.fbmParams.x;
    float fbmLac      = u.fbmParams.y;
    float fbmExp      = u.fbmParams.z;
    float domainWarp  = u.noiseParams.w;
    float noiseStr    = u.noiseParams.x;
    float sandLevel   = u.biomeLevels.x;
    float waterLevel  = u.terrainFeatures.w;
    float transition  = u.transitionPad.x;

    if (domainWarp > 0.0f) {
        int warpOct = max(fbmOct / 2, 2);
        float3 warpOffset = float3(
            fbm(tp,                                  warpOct, fbmPersist, fbmLac, fbmExp, quality, noiseTexture, noiseSampler),
            fbm(tp + float3(43.235f, 23.112f, 0.0f), warpOct, fbmPersist, fbmLac, fbmExp, quality, noiseTexture, noiseSampler),
            fbm(tp + float3(0.0f, 71.823f, 37.156f), warpOct, fbmPersist, fbmLac, fbmExp, quality, noiseTexture, noiseSampler));
        tp = tp + domainWarp * warpOffset;
    }

    float baseFBM = fbm(tp, fbmOct, fbmPersist, fbmLac, fbmExp, quality, noiseTexture, noiseSampler);

    if (u.terrainFeatures.x > 0.0f) {
        float ridged = ridgedFBM(tp, fbmOct, fbmPersist, fbmLac, quality, noiseTexture, noiseSampler);
        baseFBM = mix(baseFBM, ridged, u.terrainFeatures.x);
    }

    float f = baseFBM * noiseStr;

    if (u.terrainFeatures.y > 0.0f) {
        float craters = craterNoise(tp * 3.0f, noiseTexture, noiseSampler);
        f += craters * u.terrainFeatures.y * noiseStr * 0.5f;
    }

    if (u.terrainFeatures.z > 0.0f) {
        float continent     = sampleNoise(noiseTexture, noiseSampler, p * u.terrainFeatures.z * 0.15f);
        float continentMask = smoothstep(0.38f, 0.58f, continent);
        f = f * mix(0.15f, 1.0f, continentMask);
    }

    return mix(
        f / 3.0f + noiseStr / 50.0f,
        f,
        smoothstep(sandLevel, sandLevel + transition / 2.0f, f * 5.0f)
    );
}

float planetDist(float3 ro, float3 rd, constant PlanetUniforms& u,
                 texture3d<float> noiseTexture, sampler noiseSampler)
{
    Sphere planet = getPlanet(u);
    float smoothSphereDist = sphIntersect(ro, rd, planet);
    float3 intersection    = ro + smoothSphereDist * rd;
    float3 rotated         = planetRotation(u) * (intersection - planet.position) + planet.position;
    float  n               = planetNoise(rotated, u, noiseTexture, noiseSampler);
    float  coastline       = (u.biomeLevels.x + u.terrainFeatures.w) / 5.0f;
    float  displacement    = max(n, coastline);
    Sphere displaced;
    displaced.position = planet.position;
    displaced.radius   = planet.radius + displacement;
    return sphIntersect(ro, rd, displaced);
}

float3 planetNormal(float3 p, constant PlanetUniforms& u,
                    texture3d<float> noiseTexture, sampler noiseSampler)
{
    float3 rd   = u.planetPos_radius.xyz - p;
    float  dist = planetDist(p, rd, u, noiseTexture, noiseSampler);
    float2 res  = u.resolution_rot.xy;
    float2 e    = float2(max(0.01f, 0.03f * smoothstep(1300.0f, 300.0f, res.x)), 0.0f);
    float3 normal = dist - float3(
        planetDist(p - e.xyy, rd, u, noiseTexture, noiseSampler),
        planetDist(p - e.yxy, rd, u, noiseTexture, noiseSampler),
        planetDist(p + e.yyx, rd, u, noiseTexture, noiseSampler));
    return normalize(normal);
}

// ── Black Hole ────────────────────────────────────────────────────────────────

// Blackbody color approximation for accretion disk temperature.
// Based on Tanner Helland's fitted curves for CIE 1931 → sRGB.
// Input: temperature in Kelvin (1000–40000 K).
float3 blackbodyColor(float tempK) {
    float t = tempK / 100.0f;
    float3 c;

    // Red
    if (t <= 66.0f)
        c.r = 1.0f;
    else
        c.r = 1.2929f * pow(t - 60.0f, -0.1332f);

    // Green
    if (t <= 66.0f)
        c.g = 0.3901f * log(t) - 0.6318f;
    else
        c.g = 1.1299f * pow(t - 60.0f, -0.0755f);

    // Blue
    if (t >= 66.0f)
        c.b = 1.0f;
    else if (t <= 19.0f)
        c.b = 0.0f;
    else
        c.b = 0.5432f * log(t - 10.0f) - 1.1963f;

    return clamp(c, 0.0f, 1.0f);
}

// Core black hole ray tracer using Schwarzschild geodesic integration.
float3 traceBlackHole(float3 ro, float3 rd, constant PlanetUniforms& u,
                      texture3d<float> noiseTexture, sampler noiseSampler);

// ── Stars & space ─────────────────────────────────────────────────────────────

float3 stars(float3 p, float2 resolution, float quality,
             texture3d<float> noiseTexture, sampler noiseSampler)
{
    float3 c   = float3(0.0f);
    float  res = resolution.x * quality * 0.8f;
    for (float i = 0.0f; i < 3.0f; i += 1.0f) {
        float3 q  = fract(p * (0.15f * res)) - 0.5f;
        float3 id = floor(p * (0.15f * res));
        float2 rn = float2(
            sampleNoise(noiseTexture, noiseSampler, id / 2.0f),
            sampleNoise(noiseTexture, noiseSampler, id.zyx * 2.0f)) * 0.03f;
        float c2  = 1.0f - smoothstep(0.0f, 0.6f, length(q));
        c2 *= step(rn.x, 0.003f + i * 0.0005f);
        c += c2 * (mix(float3(1.0f, 0.49f, 0.1f), float3(0.75f, 0.9f, 1.0f), rn.y) * 0.25f + 1.2f);
        p *= 1.8f;
    }
    return c * c;
}

float3 spaceColor(float3 direction, constant PlanetUniforms& u,
                  texture3d<float> noiseTexture, sampler noiseSampler)
{
    float t          = u.cameraPos_time.w;
    float rotSpeed   = u.resolution_rot.w;
    float3x3 bgRot   = rotateY(t * rotSpeed / 4.0f);
    float3 bgCoord   = direction * bgRot;
    float  spaceN    = fbm(bgCoord * 3.0f, 4, 0.5f, 2.0f, 6.0f,
                           u.noiseParams.y, noiseTexture, noiseSampler);
    float2 resolution = u.resolution_rot.xy;
    return stars(bgCoord, resolution, u.noiseParams.y, noiseTexture, noiseSampler)
         + mix(u.deepSpaceColor.xyz, u.atmosphereColor.xyz / 12.0f, spaceN);
}

// Core black hole ray tracer using Schwarzschild geodesic integration.
// Traces a photon path through curved spacetime, accumulating accretion
// disk color at equatorial plane crossings, then composites over the
// gravitationally lensed star background.
float3 traceBlackHole(float3 ro, float3 rd, constant PlanetUniforms& u,
                      texture3d<float> noiseTexture, sampler noiseSampler)
{
    // Schwarzschild radius
    float Rs = u.bhParams1.y * u.planetPos_radius.w * 0.5f;

    // Transform ray into BH-local coordinates (origin at BH center)
    float3 pos = ro - u.planetPos_radius.xyz;
    float3 vel = rd;

    // Conserved specific angular momentum magnitude
    float h = length(cross(pos, vel));

    // Accretion disk bounds
    float rInner = u.bhParams1.z * Rs;
    float rOuter = u.bhParams1.w * Rs;

    // Front-to-back compositing state for accretion disk
    float3 diskColor = float3(0.0f);
    float diskAlpha = 0.0f;

    float prevY = pos.y; // track equatorial plane crossings
    int maxSteps = int(u.bhParams3.z);

    for (int i = 0; i < maxSteps; ++i) {
        float r = length(pos);

        // Captured by event horizon
        if (r < Rs) {
            return diskColor; // black — absorbed
        }

        // Escaped far enough — sample lensed background
        if (r > 100.0f * Rs) {
            float3 bg = spaceColor(normalize(vel), u, noiseTexture, noiseSampler);
            return diskColor + (1.0f - diskAlpha) * bg;
        }

        // Geodesic acceleration: Schwarzschild effective potential
        float r2 = r * r;
        float r5 = r2 * r2 * r;
        float3 accel = -1.5f * h * h * Rs / r5 * pos;

        // Adaptive step size: small near BH, larger far away
        float dt = 0.3f * r / (1.0f + 2.0f * Rs / max(r - Rs, 0.01f));
        dt = clamp(dt, 0.01f * Rs, 2.0f * Rs);

        // Velocity Verlet integration
        float3 newPos = pos + vel * dt + 0.5f * accel * dt * dt;
        float newR = length(newPos);
        float newR5 = newR * newR * newR * newR * newR;
        float3 newAccel = -1.5f * h * h * Rs / newR5 * newPos;
        float3 newVel = vel + 0.5f * (accel + newAccel) * dt;

        // Check equatorial plane crossing (y sign flip)
        if (prevY * newPos.y < 0.0f) {
            // Interpolate to find crossing point
            float t_cross = abs(prevY) / max(abs(prevY) + abs(newPos.y), 1e-6f);
            float3 crossPos = mix(pos, newPos, t_cross);
            float crossR = length(crossPos);

            // Is crossing within the accretion disk annulus?
            if (crossR >= rInner && crossR <= rOuter) {
                // Radial parameter [0,1] from inner to outer edge
                float radialT = (crossR - rInner) / (rOuter - rInner);

                // Temperature gradient: hot inner, cool outer
                float temp = mix(u.bhParams2.w, u.bhParams3.x, radialT);
                float3 bbColor = blackbodyColor(temp);

                // Procedural turbulence using existing noise texture
                float3 noiseCoord = crossPos * 0.5f / Rs;
                // Add time-based rotation for disk orbital motion
                float angle = atan2(crossPos.z, crossPos.x);
                angle += u.cameraPos_time.w * u.bhParams2.x * sqrt(Rs / max(crossR, Rs)) * 0.5f;
                noiseCoord.x = crossR * cos(angle) * 0.5f / Rs;
                noiseCoord.z = crossR * sin(angle) * 0.5f / Rs;
                float turb = sampleNoise(noiseTexture, noiseSampler, noiseCoord);
                turb = mix(1.0f, turb, u.bhParams2.y);

                // Density falls off at inner and outer edges
                float edgeFade = smoothstep(0.0f, 0.15f, radialT)
                               * smoothstep(1.0f, 0.85f, radialT);

                // Doppler beaming: approaching side brighter, receding dimmer
                float3 orbitDir = normalize(cross(float3(0.0f, 1.0f, 0.0f), normalize(crossPos)));
                float orbitalV = u.bhParams2.x * sqrt(Rs / (2.0f * max(crossR, Rs)));
                float doppler = 1.0f + u.bhParams3.y * orbitalV * dot(normalize(vel), orbitDir) * 4.0f;
                doppler = max(doppler, 0.1f);

                // Luminosity: brighter at inner edge (1/r² falloff)
                float luminosity = (rInner / max(crossR, rInner));
                luminosity *= luminosity;

                float3 sampleColor = bbColor * u.bhDiskTint.xyz * turb * edgeFade
                                   * luminosity * doppler * u.bhParams2.z;
                float sampleAlpha = edgeFade * turb * 0.8f;

                // Front-to-back alpha compositing
                diskColor += (1.0f - diskAlpha) * sampleAlpha * sampleColor;
                diskAlpha += (1.0f - diskAlpha) * sampleAlpha;
                diskAlpha = min(diskAlpha, 1.0f);
            }
        }

        prevY = newPos.y;
        pos = newPos;
        vel = newVel;
    }

    // Ray didn't escape or get captured within step limit — treat as escaped
    float3 bg = spaceColor(normalize(vel), u, noiseTexture, noiseSampler);
    return diskColor + (1.0f - diskAlpha) * bg;
}

float3 simpleReinhardToneMapping(float3 color) {
    float exposure = 1.5f;
    color *= exposure / (1.0f + color / exposure);
    color  = pow(color, float3(1.0f / 2.4f));
    return color;
}

// ── Atmosphere ────────────────────────────────────────────────────────────────

float3 atmosphereColor(float3 ro, float3 rd, float spaceMask, constant PlanetUniforms& u) {
    float3 planetPos   = u.planetPos_radius.xyz;
    float  planetRadius= u.planetPos_radius.w;
    float3 camPos      = u.cameraPos_time.xyz;
    float3 sunDir      = u.sunDirection.xyz;
    float  atmoDensity = u.cloudParams2.w;
    float  sunIntensity= u.cloudParams2.y;

    float distCamToOrigin = length(planetPos - camPos);
    float distCamToEdge   = sqrt(distCamToOrigin * distCamToOrigin - planetRadius * planetRadius);

    float  planetMask    = 1.0f - spaceMask;
    float3 coordFromCenter = (ro + rd * distCamToEdge) - planetPos;
    float  distFromEdge  = abs(length(coordFromCenter) - planetRadius);
    float  planetEdge    = max(planetRadius - distFromEdge, 0.0f) / planetRadius;

    float  atmosphereMask = pow(clamp(remap(dot(sunDir, coordFromCenter),
                                            -planetRadius, planetRadius / 2.0f, 0.0f, 1.0f),
                                      0.0f, 1.0f), 5.0f);
    atmosphereMask *= atmoDensity * planetRadius * sunIntensity;

    float3 atmo = float3(pow(planetEdge, 120.0f)) * 0.5f;
    atmo += pow(planetEdge, 50.0f) * 0.3f * (1.5f - planetMask);
    atmo += pow(planetEdge, 15.0f) * 0.03f;
    atmo += pow(planetEdge, 5.0f)  * 0.04f * planetMask;

    return atmo * u.atmosphereColor.xyz * atmosphereMask;
}

// ── Volumetric clouds ─────────────────────────────────────────────────────────

void marchCloudSegment(float3 ro, float3 rd, float tStart, float tEnd, int numSteps,
                       thread float3& scattered, thread float& transmittance,
                       constant PlanetUniforms& u,
                       texture3d<float> noiseTexture, sampler noiseSampler)
{
    if (tStart >= tEnd || transmittance < 0.01f) return;
    float stepSize = (tEnd - tStart) / float(numSteps);

    float  quality      = u.noiseParams.y;
    float  cloudDensity = u.cloudParams1.x;
    float  cloudScale   = u.cloudParams1.y;
    float  cloudSpeed   = u.cloudParams1.z;
    float  cloudAlt     = u.cloudParams1.w;
    float  cloudThick   = u.cloudParams2.x;
    float  sunInt       = u.cloudParams2.y;
    float  ambLight     = u.cloudParams2.z;
    float3 planetPos    = u.planetPos_radius.xyz;
    float  planetRadius = u.planetPos_radius.w;
    float  t            = u.cameraPos_time.w;
    float3 sunDir       = u.sunDirection.xyz;

    for (int i = 0; i < numSteps; ++i) {
        if (transmittance < 0.01f) break;

        float  sampleT  = tStart + (float(i) + 0.5f) * stepSize;
        float3 pos      = ro + rd * sampleT;
        float3 localPos = pos - planetPos;

        float alt      = length(localPos) - planetRadius;
        float heightFrac = (alt - cloudAlt) / cloudThick;

        float3 rotated   = planetRotation(u) * localPos + planetPos;
        float3 cloudCoord= (rotated + float3(t * 0.008f * cloudSpeed)) * cloudScale;
        float  density   = cloudNoise(cloudCoord, quality, noiseTexture, noiseSampler);

        density *= smoothstep(0.0f, 0.25f, heightFrac) * smoothstep(1.0f, 0.75f, heightFrac);

        float threshold = 1.0f - cloudDensity * 0.5f;
        density = smoothstep(threshold, threshold + 0.1f, density);

        if (density < 0.001f) continue;

        float3 normal = normalize(localPos);
        float  NdotL  = clamp(dot(normal, sunDir), 0.0f, 1.0f);
        float  cosTheta = dot(rd, sunDir);
        float  phase    = mix(0.5f, 2.5f, pow(clamp(cosTheta * 0.5f + 0.5f, 0.0f, 1.0f), 3.0f));

        float shadowDensity = 0.0f;
        if (quality >= 1.0f) {
            float3 sp     = pos + sunDir * cloudThick * 0.5f;
            float3 sLocal = sp - planetPos;
            float  sAlt   = length(sLocal) - planetRadius;
            float  sH     = (sAlt - cloudAlt) / cloudThick;
            if (sH >= 0.0f && sH <= 1.0f) {
                float3 sRot   = planetRotation(u) * sLocal + planetPos;
                float3 sCoord = (sRot + float3(t * 0.008f * cloudSpeed)) * cloudScale;
                float  sd     = cloudNoiseCheap(sCoord, quality, noiseTexture, noiseSampler);
                sd *= smoothstep(0.0f, 0.25f, sH) * smoothstep(1.0f, 0.75f, sH);
                sd = smoothstep(threshold, threshold + 0.1f, sd);
                shadowDensity = sd;
            }
        }
        float sunTransmittance = exp(-shadowDensity * 3.0f);

        float  absorption = density * stepSize * 18.0f;
        float3 sunLight   = u.sunColor.xyz * sunInt * NdotL * phase * sunTransmittance;
        float3 ambient    = u.atmosphereColor.xyz * 0.08f;
        float3 luminance  = u.cloudColor.xyz * (sunLight + ambient);
        luminance += u.sunColor.xyz * phase * 0.3f * sunInt * sunTransmittance;

        scattered    += luminance * transmittance * (1.0f - exp(-absorption));
        transmittance *= exp(-absorption);
    }
}

float4 volumetricClouds(float3 ro, float3 rd, float surfaceDist,
                        constant PlanetUniforms& u,
                        texture3d<float> noiseTexture, sampler noiseSampler)
{
    if (u.cloudParams1.x <= 0.0f) return float4(0.0f);

    float  planetRadius = u.planetPos_radius.w;
    float  cloudAlt     = u.cloudParams1.w;
    float  cloudThick   = u.cloudParams2.x;
    float  cloudInnerR  = planetRadius + cloudAlt;
    float  cloudOuterR  = cloudInnerR + cloudThick;

    Sphere inner; inner.position = u.planetPos_radius.xyz; inner.radius = cloudInnerR;
    Sphere outer; outer.position = u.planetPos_radius.xyz; outer.radius = cloudOuterR;

    float2 tOuter = sphIntersect2(ro, rd, outer);
    if (tOuter.x < 0.0f) return float4(0.0f);

    float2 tInner = sphIntersect2(ro, rd, inner);

    float frontStart = max(tOuter.x, 0.0f);
    float frontEnd   = (tInner.x > 0.0f) ? tInner.x : tOuter.y;
    frontEnd = min(frontEnd, surfaceDist);

    float backStart = -1.0f, backEnd = -1.0f;
    if (tInner.y > 0.0f && tInner.y < tOuter.y) {
        backStart = max(tInner.y, surfaceDist);
        backEnd   = tOuter.y;
    }

    int    numSteps      = 4 + int(u.noiseParams.y) * 6;
    float3 scattered     = float3(0.0f);
    float  transmittance = 1.0f;

    marchCloudSegment(ro, rd, frontStart, frontEnd, numSteps,
                      scattered, transmittance, u, noiseTexture, noiseSampler);
    if (backStart > 0.0f) {
        marchCloudSegment(ro, rd, backStart, backEnd, numSteps / 2,
                          scattered, transmittance, u, noiseTexture, noiseSampler);
    }
    return float4(scattered, 1.0f - transmittance);
}

// ── Surface intersection ──────────────────────────────────────────────────────

Hit intersectPlanet(float3 ro, float3 rd, constant PlanetUniforms& u,
                    texture3d<float> noiseTexture, sampler noiseSampler)
{
    Hit miss = makeMiss();
    float len = sphIntersect(ro, rd, getPlanet(u));
    if (len < 0.0f) return miss;

    float3 position   = ro + len * rd;
    float3 rotated    = planetRotation(u) * (position - u.planetPos_radius.xyz) + u.planetPos_radius.xyz;
    float  rawNoise   = planetNoise(rotated, u, noiseTexture, noiseSampler);
    float3 normal     = planetNormal(position, u, noiseTexture, noiseSampler);

    float sandLevel  = u.biomeLevels.x;
    float waterLevel = u.terrainFeatures.w;
    float transition = u.transitionPad.x;
    float coastline  = (sandLevel + waterLevel) / 5.0f;
    float landMask   = smoothstep(coastline - 0.001f, coastline + 0.001f, rawNoise);

    float  waterDepth = clamp(rawNoise / max(coastline, 0.001f), 0.0f, 1.0f);
    float3 waterColor = mix(u.waterColorDeep.xyz, u.waterColorSurface.xyz, waterDepth);

    float  altitude   = 5.0f * rawNoise;
    float3 landColor  = u.sandColor.xyz;
    landColor = mix(landColor, u.treeColor.xyz, smoothstep(u.biomeLevels.y, u.biomeLevels.y + transition, altitude));
    landColor = mix(landColor, u.rockColor.xyz, smoothstep(u.biomeLevels.z, u.biomeLevels.z + transition, altitude));
    landColor = mix(landColor, u.iceColor.xyz,  smoothstep(u.biomeLevels.w, u.biomeLevels.w + transition, altitude));

    float3 color = mix(waterColor, landColor, landMask);

    float3 localDir = normalize(rotated - u.planetPos_radius.xyz);
    float  latitude = abs(localDir.y);

    float bandStr  = u.bandingPolar.x;
    float bandFreq = u.bandingPolar.y;
    if (bandStr > 0.0f) {
        float band      = sin(localDir.y * bandFreq) * 0.5f + 0.5f;
        float bandNoise = sampleNoise(noiseTexture, noiseSampler, rotated * 2.0f) * 0.3f;
        band = clamp(band + bandNoise, 0.0f, 1.0f);
        color = mix(color, color * (0.6f + 0.8f * band), bandStr);
    }

    float polarCap = u.bandingPolar.z;
    if (polarCap > 0.0f) {
        float polarEdge = 1.0f - polarCap;
        float polarMask = smoothstep(polarEdge, polarEdge + 0.1f, latitude);
        color = mix(color, u.iceColor.xyz, polarMask);
    }

    float specular = 1.0f - landMask;
    Hit hit;
    hit.len              = len;
    hit.normal           = normal;
    hit.material.color   = color;
    hit.material.diffuse  = 1.0f;
    hit.material.specular = specular;
    return hit;
}

// ── Radiance ──────────────────────────────────────────────────────────────────

float3 radiance(float3 ro, float3 rd, constant PlanetUniforms& u,
                texture3d<float> noiseTexture, sampler noiseSampler)
{
    // Black hole mode: skip planet rendering entirely
    if (u.bhParams1.x > 0.5f) {
        return traceBlackHole(ro, rd, u, noiseTexture, noiseSampler);
    }

    float3 color     = float3(0.0f);
    float  spaceMask = 1.0f;
    Hit    hit       = intersectPlanet(ro, rd, u, noiseTexture, noiseSampler);

    float sunInt   = u.cloudParams2.y;
    float ambLight = u.cloudParams2.z;
    float3 sunDir  = u.sunDirection.xyz;

    if (hit.len < PLANET_INFINITY) {
        spaceMask = 0.0f;
        float  directLightIntensity = pow(clamp(dot(hit.normal, sunDir), 0.0f, 1.0f), 2.0f) * sunInt;
        float3 diffuseLight         = directLightIntensity * u.sunColor.xyz;
        float3 diffuseColor         = hit.material.color * (ambLight + diffuseLight);

        float3 reflected  = normalize(reflect(-sunDir, hit.normal));
        float  phongValue = pow(max(0.0f, dot(rd, reflected)), 10.0f) * 0.2f * sunInt;
        float3 specular   = hit.material.specular * float3(phongValue);

        color = diffuseColor + specular;
    } else {
        color = spaceColor(rd, u, noiseTexture, noiseSampler);
    }

    float4 clouds = volumetricClouds(ro, rd, hit.len, u, noiseTexture, noiseSampler);
    color = color * (1.0f - clouds.a) + clouds.rgb;

    return color + atmosphereColor(ro, rd, spaceMask, u);
}

// ── Fragment entry point ──────────────────────────────────────────────────────

fragment float4 planetFragment(VertexOut in           [[stage_in]],
                                constant PlanetUniforms& u  [[buffer(0)]],
                                texture3d<float> noiseTexture [[texture(0)]],
                                sampler noiseSampler          [[sampler(0)]])
{
    float2 screenUV = in.uv;
    screenUV.x *= u.resolution_rot.x / u.resolution_rot.y;  // aspect correct

    float3 ro = u.cameraPos_time.xyz;
    float3 rd = normalize(float3(screenUV, -1.0f));
    rd = (u.invView * float4(rd, 0.0f)).xyz;

    float3 color = radiance(ro, rd, u, noiseTexture, noiseSampler);

    color = simpleReinhardToneMapping(color);
    color *= 1.0f - 0.5f * pow(length(screenUV), 3.0f);

    return float4(color, 1.0f);
}
