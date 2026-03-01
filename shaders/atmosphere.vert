#version 410 core

layout(location = 0) in vec3 a_position;

out vec3 v_worldPos;
out vec3 v_localPos;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;

void main() {
    vec4 worldPos = u_model * vec4(a_position, 1.0);
    v_worldPos = worldPos.xyz;
    v_localPos = a_position;

    gl_Position = u_projection * u_view * worldPos;
}
