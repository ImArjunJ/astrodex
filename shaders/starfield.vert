#version 410 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;

out vec2 vTexCoord;
out vec3 vRayDir;

uniform mat4 uInvViewProj;

void main() {
    vTexCoord = aTexCoord;

    // Calculate ray direction for this pixel using near and far points
    vec4 nearClip = vec4(aPos, -1.0, 1.0);
    vec4 farClip = vec4(aPos, 1.0, 1.0);

    vec4 nearWorld = uInvViewProj * nearClip;
    vec4 farWorld = uInvViewProj * farClip;
    nearWorld /= nearWorld.w;
    farWorld /= farWorld.w;

    // Ray direction is from near to far
    vRayDir = normalize(farWorld.xyz - nearWorld.xyz);

    gl_Position = vec4(aPos, 0.9999, 1.0);  // Behind everything
}
