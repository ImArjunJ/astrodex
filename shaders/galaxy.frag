#version 410 core

in vec4 vColor;
in float vType;
in float vDistance;
in float vPointSize;

uniform float uTime;

out vec4 fragColor;

void main() {
    // Distance from center of point sprite
    vec2 coord = gl_PointCoord * 2.0 - 1.0;
    float dist = length(coord);

    // Different rendering for stars vs planets
    if (vType < 0.5) {
        // STAR - soft glow with twinkle
        float twinkle = 0.8 + 0.2 * sin(uTime * 3.0 + vDistance * 0.01);

        // Soft falloff
        float alpha = exp(-dist * dist * 2.5) * twinkle;

        // Core is brighter
        float core = exp(-dist * dist * 8.0);
        vec3 color = mix(vColor.rgb, vec3(1.0), core * 0.5);

        fragColor = vec4(color, alpha);
    } else if (vType < 1.5) {
        // PLANET - solid core with glow
        float core = smoothstep(0.4, 0.2, dist);
        float glow = exp(-dist * dist * 3.0);

        vec3 color = vColor.rgb * (core + glow * 0.5);
        float alpha = core + glow * 0.6;

        fragColor = vec4(color, alpha);
    } else {
        // SELECTED PLANET - pulsing ring highlight
        float pulse = 0.7 + 0.3 * sin(uTime * 4.0);

        float core = smoothstep(0.3, 0.1, dist);
        float ring = smoothstep(0.7, 0.5, dist) * smoothstep(0.4, 0.6, dist);
        float outerGlow = exp(-dist * dist * 2.0);

        vec3 color = vColor.rgb * core + vec3(1.0, 0.9, 0.5) * ring * pulse;
        float alpha = core + ring * pulse + outerGlow * 0.4;

        fragColor = vec4(color, alpha);
    }

    // Discard nearly transparent pixels
    if (fragColor.a < 0.01) discard;
}
