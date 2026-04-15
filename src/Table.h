#pragma once

#include "Mesh.h"
#include "Shader.h"
#include <memory>

class Table {
public:
    Table();
    void draw(Shader& shader) const;

private:
    std::unique_ptr<Mesh> surface;
    std::unique_ptr<Mesh> frameL, frameR, frameFar, frameNear;

    static std::vector<float> makeQuad(float x0, float z0, float x1, float z1, float y = 0.0f);
};
