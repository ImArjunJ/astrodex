#include "render/Camera.hpp"
#include <cmath>
#include <algorithm>

namespace astrocore {

Camera::Camera() {
    updateCameraVectors();
}

void Camera::setPosition(const glm::vec3& position) {
    m_position = position;
    m_distance = glm::length(position - m_target);
    m_viewDirty = true;
}

void Camera::setTarget(const glm::vec3& target) {
    m_target = target;
    m_viewDirty = true;
}

void Camera::setAspectRatio(float aspect) {
    m_aspectRatio = aspect;
    m_projectionDirty = true;
}

void Camera::rotate(float deltaYaw, float deltaPitch) {
    m_yaw += deltaYaw;
    m_pitch = std::clamp(m_pitch + deltaPitch, m_minPitch, m_maxPitch);
    updateCameraVectors();
    m_viewDirty = true;
}

void Camera::zoom(float delta) {
    m_distance = std::clamp(m_distance + delta, m_minDistance, m_maxDistance);
    updateCameraVectors();
    m_viewDirty = true;
}

void Camera::pan(float deltaX, float deltaY) {
    glm::vec3 right = getRight();
    glm::vec3 up = getUp();
    m_target += right * deltaX + up * deltaY;
    updateCameraVectors();
    m_viewDirty = true;
}

void Camera::update(float deltaTime) {
    // Currently no interpolation, but could add smooth camera movement here
}

void Camera::updateCameraVectors() {
    // Calculate position on sphere around target
    float x = m_distance * std::cos(m_pitch) * std::sin(m_yaw);
    float y = m_distance * std::sin(m_pitch);
    float z = m_distance * std::cos(m_pitch) * std::cos(m_yaw);

    m_position = m_target + glm::vec3(x, y, z);
}

glm::mat4 Camera::getViewMatrix() const {
    if (m_viewDirty) {
        m_viewMatrix = glm::lookAt(m_position, m_target, m_worldUp);
        m_viewDirty = false;
    }
    return m_viewMatrix;
}

glm::mat4 Camera::getProjectionMatrix() const {
    if (m_projectionDirty) {
        m_projectionMatrix = glm::perspective(m_fov, m_aspectRatio, m_nearPlane, m_farPlane);
        m_projectionDirty = false;
    }
    return m_projectionMatrix;
}

glm::mat4 Camera::getViewProjectionMatrix() const {
    return getProjectionMatrix() * getViewMatrix();
}

glm::vec3 Camera::getForward() const {
    return glm::normalize(m_target - m_position);
}

glm::vec3 Camera::getRight() const {
    return glm::normalize(glm::cross(getForward(), m_worldUp));
}

glm::vec3 Camera::getUp() const {
    return glm::normalize(glm::cross(getRight(), getForward()));
}

}  // namespace astrocore
