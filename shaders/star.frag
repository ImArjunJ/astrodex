#version 450

layout(location = 0) in vec3 vColor;
layout(location = 1) in float vBrightness;

layout(location = 0) out vec4 fragColor;

void main() {
    // Point sprite: gl_PointCoord is (0,0) at top-left, (1,1) at bottom-right
    vec2 coord = gl_PointCoord * 2.0 - 1.0;  // [-1, 1]
    float r2 = dot(coord, coord);

    // Gaussian falloff: bright core + softer halo
    float core = exp(-r2 * 8.0);    // tight bright center
    float halo = exp(-r2 * 2.0);    // wider soft glow

    float alpha = core * 0.8 + halo * 0.3;

    // Discard pixels outside the point circle
    if (alpha < 0.01) discard;

    // Core is white, halo picks up star color
    vec3 coreColor = mix(vColor, vec3(1.0), 0.6);  // whitish core
    vec3 color = mix(vColor * halo, coreColor * core, core / max(core + halo, 0.001));

    // Apply brightness with tone mapping
    float brightness = min(vBrightness, 50.0);
    color *= brightness;

    fragColor = vec4(color, alpha);
}
