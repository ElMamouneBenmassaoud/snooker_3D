#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

#include "Shader.h"
#include "Camera.h"
#include "Table.h"

const int WIDTH  = 1280;
const int HEIGHT = 720;

Camera* gCamera = nullptr;
bool    gMouseDown = false;
double  gLastX = 0, gLastY = 0;

void framebuffer_size_callback(GLFWwindow*, int w, int h) {
    glViewport(0, 0, w, h);
}

void mouse_button_callback(GLFWwindow*, int button, int action, int) {
    if (button == GLFW_MOUSE_BUTTON_LEFT)
        gMouseDown = (action == GLFW_PRESS);
}

void cursor_pos_callback(GLFWwindow* window, double x, double y) {
    if (gMouseDown && gCamera) {
        float dx = (float)(x - gLastX);
        float dy = (float)(y - gLastY);
        gCamera->processMouseDrag(dx, -dy);
    }
    gLastX = x;
    gLastY = y;
}

void scroll_callback(GLFWwindow*, double, double dy) {
    if (gCamera) gCamera->processScroll((float)dy);
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Snooker 3D", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);
    glfwSetScrollCallback(window, scroll_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;

    glEnable(GL_DEPTH_TEST);

    Shader shader(PATH_TO_SHADERS"/basic.vert", PATH_TO_SHADERS"/basic.frag");
    Table  table;
    Camera camera(glm::vec3(0.0f), 5.5f, 0.0f, 35.0f);
    gCamera = &camera;

    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)WIDTH / HEIGHT, 0.1f, 100.0f);
    glm::mat4 model      = glm::mat4(1.0f);

    shader.use();
    shader.setMat4("model",      model);
    shader.setMat4("projection", projection);
    shader.setVec3("lightPos",   0.0f, 5.0f, 0.0f);

    while (!glfwWindowShouldClose(window)) {
        processInput(window);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();
        shader.setMat4("view",    camera.getViewMatrix());
        shader.setVec3("viewPos", camera.getPosition());
        table.draw(shader);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
