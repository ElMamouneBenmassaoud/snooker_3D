#version 400 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 fragNormal;
out vec3 fragPos;

void main() {
    vec4 worldPos = model * vec4(position, 1.0);
    fragPos    = vec3(worldPos);
    fragNormal = mat3(transpose(inverse(model))) * normal;
    gl_Position = projection * view * worldPos;
}
