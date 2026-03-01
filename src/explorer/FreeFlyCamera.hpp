#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace astrocore {

class FreeFlyCamera {
public:
    FreeFlyCamera();

    // Movement (call per-frame with deltaTime)
    void processKeyboard(bool forward, bool back, bool left, bool right,
                         bool up, bool down, float deltaTime);
    void processArrowKeys(bool lookLeft, bool lookRight, bool lookUp, bool lookDown, float deltaTime);
    void processMouseMovement(float xOffset, float yOffset);
    void processScroll(float yOffset);
    void reset();

    // Matrices
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix() const;

    // Setters
    void setAspectRatio(float aspect) { m_aspect = aspect; }
    void setPosition(const glm::vec3& pos) { m_position = pos; }
    void setSpeed(float speed) { m_speed = speed; }
    void setYawPitch(float yaw, float pitch) { m_yaw = yaw; m_pitch = pitch; updateVectors(); }

    // Getters
    glm::vec3 getPosition() const { return m_position; }
    glm::vec3 getForward()  const { return m_front; }
    float     getSpeed()    const { return m_speed; }
    float     getYaw()      const { return m_yaw; }
    float     getPitch()    const { return m_pitch; }
    float     getFov()      const { return m_fov; }

private:
    void updateVectors();

    glm::vec3 m_position{0.0f, 0.0f, 0.0f};  // Sol at origin
    glm::vec3 m_front{0.0f, 0.0f, -1.0f};
    glm::vec3 m_up{0.0f, 1.0f, 0.0f};
    glm::vec3 m_right{1.0f, 0.0f, 0.0f};
    glm::vec3 m_worldUp{0.0f, 1.0f, 0.0f};

    float m_yaw   = 9.2f;     // Face ~(4.3, -0.8, 0.7) normalized
    float m_pitch = -10.4f;

    float m_speed       = 1.0f;    // parsecs/second
    float m_sensitivity = 0.1f;
    float m_fov         = 60.0f;

    float m_aspect   = 16.0f / 9.0f;
    float m_nearPlane = 0.001f;     // parsecs
    float m_farPlane  = 200000.0f;  // parsecs
};

} // namespace astrocore
