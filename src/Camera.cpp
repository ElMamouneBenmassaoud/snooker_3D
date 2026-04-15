#include "Camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

Camera::Camera(glm::vec3 target, float distance, float yaw, float pitch)
    : target(target), distance(distance), yaw(yaw), pitch(pitch) {}

glm::vec3 Camera::getPosition() const {
    return target + glm::vec3(
        distance * cos(glm::radians(pitch)) * sin(glm::radians(yaw)),
        distance * sin(glm::radians(pitch)),
        distance * cos(glm::radians(pitch)) * cos(glm::radians(yaw))
    );
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(getPosition(), target, glm::vec3(0.0f, 1.0f, 0.0f));
}

void Camera::processMouseDrag(float dx, float dy) {
    yaw   += dx * 0.4f;
    pitch += dy * 0.4f;
    pitch  = std::clamp(pitch, 5.0f, 89.0f);
}

void Camera::processScroll(float delta) {
    distance -= delta * 0.3f;
    distance  = std::clamp(distance, 1.5f, 12.0f);
}
