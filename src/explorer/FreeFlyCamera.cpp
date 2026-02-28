#include "explorer/FreeFlyCamera.hpp"
#include <algorithm>
#include <cmath>

namespace astrocore {

FreeFlyCamera::FreeFlyCamera() {
    updateVectors();
}

void FreeFlyCamera::processKeyboard(bool forward, bool back, bool left, bool right,
                                     bool up, bool down, float deltaTime) {
    float velocity = m_speed * deltaTime;

    if (forward) m_position += m_front * velocity;
    if (back)    m_position -= m_front * velocity;
    if (left)    m_position -= m_right * velocity;
    if (right)   m_position += m_right * velocity;
    if (up)      m_position += m_worldUp * velocity;
    if (down)    m_position -= m_worldUp * velocity;
}

void FreeFlyCamera::processArrowKeys(bool lookLeft, bool lookRight, bool lookUp, bool lookDown, float deltaTime) {
    float rotSpeed = 90.0f * deltaTime; // degrees per second
    if (lookLeft)  m_yaw   -= rotSpeed;
    if (lookRight) m_yaw   += rotSpeed;
    if (lookUp)    m_pitch += rotSpeed;
    if (lookDown)  m_pitch -= rotSpeed;
    m_pitch = std::clamp(m_pitch, -89.0f, 89.0f);
    if (lookLeft || lookRight || lookUp || lookDown) updateVectors();
}

void FreeFlyCamera::reset() {
    m_position = glm::vec3(0.0f);
    m_yaw   = -90.0f;
    m_pitch = 0.0f;
    m_speed = 1.0f;
    updateVectors();
}

void FreeFlyCamera::processMouseMovement(float xOffset, float yOffset) {
    m_yaw   += xOffset * m_sensitivity;
    m_pitch += yOffset * m_sensitivity;
    m_pitch  = std::clamp(m_pitch, -89.0f, 89.0f);
    updateVectors();
}

void FreeFlyCamera::processScroll(float yOffset) {
    // Multiply speed by 1.1 per scroll tick
    if (yOffset > 0) m_speed *= 1.1f;
    if (yOffset < 0) m_speed /= 1.1f;
    m_speed = std::clamp(m_speed, 0.001f, 100000.0f);
}

glm::mat4 FreeFlyCamera::getViewMatrix() const {
    return glm::lookAt(m_position, m_position + m_front, m_up);
}

glm::mat4 FreeFlyCamera::getProjectionMatrix() const {
    return glm::perspective(glm::radians(m_fov), m_aspect, m_nearPlane, m_farPlane);
}

void FreeFlyCamera::updateVectors() {
    float yawRad   = glm::radians(m_yaw);
    float pitchRad = glm::radians(m_pitch);

    m_front.x = std::cos(yawRad) * std::cos(pitchRad);
    m_front.y = std::sin(pitchRad);
    m_front.z = std::sin(yawRad) * std::cos(pitchRad);
    m_front = glm::normalize(m_front);

    m_right = glm::normalize(glm::cross(m_front, m_worldUp));
    m_up    = glm::normalize(glm::cross(m_right, m_front));
}

} // namespace astrocore
