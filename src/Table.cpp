#include "Table.h"

static const float PX = 1.35f;
static const float PZ = 0.675f;
static const float FT = 0.15f;

std::vector<float> Table::makeQuad(float x0, float z0, float x1, float z1, float y) {
    return {
        x0, y, z0,  0,1,0,
        x1, y, z0,  0,1,0,
        x1, y, z1,  0,1,0,
        x0, y, z0,  0,1,0,
        x1, y, z1,  0,1,0,
        x0, y, z1,  0,1,0,
    };
}

Table::Table() {
    surface  = std::make_unique<Mesh>(makeQuad(-PX,       -PZ,       PX,      PZ));
    frameL   = std::make_unique<Mesh>(makeQuad(-PX - FT,  -PZ - FT, -PX,     PZ + FT));
    frameR   = std::make_unique<Mesh>(makeQuad( PX,       -PZ - FT,  PX + FT, PZ + FT));
    frameFar = std::make_unique<Mesh>(makeQuad(-PX,       -PZ - FT,  PX,     -PZ));
    frameNear= std::make_unique<Mesh>(makeQuad(-PX,        PZ,        PX,      PZ + FT));
}

void Table::draw(Shader& shader) const {
    shader.setVec3("objectColor", 0.07f, 0.40f, 0.07f);
    surface->draw();

    shader.setVec3("objectColor", 0.35f, 0.18f, 0.05f);
    frameL->draw();
    frameR->draw();
    frameFar->draw();
    frameNear->draw();
}
