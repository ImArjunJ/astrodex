#pragma once

#include "render/ShaderProgram.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <cstdint>

namespace astrocore {

// 3D Galaxy Renderer - renders a navigable spiral galaxy
class Galaxy3DRenderer {
public:
    struct GalaxyStar {
        glm::vec3 position;
        glm::vec3 color;
        float size;
        float brightness;
    };

    struct GalaxyObject {
        std::string name;
        std::string type;
        glm::vec3 position;
        glm::vec3 color;
        float size;
        int id;          // Index in planet list
        bool isSelected;
    };

    Galaxy3DRenderer() = default;
    ~Galaxy3DRenderer();

    void init();
    void shutdown();

    // Generate galaxy structure
    void generateGalaxy(int numStars = 50000, int numArms = 4);

    // Add a planet/exoplanet to the galaxy
    void addObject(const std::string& name, const std::string& type,
                   float distanceLY, const glm::vec3& color, int id);

    // Set which object is selected
    void setSelectedObject(int id);
    int getSelectedObject() const { return m_selectedId; }

    // Get object at screen position (for mouse picking)
    int pickObject(const glm::vec2& screenPos, const glm::mat4& viewProj,
                   const glm::vec2& screenSize) const;

    // Render the galaxy
    void render(const glm::mat4& viewProjection, const glm::vec3& cameraPos, float time);

    // Get camera target for flying to an object
    glm::vec3 getObjectPosition(int id) const;

    // Clear all objects (keep stars)
    void clearObjects();

    // Galaxy bounds for camera limits
    float getGalaxyRadius() const { return m_galaxyRadius; }

private:
    void uploadStarData();
    void uploadObjectData();

    ShaderProgram m_shader;

    // Stars (background galaxy)
    std::vector<GalaxyStar> m_stars;
    uint32_t m_starVAO = 0;
    uint32_t m_starVBO = 0;
    bool m_starsDirty = true;

    // Objects (planets)
    std::vector<GalaxyObject> m_objects;
    uint32_t m_objectVAO = 0;
    uint32_t m_objectVBO = 0;
    bool m_objectsDirty = true;

    int m_selectedId = -1;
    float m_galaxyRadius = 1000.0f;  // Light years scale (visual)

    bool m_initialized = false;
};

}  // namespace astrocore
