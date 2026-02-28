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

// Extra color uniforms
uniform vec3 uCloudColor;
uniform vec3 uSunColor;
uniform vec3 uDeepSpaceColor;

// Terrain diversity
uniform int uFbmOctaves;
uniform float uFbmPersistence;
uniform float uFbmLacunarity;
uniform float uFbmExponentiation;
uniform float uDomainWarpStrength;
uniform float uRidgedStrength;
uniform float uCraterStrength;
uniform float uContinentScale;
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
    color *= 1. - 0.5 * pow(length(screenUV), 3.);

    fragColor = vec4(color, 1.0);
}
