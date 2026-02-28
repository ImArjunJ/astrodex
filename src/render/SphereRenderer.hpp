#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>
#include "render/ShaderProgram.hpp"
#include <vector>

namespace astrocore {

// Simple sphere renderer for drawing multiple celestial bodies
// Uses instanced rendering for efficiency
class SphereRenderer {
public:
    static constexpr int MAX_INSTANCES = 1024;

    SphereRenderer();
    ~SphereRenderer();

    void init();
    void shutdown();

    // Begin a new frame of rendering
    void begin(const glm::mat4& viewProjection, const glm::vec3& cameraPos,
               const glm::vec3& sunDir, float sunIntensity);

    // Queue a sphere for rendering
    void drawSphere(const glm::vec3& position, float radius,
                    const glm::vec3& color, float emissive = 0.0f,
                    bool hasAtmosphere = false, const glm::vec3& atmosphereColor = glm::vec3(0.3f, 0.5f, 1.0f));

    // Flush all queued spheres
    void end();

    // Get number of spheres rendered last frame
    int getRenderedCount() const { return m_renderedCount; }

private:
    void createSphereMesh(int segments);
    void createShader();

    // Mesh
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_ebo = 0;
    GLuint m_instanceVBO = 0;
    int m_indexCount = 0;

    // Shader
    ShaderProgram m_shader;

    // Instance data
    struct InstanceData {
        glm::vec3 position;
        float radius;
        glm::vec3 color;
        float emissive;
        glm::vec3 atmosphereColor;
        float hasAtmosphere;
    };
    std::vector<InstanceData> m_instances;

    // Frame state
    glm::mat4 m_viewProjection;
    glm::vec3 m_cameraPos;
    glm::vec3 m_sunDir;
    float m_sunIntensity = 1.0f;
    int m_renderedCount = 0;

    bool m_initialized = false;
};

} // namespace astrocore
