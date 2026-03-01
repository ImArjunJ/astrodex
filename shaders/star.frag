#version 450

layout(location = 0) in vec3 vColor;
layout(location = 1) in float vBrightness;

layout(set = 0, binding = 0) uniform StarUniforms {
    mat4 view;
    mat4 projection;
    vec4 cameraPos_time;
    vec4 renderParams;   // x = pointScale, y = brightnessBoost, z = debugMode, w = unused
} u;

layout(location = 0) out vec4 fragColor;

void main() {
    vec2 coord = gl_PointCoord * 2.0 - 1.0;
    float r2 = dot(coord, coord);

    // Discard outside circle
    if (r2 > 1.0) discard;

    // Debug mode: all stars as flat bright white dots (tests if positions are correct)
    if (u.renderParams.z > 0.5) {
        float a = 1.0 - r2;
        fragColor = vec4(vec3(a), a);
        return;
    }

    // Normal rendering
    float core = exp(-r2 * 8.0);
    float halo = exp(-r2 * 2.0);
    float alpha = core * 0.8 + halo * 0.3;
    if (alpha < 0.01) discard;

    vec3 coreColor = mix(vColor, vec3(1.0), 0.6);
    vec3 color = mix(vColor * halo, coreColor * core, core / max(core + halo, 0.001));

    float brightness = min(vBrightness, 50.0);
    color *= brightness;

    fragColor = vec4(color, alpha);
}
