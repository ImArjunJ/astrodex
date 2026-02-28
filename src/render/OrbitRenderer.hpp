#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>
#include "render/ShaderProgram.hpp"
#include <vector>
#include <deque>
#include <unordered_map>

namespace astrocore {

// Renders orbital trails for celestial bodies
class OrbitRenderer {
public:
    static constexpr int MAX_TRAIL_POINTS = 1000;
    static constexpr int MAX_BODIES = 64;

    OrbitRenderer();
    ~OrbitRenderer();

    void init();
    void shutdown();

    // Record a position for a body's trail
    // Record position in world/physics coordinates (will be converted at render time)
    void recordPosition(uint64_t bodyId, const glm::dvec3& physicsPosition, const glm::vec3& color);

    // Clear all trails
    void clearTrails();

    // Render all trails - needs focus position and scale to convert physics to render coords
    void render(const glm::mat4& viewProjection, const glm::dvec3& focusPos, double scaleFactor);

    // Clear all accumulated trail data
    void clear() { m_trails.clear(); }

private:
    void createShader();

    struct TrailPoint {
        glm::dvec3 physicsPosition;  // Store in physics coords, convert at render time
        glm::vec3 color;
        float alpha;
    };

    struct Trail {
        std::deque<TrailPoint> points;
        glm::vec3 color;
    };

    std::unordered_map<uint64_t, Trail> m_trails;

    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    ShaderProgram m_shader;

    bool m_initialized = false;
};

} // namespace astrocore
