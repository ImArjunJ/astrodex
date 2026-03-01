#version 410 core

// Instanced terrain vertex shader
// One shared mesh, per-instance UV bounds

layout(location = 0) in vec2 aLocalUV;      // [0,1] local patch UV
layout(location = 1) in vec4 aUVBounds;     // Instance: minU, minV, maxU, maxV
layout(location = 2) in float aFace;        // Instance: cube face
layout(location = 3) in float aLevel;       // Instance: LOD level

out VS_OUT {
    vec2 uv;
    float face;
    float level;
} vs_out;

void main() {
    // Transform local UV to world UV using instance bounds
    vec2 worldUV = mix(aUVBounds.xy, aUVBounds.zw, aLocalUV);

    vs_out.uv = worldUV;
    vs_out.face = aFace;
    vs_out.level = aLevel;

    gl_Position = vec4(worldUV, 0.0, 1.0);
}
