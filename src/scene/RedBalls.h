#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "Shader.h"
#include <array>
#include <vector>

class RedBalls {
public:
    static constexpr float RADIUS = 0.022f;
    static constexpr int   COUNT  = 15;

    std::array<glm::vec3, COUNT> positions;
    std::array<glm::quat, COUNT> orients;

    explicit RedBalls(const std::array<glm::vec3, COUNT>& positions);
    ~RedBalls();

    // Upload updated positions + orientations to GPU (call after physics step)
    void uploadInstanceData();

    void draw(Shader& shader) const;

private:
    GLuint VAO, sphereVBO, instanceVBO;
    int    vertexCount;

    static std::vector<float> buildSphere();
};
