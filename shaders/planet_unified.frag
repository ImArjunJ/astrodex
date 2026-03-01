#version 410 core

// =============================================================================
// AstroCore Unified Planet Shader
// Based on Julien Sulpis's approach - sphere intersection with noise displacement
// =============================================================================

in vec2 v_uv;
out vec4 frag_color;

// Body types
const int BODY_TERRESTRIAL = 0;
const int BODY_GAS_GIANT = 1;
const int BODY_ICE_GIANT = 2;
const int BODY_STAR = 3;
const int BODY_MOON = 5;

// Camera
uniform mat4 u_invView;
uniform mat4 u_invProjection;
uniform vec3 u_cameraPos;
uniform float u_aspectRatio;
uniform float u_time;

// Body
uniform int u_bodyType;
uniform float u_radius;
uniform uint u_seed;

// Terrain
uniform float u_seaLevel;
uniform float u_heightScale;
uniform float u_terrainFrequency;

// Biome
uniform float u_polarIceExtent;
uniform float u_glacierAltitude;
uniform float u_treelineAltitude;

// Colors
uniform vec3 u_colorOceanDeep;
uniform vec3 u_colorOceanShallow;
uniform vec3 u_colorBeach;
uniform vec3 u_colorLowlandGrass;
uniform vec3 u_colorHighland;
uniform vec3 u_colorMountain;
uniform vec3 u_colorSnow;
uniform vec3 u_colorIce;
uniform vec3 u_colorRock;

// Clouds
uniform float u_cloudCoverage;
uniform float u_cloudDensity;

// Atmosphere
uniform vec3 u_rayleighCoeff;
uniform float u_rayleighScale;
uniform float u_atmosphereHeight;
uniform float u_atmosphereDensity;

// Gas giant
uniform float u_bandCount;
uniform float u_bandContrast;
uniform float u_bandTurbulence;
uniform vec3 u_bandColor1;
uniform vec3 u_bandColor2;
uniform vec3 u_stormColor;
uniform float u_stormFrequency;
uniform float u_greatSpotSize;
uniform vec2 u_greatSpotPosition;
uniform vec3 u_greatSpotColor;

// Star
uniform float u_starTemperature;
uniform float u_limbDarkening;
uniform float u_granulationScale;
uniform float u_granulationContrast;
uniform float u_luminosity;

// Lighting
uniform vec3 u_sunDirection;
uniform vec3 u_sunColor;
uniform float u_sunIntensity;
uniform float u_ambientIntensity;

// Constants
const float PI = 3.14159265359;
const float EPSILON = 0.001;
const float INFINITY = 1e10;

// =============================================================================
// NOISE - Simplex 3D
// =============================================================================

vec4 permute(vec4 x) { return mod(((x * 34.0) + 1.0) * x, 289.0); }
vec4 taylorInvSqrt(vec4 r) { return 1.79284291400159 - 0.85373472095314 * r; }

float snoise(vec3 v) {
    const vec2 C = vec2(1.0/6.0, 1.0/3.0);
    const vec4 D = vec4(0.0, 0.5, 1.0, 2.0);

    vec3 i = floor(v + dot(v, C.yyy));
    vec3 x0 = v - i + dot(i, C.xxx);
    vec3 g = step(x0.yzx, x0.xyz);
    vec3 l = 1.0 - g;
    vec3 i1 = min(g.xyz, l.zxy);
    vec3 i2 = max(g.xyz, l.zxy);
    vec3 x1 = x0 - i1 + C.xxx;
    vec3 x2 = x0 - i2 + C.yyy;
    vec3 x3 = x0 - D.yyy;

    i = mod(i, 289.0);
    vec4 p = permute(permute(permute(
        i.z + vec4(0.0, i1.z, i2.z, 1.0))
        + i.y + vec4(0.0, i1.y, i2.y, 1.0))
        + i.x + vec4(0.0, i1.x, i2.x, 1.0));

    float n_ = 1.0/7.0;
    vec3 ns = n_ * D.wyz - D.xzx;
    vec4 j = p - 49.0 * floor(p * ns.z * ns.z);
    vec4 x_ = floor(j * ns.z);
    vec4 y_ = floor(j - 7.0 * x_);
    vec4 x = x_ * ns.x + ns.yyyy;
    vec4 y = y_ * ns.x + ns.yyyy;
    vec4 h = 1.0 - abs(x) - abs(y);
    vec4 b0 = vec4(x.xy, y.xy);
    vec4 b1 = vec4(x.zw, y.zw);
    vec4 s0 = floor(b0) * 2.0 + 1.0;
    vec4 s1 = floor(b1) * 2.0 + 1.0;
    vec4 sh = -step(h, vec4(0.0));
    vec4 a0 = b0.xzyw + s0.xzyw * sh.xxyy;
    vec4 a1 = b1.xzyw + s1.xzyw * sh.zzww;
    vec3 p0 = vec3(a0.xy, h.x);
    vec3 p1 = vec3(a0.zw, h.y);
    vec3 p2 = vec3(a1.xy, h.z);
    vec3 p3 = vec3(a1.zw, h.w);
    vec4 norm = taylorInvSqrt(vec4(dot(p0,p0), dot(p1,p1), dot(p2,p2), dot(p3,p3)));
    p0 *= norm.x; p1 *= norm.y; p2 *= norm.z; p3 *= norm.w;
    vec4 m = max(0.6 - vec4(dot(x0,x0), dot(x1,x1), dot(x2,x2), dot(x3,x3)), 0.0);
    m = m * m;
    return 42.0 * dot(m*m, vec4(dot(p0,x0), dot(p1,x1), dot(p2,x2), dot(p3,x3)));
}

// FBM with 4 octaves
float fbm(vec3 p) {
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;

    for (int i = 0; i < 4; i++) {
        value += amplitude * (snoise(p * frequency) * 0.5 + 0.5);
        amplitude *= 0.5;
        frequency *= 2.0;
    }
    return value;
}

// Domain warping for organic shapes
float warpedFbm(vec3 p) {
    vec3 q = vec3(fbm(p), fbm(p + vec3(5.2, 1.3, 2.8)), 0.0);
    return fbm(p + q * 0.5);
}

// =============================================================================
// SPHERE INTERSECTION
// =============================================================================

float sphIntersect(vec3 ro, vec3 rd, vec3 center, float radius) {
    vec3 oc = ro - center;
    float b = dot(oc, rd);
    float c = dot(oc, oc) - radius * radius;
    float h = b * b - c;
    if (h < 0.0) return -1.0;
    return -b - sqrt(h);
}

// =============================================================================
// TERRAIN NOISE
// =============================================================================

mat3 rotateY(float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return mat3(c, 0, s, 0, 1, 0, -s, 0, c);
}

float planetNoise(vec3 p) {
    vec3 seedOffset = vec3(float(u_seed) * 0.1);
    vec3 coord = p + seedOffset;

    float terrain = warpedFbm(coord * u_terrainFrequency);
    terrain = pow(terrain, 1.5) * u_heightScale;

    // Flatten oceans
    float seaLevelNoise = u_seaLevel * u_heightScale;
    if (terrain < seaLevelNoise) {
        terrain = mix(seaLevelNoise * 0.8, terrain, 0.3);
    }

    return terrain;
}

// =============================================================================
// TERRESTRIAL COLORING
// =============================================================================

vec3 getTerrestrialColor(vec3 position, vec3 normal, float altitude) {
    float latitude = abs(normal.y);
    float normalizedAlt = altitude / u_heightScale;
    float seaLevelNorm = u_seaLevel;

    // Ocean
    if (normalizedAlt < seaLevelNorm) {
        float depth = (seaLevelNorm - normalizedAlt) / seaLevelNorm;
        return mix(u_colorOceanShallow, u_colorOceanDeep, depth);
    }

    float landAlt = (normalizedAlt - seaLevelNorm) / (1.0 - seaLevelNorm);

    // Polar ice
    if (latitude > 1.0 - u_polarIceExtent) {
        float ice = smoothstep(1.0 - u_polarIceExtent, 0.95, latitude);
        return mix(u_colorSnow * 0.9, u_colorIce, ice);
    }

    // Color transitions based on altitude
    vec3 color = u_colorBeach;
    color = mix(color, u_colorLowlandGrass, smoothstep(0.0, 0.05, landAlt));
    color = mix(color, u_colorHighland, smoothstep(0.15, u_treelineAltitude, landAlt));
    color = mix(color, u_colorMountain, smoothstep(u_treelineAltitude, u_glacierAltitude, landAlt));
    color = mix(color, u_colorSnow, smoothstep(u_glacierAltitude, 1.0, landAlt));

    // Variation
    float variation = snoise(position * 10.0) * 0.1;
    color *= 1.0 + variation;

    return color;
}

float getClouds(vec3 position) {
    if (u_cloudCoverage < 0.01) return 0.0;

    vec3 cloudCoord = position * 3.0 + vec3(u_time * 0.02, 0.0, 0.0);
    float clouds = warpedFbm(cloudCoord);

    float threshold = 1.0 - u_cloudCoverage * 0.5;
    clouds = smoothstep(threshold, threshold + 0.15, clouds);

    // Less clouds at poles
    float latitude = abs(normalize(position).y);
    clouds *= smoothstep(0.9, 0.7, latitude);

    return clouds * u_cloudDensity;
}

// =============================================================================
// GAS GIANT
// =============================================================================

vec3 getGasGiantColor(vec3 normal) {
    float latitude = asin(normal.y) / (PI * 0.5);

    // Bands
    float bands = sin(latitude * PI * u_bandCount);
    bands += snoise(normal * 8.0) * u_bandTurbulence;
    bands = bands * 0.5 + 0.5;

    vec3 color = mix(u_bandColor1, u_bandColor2, bands);

    // Storms
    if (u_stormFrequency > 0.01) {
        float storm = snoise(normal * 12.0);
        storm = smoothstep(0.6, 0.8, storm) * u_stormFrequency;
        color = mix(color, u_stormColor, storm);
    }

    // Great spot
    if (u_greatSpotSize > 0.01) {
        vec3 spotDir = vec3(
            cos(u_greatSpotPosition.x) * cos(u_greatSpotPosition.y),
            sin(u_greatSpotPosition.y),
            sin(u_greatSpotPosition.x) * cos(u_greatSpotPosition.y)
        );
        float dist = acos(clamp(dot(normal, spotDir), -1.0, 1.0));
        float spot = 1.0 - smoothstep(0.0, u_greatSpotSize * PI, dist);
        color = mix(color, u_greatSpotColor, spot);
    }

    // Depth variation
    color *= 0.85 + fbm(normal * 15.0) * 0.3;

    return color;
}

// =============================================================================
// STAR
// =============================================================================

vec3 blackbodyColor(float temp) {
    float t = temp / 100.0;
    vec3 color;
    color.r = t <= 66.0 ? 1.0 : 1.29293618606 * pow(t - 60.0, -0.1332047592);
    color.g = t <= 66.0 ? 0.390081578769 * log(t) - 0.631841443788 : 1.12989086089 * pow(t - 60.0, -0.0755148492);
    color.b = t >= 66.0 ? 1.0 : (t <= 19.0 ? 0.0 : 0.543206789110 * log(t - 10.0) - 1.19625408914);
    return clamp(color, 0.0, 1.0);
}

vec3 getStarColor(vec3 normal, vec3 viewDir) {
    vec3 baseColor = blackbodyColor(u_starTemperature);

    // Limb darkening
    float mu = max(dot(normal, -viewDir), 0.0);
    float limb = 1.0 - u_limbDarkening * (1.0 - mu);

    // Granulation
    float gran = fbm(normal * (1.0 / u_granulationScale)) * u_granulationContrast;

    return baseColor * u_luminosity * limb * (1.0 + gran);
}

// =============================================================================
// ATMOSPHERE
// =============================================================================

vec3 atmosphereGlow(vec3 ro, vec3 rd, float hitDist) {
    if (u_atmosphereDensity < 0.01) return vec3(0.0);

    vec3 planetCenter = vec3(0.0);
    float atmosRadius = u_radius * (1.0 + u_atmosphereHeight);

    // Distance from ray to planet center
    float distToCenter = length(ro - planetCenter);
    float closestApproach = length(cross(rd, ro - planetCenter));

    // Glow based on how close ray passes to planet edge
    float edgeDist = closestApproach - u_radius;
    float atmosThickness = atmosRadius - u_radius;

    if (edgeDist > atmosThickness) return vec3(0.0);
    if (hitDist > 0.0) return vec3(0.0); // Hit planet, no limb glow

    float glow = 1.0 - edgeDist / atmosThickness;
    glow = pow(glow, 3.0);

    // Scatter towards sun
    float sunDot = dot(rd, u_sunDirection);
    float scatter = 0.5 + sunDot * 0.5;

    vec3 atmosColor = u_rayleighCoeff * u_rayleighScale * 1e6;
    atmosColor = normalize(atmosColor + 0.1) * 0.5;

    return atmosColor * glow * scatter * u_atmosphereDensity * u_sunIntensity;
}

vec3 atmosphereScatter(vec3 normal) {
    float NdotL = dot(normal, u_sunDirection);
    float scatter = smoothstep(-0.2, 0.3, NdotL);

    vec3 atmosColor = u_rayleighCoeff * u_rayleighScale * 5e5;
    atmosColor = normalize(atmosColor + 0.1);

    // Fresnel-like rim
    return atmosColor * scatter * u_atmosphereDensity * 0.15;
}

// =============================================================================
// STARS BACKGROUND
// =============================================================================

vec3 starsBackground(vec3 rd) {
    float stars = pow(max(snoise(rd * 500.0), 0.0), 20.0);
    stars += pow(max(snoise(rd * 250.0 + vec3(100.0)), 0.0), 15.0) * 0.5;
    return vec3(stars);
}

// =============================================================================
// TONE MAPPING
// =============================================================================

vec3 toneMap(vec3 color) {
    float exposure = 1.5;
    color *= exposure / (1.0 + color / exposure);
    return pow(color, vec3(1.0 / 2.2));
}

// =============================================================================
// MAIN
// =============================================================================

void main() {
    vec2 ndc = v_uv * 2.0 - 1.0;
    ndc.x *= u_aspectRatio;

    vec3 ro = u_cameraPos;
    vec4 rayClip = vec4(ndc, -1.0, 1.0);
    vec4 rayView = u_invProjection * rayClip;
    rayView = vec4(rayView.xy, -1.0, 0.0);
    vec3 rd = normalize((u_invView * rayView).xyz);

    vec3 planetCenter = vec3(0.0);
    vec3 color = vec3(0.0);

    // Sphere intersection
    float dist = sphIntersect(ro, rd, planetCenter, u_radius);

    if (dist > 0.0) {
        vec3 hitPos = ro + rd * dist;
        vec3 normal = normalize(hitPos - planetCenter);

        // Apply rotation
        mat3 rotation = rotateY(u_time * 0.1);
        vec3 rotatedPos = rotation * hitPos;
        vec3 rotatedNormal = rotation * normal;

        // Get altitude from noise
        float altitude = planetNoise(rotatedPos);

        // Get color based on body type
        if (u_bodyType == BODY_TERRESTRIAL) {
            color = getTerrestrialColor(rotatedPos, rotatedNormal, altitude);

            // Clouds
            float clouds = getClouds(rotatedPos);
            float cloudLight = max(dot(normal, u_sunDirection), 0.0) * 0.5 + 0.5;
            color = mix(color, vec3(cloudLight), clouds);

        } else if (u_bodyType == BODY_GAS_GIANT || u_bodyType == BODY_ICE_GIANT) {
            color = getGasGiantColor(rotatedNormal);

        } else if (u_bodyType == BODY_STAR) {
            color = getStarColor(normal, rd);

        } else if (u_bodyType == BODY_MOON) {
            // Simple rocky moon
            float moonNoise = fbm(rotatedPos * 5.0);
            color = u_colorRock * (0.7 + moonNoise * 0.6);
        }

        // Lighting (not for stars)
        if (u_bodyType != BODY_STAR) {
            float NdotL = max(dot(normal, u_sunDirection), 0.0);
            float terminator = smoothstep(-0.1, 0.2, dot(normal, u_sunDirection));

            vec3 ambient = vec3(u_ambientIntensity);
            vec3 diffuse = u_sunColor * u_sunIntensity * NdotL * terminator;

            color = color * (ambient + diffuse);

            // Ocean specular
            float normalizedAlt = altitude / u_heightScale;
            if (normalizedAlt < u_seaLevel && u_bodyType == BODY_TERRESTRIAL) {
                vec3 h = normalize(u_sunDirection - rd);
                float spec = pow(max(dot(normal, h), 0.0), 64.0);
                color += spec * u_sunColor * 0.5 * NdotL;
            }

            // Atmosphere scatter on surface
            if (u_atmosphereDensity > 0.01) {
                vec3 scatter = atmosphereScatter(normal);
                color += scatter * terminator;
            }
        }
    } else {
        // Space background
        color = starsBackground(rd);
    }

    // Atmosphere limb glow
    if (u_bodyType != BODY_STAR) {
        color += atmosphereGlow(ro, rd, dist);
    }

    // Tone mapping
    color = toneMap(color);

    // Vignette
    float vignette = 1.0 - 0.3 * pow(length(v_uv - 0.5) * 1.5, 2.0);
    color *= vignette;

    frag_color = vec4(color, 1.0);
}
