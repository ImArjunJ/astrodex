#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>
#include "render/ShaderProgram.hpp"
#include <vector>
#include <string>

namespace astrocore {

// A single ring band
struct RingBand {
    float innerRadius = 1.5f;    // Inner edge (planet radii)
    float outerRadius = 2.0f;    // Outer edge (planet radii)
    glm::vec3 innerColor{0.8f, 0.75f, 0.65f};
    glm::vec3 outerColor{0.6f, 0.55f, 0.5f};
    float opacity = 0.8f;
    float density = 1.0f;        // Relative particle density
};

struct RingParams {
    bool enabled = false;
    int totalParticles = 30000;  // Total particles across all bands
    float thickness = 0.02f;     // Vertical spread

    // Multiple ring bands (like Saturn's A, B, C rings)
    std::vector<RingBand> bands;

    // Helper to add a band
    void addBand(float inner, float outer, glm::vec3 color1, glm::vec3 color2, float opacity = 0.8f) {
        bands.push_back({inner, outer, color1, color2, opacity, 1.0f});
    }

    void clearBands() { bands.clear(); }
};

class RingRenderer {
public:
    RingRenderer() = default;
    ~RingRenderer();

    RingRenderer(const RingRenderer&) = delete;
    RingRenderer& operator=(const RingRenderer&) = delete;

    void init();
    void shutdown();

    // Generate ring particles for a planet
    void generateRing(const RingParams& params, float planetRadius, float planetMass);

    // Update particle positions (orbital motion)
    void update(float deltaTime, float planetMass);

    // Render the ring
    void render(const glm::mat4& viewProj, const glm::vec3& planetPos,
                const glm::vec3& cameraPos, const RingParams& params);

    bool hasRing() const { return m_particleCount > 0; }

private:
    struct RingParticle {
        glm::vec3 position;
        float orbitRadius;
        float angle;
        float speed;  // Angular velocity
        float size;
        size_t bandIndex;  // Which band this particle belongs to
    };

    std::vector<RingParticle> m_particles;

    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_instanceVBO = 0;

    ShaderProgram m_shader;
    int m_particleCount = 0;
    float m_planetRadius = 1.0f;
};

}  // namespace astrocore
