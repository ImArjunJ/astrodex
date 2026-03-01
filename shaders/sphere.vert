#version 410 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aInstancePosRadius;  // xyz = position, w = radius
layout(location = 3) in vec4 aInstanceColorEmissive;  // xyz = color, w = emissive
layout(location = 4) in vec4 aInstanceAtmosphere;  // xyz = atmosphere color, w = hasAtmosphere

uniform mat4 uViewProjection;
uniform vec3 uCameraPos;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vColor;
out float vEmissive;
out vec3 vAtmosphereColor;
out float vHasAtmosphere;
out vec3 vViewDir;
out float vDistToCamera;

void main() {
    vec3 worldPos = aPos * aInstancePosRadius.w + aInstancePosRadius.xyz;
    vWorldPos = worldPos;
    vNormal = aNormal;
    vColor = aInstanceColorEmissive.xyz;
    vEmissive = aInstanceColorEmissive.w;
    vAtmosphereColor = aInstanceAtmosphere.xyz;
    vHasAtmosphere = aInstanceAtmosphere.w;
    vViewDir = normalize(uCameraPos - worldPos);
    vDistToCamera = length(uCameraPos - worldPos);

    gl_Position = uViewProjection * vec4(worldPos, 1.0);
}
