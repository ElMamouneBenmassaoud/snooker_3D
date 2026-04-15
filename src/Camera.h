#pragma once

#include <glm/glm.hpp>

class Camera {
public:
    Camera(glm::vec3 target, float distance, float yaw, float pitch);

    glm::mat4 getViewMatrix() const;
    glm::vec3 getPosition()   const;

    void processMouseDrag(float dx, float dy);
    void processScroll(float delta);

private:
    glm::vec3 target;
    float distance;
    float yaw;
    float pitch;
};
