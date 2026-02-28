#version 450

layout(location = 0) in vec3 aPosition;   // xyz in parsecs
layout(location = 1) in float aMagnitude;  // apparent magnitude (from Earth)
layout(location = 2) in vec3 aColor;       // RGB from B-V

layout(set = 0, binding = 0) uniform StarUniforms {
    mat4 view;
    mat4 projection;
    vec4 cameraPos_time;     // xyz = camera position, w = time
    vec4 renderParams;       // x = pointScale, y = brightnessBoost, z = unused, w = unused
} u;

layout(location = 0) out vec3 vColor;
layout(location = 1) out float vBrightness;

void main() {
    vec4 viewPos = u.view * vec4(aPosition, 1.0);
    gl_Position = u.projection * viewPos;

    float cameraDist = length(viewPos.xyz);

    // Catalog flux is apparent magnitude as seen from Sol (origin).
    // Recompute flux at the camera's actual distance using inverse-square law.
    float catalogFlux = pow(10.0, -0.4 * aMagnitude);
    float solDist = length(aPosition);
    float ratio = solDist / max(cameraDist, 0.001);
    float flux = catalogFlux * ratio * ratio;

    // Point size: brighter and closer → larger, but every star is at least 1px
    float scale = u.renderParams.x;
    float size = scale * pow(max(flux, 1e-10), 0.25) * 30.0;
    gl_PointSize = clamp(size, 1.0, 64.0);

    // Perceptual brightness: use log scale so dim stars are still visible.
    // Maps flux range [1e-10, 1e2] to visible brightness [0.08, ~2.0].
    // Every star gets at least ~8% brightness so it's never invisible.
    float boost = u.renderParams.y;
    float logBright = log2(flux * 1e6 + 1.0) / 20.0;  // compress huge range
    vBrightness = max(0.08, logBright) * boost;

    vColor = aColor;
}
