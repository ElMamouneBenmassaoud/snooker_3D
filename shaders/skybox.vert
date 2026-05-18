#version 400 core

layout(location = 0) in vec3 position;

uniform mat4 projection;
uniform mat4 view;

out vec3 texCoord;

void main() {
    texCoord    = position;
    vec4 pos    = projection * view * vec4(position, 1.0);
    gl_Position = pos.xyww;  // force depth = 1.0 (skybox always behind everything)
}
