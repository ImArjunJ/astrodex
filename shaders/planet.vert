#version 410 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;

out vec2 uv;

void main() {
    uv = aPos;  // -1 to 1 for ray direction
    gl_Position = vec4(aPos, 0.0, 1.0);
}
