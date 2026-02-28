#pragma once

#include "render/terrain/PlanetQuadtree.hpp"
#include "render/terrain/HeightmapCache.hpp"
#include "render/terrain/TerrainMesh.hpp"
#include "render/ShaderProgram.hpp"
#include "render/PlanetParameters.hpp"
#include <glm/glm.hpp>
#include <memory>

namespace astrocore {

class Camera;

// Main integration point for tessellated terrain rendering
// Manages quadtree, shaders, and terrain generation
class TerrainRenderer {
public:
    TerrainRenderer();
    ~TerrainRenderer();

    // Initialize terrain system
    void init(const glm::vec3& planetCenter, float planetRadius);

    // Update LOD based on camera
    void update(const Camera& camera);

    // Render terrain
    void render(const Camera& camera, const PlanetParameters& params);

    // Check if terrain system is ready
    bool isReady() const { return m_initialized; }

    // Get statistics for debugging
    struct Stats {
        int totalNodes = 0;
        int leafNodes = 0;
        int maxLevel = 0;
        int trianglesRendered = 0;
    };
    Stats getStats() const;

    // Configuration
    void setTessellationFactor(float factor) { m_tessellationFactor = factor; }
    float getTessellationFactor() const { return m_tessellationFactor; }

    // Debug options
    void setWireframe(bool enabled) { m_wireframe = enabled; }
    bool isWireframe() const { return m_wireframe; }

private:
    void setupUniforms(const Camera& camera, const PlanetParameters& params);

    PlanetQuadtree m_quadtree;
    HeightmapCache m_heightmapCache;
    ShaderProgram m_shader;

    glm::vec3 m_planetCenter{0.0f};
    float m_planetRadius = 1.0f;
    float m_tessellationFactor = 1.0f;

    bool m_initialized = false;
    bool m_wireframe = false;
};

}  // namespace astrocore
