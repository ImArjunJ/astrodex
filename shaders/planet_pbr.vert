#version 410 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;

out vec3 v_worldPos;
out vec3 v_normal;
out vec3 v_localPos;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;

void main() {
    vec4 worldPos = u_model * vec4(a_position, 1.0);
    v_worldPos = worldPos.xyz;
    v_normal = mat3(transpose(inverse(u_model))) * a_normal;
    v_localPos = a_position;

    gl_Position = u_projection * u_view * worldPos;
}
