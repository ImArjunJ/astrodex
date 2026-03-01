#version 410 core

in vec3 v_worldPos;
in vec3 v_normal;
in vec3 v_localPos;

out vec4 frag_color;

// Textures
uniform samplerCube u_albedo;
uniform samplerCube u_normalMap;
uniform samplerCube u_roughness;
uniform samplerCube u_clouds;

// Camera
uniform vec3 u_cameraPos;

// Lighting
uniform vec3 u_sunDirection;
uniform vec3 u_sunColor;
uniform float u_sunIntensity;

// Planet params
uniform float u_cloudDensity;

const float PI = 3.14159265359;

// PBR Functions
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Simplified atmospheric effect

void main() {
    vec3 N = normalize(v_normal);
    vec3 V = normalize(u_cameraPos - v_worldPos);
    vec3 L = normalize(u_sunDirection);

    // Sample textures using local position as cubemap direction
    vec3 cubeDir = normalize(v_localPos);
    vec3 albedo = texture(u_albedo, cubeDir).rgb;
    float clouds = texture(u_clouds, cubeDir).r * u_cloudDensity;

    // Simple diffuse lighting
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);

    // Basic lighting
    vec3 color = albedo * (NdotL * 0.8 + 0.2);

    // Cloud layer
    vec3 cloudColor = vec3(1.0) * (NdotL * 0.7 + 0.3);
    color = mix(color, cloudColor, clouds * 0.6);

    // Atmospheric rim glow
    float rim = pow(1.0 - NdotV, 3.0);
    color += vec3(0.3, 0.5, 1.0) * rim * 0.25 * (NdotL + 0.3);

    // Gamma correction
    color = pow(clamp(color, 0.0, 1.0), vec3(1.0 / 2.2));

    frag_color = vec4(color, 1.0);
}
