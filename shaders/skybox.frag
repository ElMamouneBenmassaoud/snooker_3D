#version 400 core

in vec3 texCoord;

uniform samplerCube skybox;

out vec4 fragColor;

void main() {
    fragColor = texture(skybox, texCoord);
}
