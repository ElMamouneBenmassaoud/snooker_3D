#include "Shader.h"

#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>
#include <iostream>

Shader::Shader(const char* vertexPath, const char* fragmentPath) {
    std::string vertCode, fragCode;
    std::ifstream vFile(vertexPath), fFile(fragmentPath);

    if (!vFile.is_open() || !fFile.is_open()) {
        std::cerr << "ERROR: Cannot open shader files\n"
                  << "  vert: " << vertexPath << "\n"
                  << "  frag: " << fragmentPath << "\n";
        ID = 0;
        return;
    }

    std::stringstream vStream, fStream;
    vStream << vFile.rdbuf();
    fStream << fFile.rdbuf();
    vertCode = vStream.str();
    fragCode = fStream.str();

    const char* vSrc = vertCode.c_str();
    const char* fSrc = fragCode.c_str();

    GLuint vert = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert, 1, &vSrc, nullptr);
    glCompileShader(vert);
    checkCompileErrors(vert, "VERTEX");

    GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag, 1, &fSrc, nullptr);
    glCompileShader(frag);
    checkCompileErrors(frag, "FRAGMENT");

    ID = glCreateProgram();
    glAttachShader(ID, vert);
    glAttachShader(ID, frag);
    glLinkProgram(ID);
    checkCompileErrors(ID, "PROGRAM");

    glDeleteShader(vert);
    glDeleteShader(frag);
}

void Shader::use() {
    glUseProgram(ID);
}

void Shader::setInt(const std::string& name, int value) {
    glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
}

void Shader::setFloat(const std::string& name, float value) {
    glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
}

void Shader::setVec3(const std::string& name, const glm::vec3& value) {
    glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, glm::value_ptr(value));
}

void Shader::setVec3(const std::string& name, float x, float y, float z) {
    glUniform3f(glGetUniformLocation(ID, name.c_str()), x, y, z);
}

void Shader::setMat4(const std::string& name, const glm::mat4& value) {
    glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::checkCompileErrors(GLuint shader, const std::string& type) {
    int success;
    char log[1024];

    if (type != "PROGRAM") {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 1024, nullptr, log);
            std::cerr << "ERROR: " << type << " shader compilation failed\n" << log << "\n";
        }
    } else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, 1024, nullptr, log);
            std::cerr << "ERROR: Program linking failed\n" << log << "\n";
        }
    }
}
