#version 410 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vColor;
in float vEmissive;
in vec3 vAtmosphereColor;
in float vHasAtmosphere;
in vec3 vViewDir;
in float vDistToCamera;

uniform vec3 uSunDir;
uniform float uSunIntensity;
uniform vec3 uSunPos;

out vec4 FragColor;

void main() {
    vec3 normal = normalize(vNormal);
    vec3 viewDir = normalize(vViewDir);

    // Base lighting
    float NdotL = max(dot(normal, uSunDir), 0.0);
    float ambient = 0.08;

    // Specular highlight
    vec3 halfVec = normalize(uSunDir + viewDir);
    float spec = pow(max(dot(normal, halfVec), 0.0), 32.0) * 0.5;

    // Fresnel rim lighting - makes edges visible against dark space
    float fresnel = pow(1.0 - max(dot(normal, viewDir), 0.0), 3.0);

    vec3 color;

    if (vEmissive > 0.5) {
        // Emissive body (star/sun)
        // Core is bright white-yellow, edges have color glow
        vec3 coreColor = mix(vColor, vec3(1.0, 1.0, 0.95), 0.7);
        vec3 edgeColor = vColor * 1.5;

        // Bright core, colored edges
        color = mix(coreColor, edgeColor, fresnel);

        // Add bloom-like glow at edges
        color += vColor * fresnel * 2.0;

        // Always full bright
        color = max(color, vColor);
    } else {
        // Non-emissive body (planet/moon)
        float lighting = ambient + NdotL * uSunIntensity;
        color = vColor * lighting;

        // Add specular
        color += vec3(1.0) * spec * NdotL;

        // Atmosphere glow
        if (vHasAtmosphere > 0.5) {
            // Atmosphere scattering at the edges
            vec3 atmosGlow = vAtmosphereColor * fresnel * 0.8;

            // Backlit atmosphere effect (light shining through)
            float backlit = max(dot(normal, -uSunDir), 0.0);
            atmosGlow += vAtmosphereColor * backlit * fresnel * 0.5;

            color += atmosGlow;
        }

        // Subtle rim light to separate from black space
        color += vec3(0.1, 0.15, 0.2) * fresnel * 0.3;
    }

    // Tone mapping to prevent blowout
    color = color / (color + vec3(1.0));

    // Slight gamma correction
    color = pow(color, vec3(0.9));

    FragColor = vec4(color, 1.0);
}
