#pragma once

#include <glad/glad.h>
#include <vector>

class Mesh {
public:
    GLuint VAO;
    int    vertexCount;

    Mesh(const std::vector<float>& vertices);
    ~Mesh();

    void draw() const;

private:
    GLuint VBO;
};
