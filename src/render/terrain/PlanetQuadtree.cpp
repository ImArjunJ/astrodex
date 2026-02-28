#include "render/terrain/PlanetQuadtree.hpp"
#include "render/ShaderProgram.hpp"
#include "render/Camera.hpp"
#include "core/Logger.hpp"

namespace astrocore {

PlanetQuadtree::PlanetQuadtree() = default;
PlanetQuadtree::~PlanetQuadtree() = default;

void PlanetQuadtree::init(const glm::vec3& center, float radius) {
    m_center = center;
    m_radius = radius;

    // Initialize shared mesh (one-time setup)
    m_mesh.init();

    // Create 6 root nodes for cube faces
    for (int face = 0; face < 6; ++face) {
        m_roots[face] = std::make_unique<QuadtreeNode>(
            face, 0,
            glm::vec2(-1.0f, -1.0f),
            glm::vec2(1.0f, 1.0f),
            nullptr
        );
    }

    LOG_INFO("PlanetQuadtree initialized: 6 faces, instanced rendering");
}

void PlanetQuadtree::update(const Camera& camera) {
    int totalVisible = 0;

    for (auto& root : m_roots) {
        if (root) {
            totalVisible += root->update(camera, m_center, m_radius);
        }
    }

    m_lastStats.visibleNodes = totalVisible;
}

void PlanetQuadtree::render(ShaderProgram& shader) const {
    // Start fresh frame
    m_mesh.beginFrame();

    // Collect all visible leaf nodes into instance buffer
    for (const auto& root : m_roots) {
        if (root) {
            // Need non-const camera for frustum check - using a dummy for collection
            // The actual visibility was already determined in update()
            root->collectVisibleNodes(m_mesh, Camera(), m_center, m_radius);
        }
    }

    m_lastStats.totalInstances = m_mesh.getInstanceCount();

    // Set uniforms
    shader.setVec3("uPlanetCenter", m_center);
    shader.setFloat("uPlanetRadius", m_radius);

    // Single draw call for all patches!
    m_mesh.render();
}

PlanetQuadtree::Stats PlanetQuadtree::getStats() const {
    return m_lastStats;
}

}  // namespace astrocore
