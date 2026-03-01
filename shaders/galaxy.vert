#version 410 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec4 aColor;      // RGB + size
layout(location = 2) in float aType;      // 0 = star, 1 = planet, 2 = selected planet

uniform mat4 uViewProjection;
uniform vec3 uCameraPos;
uniform float uTime;

out vec4 vColor;
out float vType;
out float vDistance;
out float vPointSize;

void main() {
    vec4 worldPos = vec4(aPosition, 1.0);
    gl_Position = uViewProjection * worldPos;

    // Distance from camera for fading and sizing
    vDistance = length(aPosition - uCameraPos);

    // Base point size from attribute, scaled by distance
    float baseSize = aColor.a;
    float distanceFade = clamp(500.0 / (vDistance + 1.0), 0.1, 1.0);

    // Stars get smaller with distance, planets stay more visible
    if (aType < 0.5) {
        // Star
        vPointSize = baseSize * distanceFade * 3.0;
    } else {
        // Planet - stay more visible
        vPointSize = baseSize * distanceFade * 8.0 + 2.0;
    }

    // Clamp point size
    gl_PointSize = clamp(vPointSize, 1.0, 50.0);

    vColor = vec4(aColor.rgb, 1.0);
    vType = aType;
}
