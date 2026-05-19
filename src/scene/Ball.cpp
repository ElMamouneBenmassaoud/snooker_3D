#include "Ball.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cmath>

static const float PI = 3.14159265f;

std::vector<float> Ball::buildSphere() {
    const int stacks = 16, slices = 16;
    const float r = RADIUS;
    std::vector<float> v;
    v.reserve(stacks * slices * 6 * 8);

    for (int i = 0; i < stacks; i++) {
        float phi0 = PI * i       / stacks;
        float phi1 = PI * (i + 1) / stacks;
        for (int j = 0; j < slices; j++) {
            float th0 = 2.0f * PI * j       / slices;
            float th1 = 2.0f * PI * (j + 1) / slices;

            auto push = [&](float phi, float th) {
                float nx = sinf(phi) * cosf(th);
                float ny = cosf(phi);
                float nz = sinf(phi) * sinf(th);
                float u  = th  / (2.0f * PI);
                float vt = phi / PI;
                v.insert(v.end(), { r*nx, r*ny, r*nz, nx, ny, nz, u, vt });
            };

            push(phi0, th0); push(phi1, th0); push(phi1, th1);
            push(phi0, th0); push(phi1, th1); push(phi0, th1);
        }
    }
    return v;
}

Ball::Ball(glm::vec3 pos, glm::vec3 color)
    : pos(pos), color(color)
{
    mesh = std::make_unique<Mesh>(buildSphere());
}

void Ball::draw(Shader& shader) const {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), pos) * glm::mat4_cast(orient);
    shader.setMat4("model", model);
    shader.setInt("useTexture", 0);
    shader.setVec3("objectColor", color);
    mesh->draw();
}
