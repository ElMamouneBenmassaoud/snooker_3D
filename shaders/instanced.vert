#version 400 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec3 instancePos;   // one per instance, divisor = 1

uniform mat4 view;
uniform mat4 projection;

out vec3 fragNormal;
out vec3 fragPos;
out vec2 fragUV;

void main() {
    // Pure translation: normal is unchanged (no rotation/scale)
    fragPos    = position + instancePos;
    fragNormal = normal;
    fragUV     = uv;
    gl_Position = projection * view * vec4(fragPos, 1.0);
}
