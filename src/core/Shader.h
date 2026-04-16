#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>

class Shader {
public:
    GLuint ID;

    Shader(const char* vertexPath, const char* fragmentPath);

    void use();

    void setInt(const std::string& name, int value);
    void setFloat(const std::string& name, float value);
    void setVec3(const std::string& name, const glm::vec3& value);
    void setVec3(const std::string& name, float x, float y, float z);
    void setMat4(const std::string& name, const glm::mat4& value);

private:
    void checkCompileErrors(GLuint shader, const std::string& type);
};
