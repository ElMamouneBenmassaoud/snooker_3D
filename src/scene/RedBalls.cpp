#include "RedBalls.h"
#include <cmath>

static const float PI = 3.14159265f;

std::vector<float> RedBalls::buildSphere() {
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
                v.insert(v.end(), { r*nx, r*ny, r*nz,  nx, ny, nz,
                                    th / (2.0f * PI), phi / PI });
            };

            push(phi0, th0); push(phi1, th0); push(phi1, th1);
            push(phi0, th0); push(phi1, th1); push(phi0, th1);
        }
    }
    return v;
}

RedBalls::RedBalls(const std::array<glm::vec3, COUNT>& pos) : positions(pos) {
    std::vector<float> sphere = buildSphere();
    vertexCount = (int)sphere.size() / 8;

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &sphereVBO);
    glGenBuffers(1, &instanceVBO);

    glBindVertexArray(VAO);

    // Sphere geometry (shared across all instances)
    glBindBuffer(GL_ARRAY_BUFFER, sphereVBO);
    glBufferData(GL_ARRAY_BUFFER, sphere.size() * sizeof(float), sphere.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // Per-instance positions (GL_DYNAMIC_DRAW: updated each frame by physics)
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, COUNT * sizeof(glm::vec3), positions.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(3);
    glVertexAttribDivisor(3, 1);   // advance once per instance, not per vertex

    glBindVertexArray(0);
}

RedBalls::~RedBalls() {
    glDeleteBuffers(1, &instanceVBO);
    glDeleteBuffers(1, &sphereVBO);
    glDeleteVertexArrays(1, &VAO);
}

void RedBalls::uploadPositions() {
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, COUNT * sizeof(glm::vec3), positions.data());
}

void RedBalls::draw(Shader& shader) const {
    shader.setInt("useTexture", 0);
    shader.setVec3("objectColor", 0.85f, 0.08f, 0.08f);
    glBindVertexArray(VAO);
    glDrawArraysInstanced(GL_TRIANGLES, 0, vertexCount, COUNT);
    glBindVertexArray(0);
}
