#pragma once

#include <glad/glad.h>
#include <string>
#include <vector>

class Cubemap {
public:
    GLuint ID;

    // faces order: right, left, top, bottom, front, back
    explicit Cubemap(const std::vector<std::string>& facePaths);
    ~Cubemap();

    void bind(int unit = 0) const;
    void draw() const;

private:
    GLuint VAO, VBO;

    void initGeometry();
};
