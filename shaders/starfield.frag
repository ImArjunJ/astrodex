#version 410 core

in vec2 vTexCoord;
in vec3 vRayDir;

out vec4 FragColor;

uniform float uTime;

// Hash functions for procedural stars
float hash(vec3 p) {
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float hash2(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

// Procedural stars based on ray direction
vec3 stars(vec3 dir) {
    vec3 color = vec3(0.0);

    // Multiple layers of stars at different scales
    for (int layer = 0; layer < 3; layer++) {
        float scale = 100.0 + float(layer) * 150.0;
        vec3 p = dir * scale;
        vec3 cell = floor(p);
        vec3 local = fract(p) - 0.5;

        // Check this cell and neighbors
        for (int x = -1; x <= 1; x++) {
            for (int y = -1; y <= 1; y++) {
                for (int z = -1; z <= 1; z++) {
                    vec3 offset = vec3(x, y, z);
                    vec3 cellId = cell + offset;

                    // Random star position within cell
                    float h = hash(cellId);
                    if (h > 0.97) {  // Only some cells have stars
                        vec3 starPos = offset + vec3(
                            hash(cellId + 0.1) - 0.5,
                            hash(cellId + 0.2) - 0.5,
                            hash(cellId + 0.3) - 0.5
                        ) * 0.8;

                        float dist = length(local - starPos);

                        // Star brightness and color
                        float brightness = hash(cellId + 0.5);
                        float size = 0.02 + brightness * 0.03;

                        if (dist < size) {
                            float intensity = 1.0 - dist / size;
                            intensity = pow(intensity, 2.0);

                            // Star color temperature
                            float temp = hash(cellId + 0.7);
                            vec3 starColor;
                            if (temp < 0.3) {
                                starColor = vec3(1.0, 0.8, 0.6);  // Orange/red
                            } else if (temp < 0.7) {
                                starColor = vec3(1.0, 1.0, 1.0);  // White
                            } else {
                                starColor = vec3(0.8, 0.9, 1.0);  // Blue-white
                            }

                            // Twinkle
                            float twinkle = 0.8 + 0.2 * sin(uTime * (2.0 + hash(cellId) * 3.0) + hash(cellId) * 6.28);

                            color += starColor * intensity * brightness * twinkle * 0.8;
                        }
                    }
                }
            }
        }
    }

    return color;
}

// Milky way band
vec3 milkyWay(vec3 dir) {
    // Galactic plane roughly along one axis
    float galacticLat = abs(dir.y);
    float band = exp(-galacticLat * galacticLat * 8.0);

    // Some variation along the band
    float noise = hash(floor(dir * 50.0)) * 0.5 + 0.5;

    vec3 milkyColor = vec3(0.15, 0.12, 0.18) * band * noise;
    return milkyColor * 0.3;
}

void main() {
    vec3 dir = normalize(vRayDir);

    // Deep space background
    vec3 color = vec3(0.002, 0.003, 0.008);

    // Add milky way
    color += milkyWay(dir);

    // Add stars
    color += stars(dir);

    FragColor = vec4(color, 1.0);
}
