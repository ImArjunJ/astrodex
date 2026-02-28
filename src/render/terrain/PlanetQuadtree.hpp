#pragma once

#include "render/terrain/QuadtreeNode.hpp"
#include "render/terrain/TerrainMesh.hpp"
#include <array>
#include <memory>

namespace astrocore {

class Camera;
class ShaderProgram;

// Manages quadtree LOD for entire planet with instanced rendering
class PlanetQuadtree {
public:
    PlanetQuadtree();
    ~PlanetQuadtree();

    void init(const glm::vec3& center, float radius);
    void update(const Camera& camera);
    void render(ShaderProgram& shader) const;

    glm::vec3 getCenter() const { return m_center; }
    float getRadius() const { return m_radius; }

    struct Stats {
        int visibleNodes = 0;
        int totalInstances = 0;
    };
    Stats getStats() const;

private:
    glm::vec3 m_center{0.0f};
    float m_radius = 1.0f;

    std::array<std::unique_ptr<QuadtreeNode>, 6> m_roots;
    mutable TerrainMesh m_mesh;

    mutable Stats m_lastStats;
};

}  // namespace astrocore
