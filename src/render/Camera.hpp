#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace astrocore {

class Camera {
public:
    Camera();

    // Camera positioning
    void setPosition(const glm::vec3& position);
    void setTarget(const glm::vec3& target);
    void setAspectRatio(float aspect);

    // Orbit controls
    void rotate(float deltaYaw, float deltaPitch);
    void zoom(float delta);
    void pan(float deltaX, float deltaY);

    // Update camera state
    void update(float deltaTime);

    // Getters
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix() const;
    glm::mat4 getViewProjectionMatrix() const;
    glm::vec3 getPosition() const { return m_position; }
    glm::vec3 getTarget() const { return m_target; }
    glm::vec3 getForward() const;
    glm::vec3 getRight() const;
    glm::vec3 getUp() const;

    // Projection parameters
    float getFov() const { return m_fov; }
    float getNearPlane() const { return m_nearPlane; }
    float getFarPlane() const { return m_farPlane; }
    float getAspectRatio() const { return m_aspectRatio; }

private:
    void updateCameraVectors();

    // Camera position and orientation
    glm::vec3 m_position{0.0f, 0.0f, 8.0f};
    glm::vec3 m_target{0.0f};
    glm::vec3 m_up{0.0f, 1.0f, 0.0f};
    glm::vec3 m_worldUp{0.0f, 1.0f, 0.0f};

    // Orbit parameters
    float m_distance = 8.0f;
    float m_yaw = 0.0f;       // Horizontal angle
    float m_pitch = 0.0f;     // Vertical angle
    float m_minDistance = 1.0f;
    float m_maxDistance = 100.0f;
    float m_minPitch = -89.0f * 0.0174533f;  // -89 degrees in radians
    float m_maxPitch = 89.0f * 0.0174533f;   // 89 degrees in radians

    // Projection parameters
    float m_fov = 45.0f * 0.0174533f;  // 45 degrees in radians
    float m_aspectRatio = 16.0f / 9.0f;
    float m_nearPlane = 0.1f;
    float m_farPlane = 1000.0f;

    // Cached matrices
    mutable glm::mat4 m_viewMatrix{1.0f};
    mutable glm::mat4 m_projectionMatrix{1.0f};
    mutable bool m_viewDirty = true;
    mutable bool m_projectionDirty = true;
};

}  // namespace astrocore
