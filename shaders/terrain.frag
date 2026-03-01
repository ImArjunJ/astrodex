#version 410 core

in TES_OUT {
    vec3 worldPos;
    vec3 normal;
    vec3 sphereDir;
    float height;
} fs_in;

out vec4 fragColor;

uniform vec3 uCameraPosition;
uniform vec3 uSunDirection;
uniform float uSunIntensity;
uniform float uAmbientLight;
uniform vec3 uSunColor;
uniform vec3 uPlanetCenter;
uniform float uPlanetRadius;
uniform float uNoiseStrength;
uniform float uWaterLevel;

uniform vec3 uWaterColorDeep;
uniform vec3 uWaterColorSurface;
uniform vec3 uSandColor;
uniform vec3 uTreeColor;
uniform vec3 uRockColor;
uniform vec3 uIceColor;
uniform vec3 uAtmosphereColor;
uniform float uAtmosphereDensity;
uniform float uPolarCapSize;

void main() {
    vec3 N = normalize(fs_in.normal);
    vec3 V = normalize(uCameraPosition - fs_in.worldPos);
    vec3 L = normalize(uSunDirection);

    // Height-based color
    float h = fs_in.height;
    float maxH = uNoiseStrength;

    vec3 color;
    if (h < uWaterLevel * maxH * 0.5) {
        // Water
        color = mix(uWaterColorDeep, uWaterColorSurface, h / (uWaterLevel * maxH * 0.5 + 0.001));
    } else {
        // Land - simple gradient
        float t = clamp((h - uWaterLevel * maxH * 0.5) / (maxH - uWaterLevel * maxH * 0.5 + 0.001), 0.0, 1.0);

        if (t < 0.2) {
            color = mix(uSandColor, uTreeColor, t * 5.0);
        } else if (t < 0.5) {
            color = mix(uTreeColor, uRockColor, (t - 0.2) / 0.3);
        } else {
            color = mix(uRockColor, uIceColor, (t - 0.5) / 0.5);
        }
    }

    // Polar caps
    float lat = abs(fs_in.sphereDir.y);
    if (lat > 1.0 - uPolarCapSize) {
        float polarT = (lat - (1.0 - uPolarCapSize)) / uPolarCapSize;
        color = mix(color, uIceColor, polarT);
    }

    // Lighting
    float NdotL = max(dot(N, L), 0.0);
    float diffuse = NdotL * uSunIntensity;

    vec3 finalColor = color * (uAmbientLight + diffuse) * uSunColor;

    // Rim light
    float rim = pow(1.0 - max(dot(N, V), 0.0), 3.0);
    finalColor += rim * uAtmosphereColor * uAtmosphereDensity * 0.3;

    // Tone map
    finalColor = finalColor / (finalColor + vec3(1.0));
    finalColor = pow(finalColor, vec3(1.0 / 2.2));

    fragColor = vec4(finalColor, 1.0);
}
