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

void Camera::moveForward(float distance) {
    glm::vec3 forward = getForward();
    if (m_freeMode) {
        // Free mode: move position and target together
        m_position += forward * distance;
        m_target += forward * distance;
    } else {
        // Orbit mode: zoom in/out
        zoom(-distance);
        return;
    }
    m_viewDirty = true;
}

void Camera::moveRight(float distance) {
    glm::vec3 right = getRight();
    if (m_freeMode) {
        m_position += right * distance;
        m_target += right * distance;
    } else {
        pan(distance, 0.0f);
        return;
    }
    m_viewDirty = true;
}

void Camera::moveUp(float distance) {
    if (m_freeMode) {
        // Use world up for free movement
        m_position += m_worldUp * distance;
        m_target += m_worldUp * distance;
    } else {
        pan(0.0f, distance);
        return;
    }
    m_viewDirty = true;
}

void Camera::update(float deltaTime) {
    // Handle smooth transitions
    if (m_transitioning) {
        m_transitionProgress += deltaTime / m_transitionDuration;
        if (m_transitionProgress >= 1.0f) {
            m_transitionProgress = 1.0f;
            m_transitioning = false;
        }

        // Smooth ease-in-out interpolation
        float t = m_transitionProgress;
        float smooth = t * t * (3.0f - 2.0f * t);  // smoothstep

        m_target = glm::mix(m_transitionStartTarget, m_transitionEndTarget, smooth);
        m_position = glm::mix(m_transitionStartPos, m_transitionEndPos, smooth);
        m_distance = glm::mix(m_transitionStartDistance, m_transitionEndDistance, smooth);
        m_viewDirty = true;
    }
}

void Camera::transitionTo(const glm::vec3& newTarget, float duration, float targetDistance) {
    m_transitionStartPos = m_position;
    m_transitionStartTarget = m_target;
    m_transitionEndTarget = newTarget;
    m_transitionStartDistance = m_distance;

    // If target distance specified, transition to it; otherwise keep current
    if (targetDistance > 0.0f) {
        m_transitionEndDistance = targetDistance;
        // Calculate end position at the target distance
        glm::vec3 dir = glm::normalize(m_position - m_target);
        m_transitionEndPos = newTarget + dir * targetDistance;
    } else {
        m_transitionEndDistance = m_distance;
        glm::vec3 offset = m_position - m_target;
        m_transitionEndPos = newTarget + offset;
    }

    m_transitionProgress = 0.0f;
    m_transitionDuration = duration;
    m_transitioning = true;
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
