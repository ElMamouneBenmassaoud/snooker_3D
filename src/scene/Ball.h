#pragma once

#include <glm/glm.hpp>
#include "Mesh.h"
#include "Shader.h"
#include <memory>
#include <vector>

class Ball {
public:
    static constexpr float RADIUS = 0.022f;

    glm::vec3 pos;
    glm::vec3 color;

    Ball(glm::vec3 pos, glm::vec3 color);
    void draw(Shader& shader) const;

private:
    std::unique_ptr<Mesh> mesh;
    static std::vector<float> buildSphere();
};
