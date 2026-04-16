#pragma once

#include <glad/glad.h>
#include <string>

class Texture {
public:
    GLuint ID;

    Texture(const std::string& path);
    ~Texture();

    void bind(int unit = 0) const;
};
