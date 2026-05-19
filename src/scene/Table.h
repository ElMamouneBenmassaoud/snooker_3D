#pragma once

#include "Mesh.h"
#include "Shader.h"
#include "Texture.h"
#include <memory>
#include <vector>

class Table {
public:
    Table();
    void draw(Shader& shader, const Texture& feltTex, const Texture& woodTex) const;

private:
    std::unique_ptr<Mesh> surface;
    std::unique_ptr<Mesh> underPanel;
    std::vector<std::unique_ptr<Mesh>> sideWalls;
    std::unique_ptr<Mesh> frameL, frameR, frameFar, frameNear;
    std::unique_ptr<Mesh> baulkLine, dMarking;
    std::vector<std::unique_ptr<Mesh>> pockets;
    std::vector<std::unique_ptr<Mesh>> cushions;
    std::vector<std::unique_ptr<Mesh>> legs;
    std::vector<std::unique_ptr<Mesh>> spots;

    static std::vector<float> makeQuad(float x0, float z0, float x1, float z1,
                                       float tU = 1, float tV = 1, float y = 0);
    static std::vector<float> makeDisc(float cx, float cz, float radius,
                                       int segments = 32, float y = 0.001f);
    static std::vector<float> makeSemiDisc(float cx, float cz, float radius,
                                           int segments = 32, float y = 0.001f);
    static std::vector<float> makeCushionZ(float x0, float x1, float z0, float z1, float nx,
                                           float lo_bevel = 0, float hi_bevel = 0);
    static std::vector<float> makeCushionX(float z0, float z1, float x0, float x1, float nz,
                                           float lo_bevel = 0, float hi_bevel = 0);
    static std::vector<float> makeLeg(float cx, float cz);
    static std::vector<float> makeCornerPocket(float sx, float sz);
    static std::vector<float> makeEllipseDisc(float cx, float cz, float rx, float rz,
                                               int segments = 32, float y = 0.001f);
};
