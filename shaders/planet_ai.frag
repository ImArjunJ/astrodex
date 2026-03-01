#version 410 core

in vec3 v_worldPos;
in vec3 v_normal;
in vec3 v_localPos;

out vec4 frag_color;

// AI-generated texture (2D)
uniform sampler2D u_planetTexture;

// Camera and lighting
uniform vec3 u_cameraPos;
uniform vec3 u_sunDirection;
uniform float u_sunIntensity;

const float PI = 3.14159265359;

// Convert 3D direction to equirectangular UV
vec2 dirToEquirect(vec3 dir) {
    float u = atan(dir.z, dir.x) / (2.0 * PI) + 0.5;
    float v = asin(clamp(dir.y, -1.0, 1.0)) / PI + 0.5;
    return vec2(u, v);
}

void main() {
    vec3 N = normalize(v_normal);
    vec3 V = normalize(u_cameraPos - v_worldPos);
    vec3 L = normalize(u_sunDirection);

    // Sample texture using spherical mapping
    vec3 sphereDir = normalize(v_localPos);
    vec2 uv = dirToEquirect(sphereDir);
    vec3 albedo = texture(u_planetTexture, uv).rgb;

    // Convert from sRGB to linear for correct lighting
    albedo = pow(albedo, vec3(2.2));

    // Simple diffuse lighting
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);

    // Ambient + diffuse
    vec3 color = albedo * (NdotL * u_sunIntensity * 0.9 + 0.15);

    // Subtle specular for oceans (detected by blue-ish color)
    float isOcean = step(0.5, albedo.b / (albedo.r + albedo.g + albedo.b + 0.001));
    vec3 H = normalize(V + L);
    float spec = pow(max(dot(N, H), 0.0), 64.0) * isOcean * 0.3;
    color += vec3(1.0) * spec * NdotL;

    // Atmospheric rim glow
    float rim = pow(1.0 - NdotV, 3.5);
    vec3 rimColor = vec3(0.4, 0.6, 1.0);
    color += rimColor * rim * 0.35 * (NdotL + 0.3);

    // Tone mapping
    color = color / (color + vec3(1.0));

    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));

    frag_color = vec4(color, 1.0);
}
