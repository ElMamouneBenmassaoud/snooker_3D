#version 400 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec3 instancePos;   // per-instance position
layout(location = 4) in vec4 instanceQuat;  // per-instance orientation (x,y,z,w)

uniform mat4 view;
uniform mat4 projection;

out vec3 fragNormal;
out vec3 fragPos;
out vec2 fragUV;

// Convert unit quaternion (x,y,z,w) to rotation matrix (column-major)
mat3 quatToMat3(vec4 q) {
    float x = q.x, y = q.y, z = q.z, w = q.w;
    return mat3(
        vec3(1.0 - 2.0*(y*y + z*z),       2.0*(x*y + w*z),       2.0*(x*z - w*y)),
        vec3(      2.0*(x*y - w*z), 1.0 - 2.0*(x*x + z*z),       2.0*(y*z + w*x)),
        vec3(      2.0*(x*z + w*y),       2.0*(y*z - w*x), 1.0 - 2.0*(x*x + y*y))
    );
}

void main() {
    mat3 rot = quatToMat3(instanceQuat);

    // Rotate sphere vertex/normal around ball centre, then translate
    fragPos    = rot * position + instancePos;
    fragNormal = rot * normal;           // rotation is orthogonal: no transpose/inverse needed
    fragUV     = uv;

    gl_Position = projection * view * vec4(fragPos, 1.0);
}
