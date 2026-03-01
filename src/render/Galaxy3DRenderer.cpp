#include "render/Galaxy3DRenderer.hpp"
#include "core/Logger.hpp"
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <random>
#include <cmath>
#include <algorithm>

namespace astrocore {

Galaxy3DRenderer::~Galaxy3DRenderer() {
    shutdown();
}

void Galaxy3DRenderer::init() {
    if (m_initialized) return;

    // Load shader
    if (!m_shader.loadFromFiles("shaders/galaxy.vert", "shaders/galaxy.frag")) {
        LOG_ERROR("Failed to load galaxy shader");
        return;
    }

    // Create VAOs/VBOs for stars
    glGenVertexArrays(1, &m_starVAO);
    glGenBuffers(1, &m_starVBO);

    // Create VAOs/VBOs for objects
    glGenVertexArrays(1, &m_objectVAO);
    glGenBuffers(1, &m_objectVBO);

    m_initialized = true;
    LOG_INFO("Galaxy3DRenderer initialized");
}

void Galaxy3DRenderer::shutdown() {
    if (m_starVAO) glDeleteVertexArrays(1, &m_starVAO);
    if (m_starVBO) glDeleteBuffers(1, &m_starVBO);
    if (m_objectVAO) glDeleteVertexArrays(1, &m_objectVAO);
    if (m_objectVBO) glDeleteBuffers(1, &m_objectVBO);
    m_starVAO = m_starVBO = m_objectVAO = m_objectVBO = 0;
    m_initialized = false;
}

void Galaxy3DRenderer::generateGalaxy(int numStars, int numArms) {
    m_stars.clear();
    m_stars.reserve(static_cast<size_t>(numStars));

    std::mt19937 rng(42);
    auto frand = [&](float lo, float hi) {
        return lo + std::uniform_real_distribution<float>(0.f, 1.f)(rng) * (hi - lo);
    };

    const float armSpread = 0.4f;      // How spread out each arm is
    const float armOffset = 2.0f * 3.14159f / static_cast<float>(numArms);
    const float rotationFactor = 4.0f; // How much arms wind

    for (int i = 0; i < numStars; ++i) {
        GalaxyStar star;

        // Random distance from center (more stars near center)
        float distFromCenter = frand(0.0f, 1.0f);
        distFromCenter = std::pow(distFromCenter, 0.5f);  // Concentrate toward center
        float distance = distFromCenter * m_galaxyRadius;

        // Choose an arm
        int arm = static_cast<int>(frand(0.f, static_cast<float>(numArms)));
        float armAngle = static_cast<float>(arm) * armOffset;

        // Spiral angle increases with distance
        float spiralAngle = armAngle + distance * rotationFactor / m_galaxyRadius;

        // Add spread perpendicular to arm
        float spread = armSpread * (1.0f + distance / m_galaxyRadius);
        float angleOffset = frand(-spread, spread);
        spiralAngle += angleOffset;

        // Height above/below galactic plane (disk is thin)
        float heightScale = 0.05f + 0.1f * distFromCenter;  // Thicker at edges
        float height = frand(-1.f, 1.f) * heightScale * m_galaxyRadius;

        // Add some stars to bulge (spherical central region)
        if (frand(0.f, 1.f) < 0.15f && distFromCenter < 0.2f) {
            // Bulge star - more spherical distribution
            float theta = frand(0.f, 2.f * 3.14159f);
            float phi = std::acos(frand(-1.f, 1.f));
            float r = frand(0.f, 0.2f) * m_galaxyRadius;
            star.position = glm::vec3(
                r * std::sin(phi) * std::cos(theta),
                r * std::cos(phi),
                r * std::sin(phi) * std::sin(theta)
            );
        } else {
            // Disk star
            star.position = glm::vec3(
                distance * std::cos(spiralAngle),
                height,
                distance * std::sin(spiralAngle)
            );
        }

        // Star color based on position (older stars near center, younger in arms)
        float temp = frand(0.f, 1.f);
        if (distFromCenter < 0.3f) {
            // Central bulge - older, redder stars
            star.color = glm::vec3(
                frand(0.9f, 1.0f),
                frand(0.7f, 0.9f),
                frand(0.5f, 0.7f)
            );
        } else if (temp < 0.7f) {
            // Main sequence - white/yellow
            star.color = glm::vec3(
                frand(0.9f, 1.0f),
                frand(0.9f, 1.0f),
                frand(0.85f, 1.0f)
            );
        } else if (temp < 0.9f) {
            // Orange/red dwarfs
            star.color = glm::vec3(
                frand(0.95f, 1.0f),
                frand(0.6f, 0.8f),
                frand(0.4f, 0.6f)
            );
        } else {
            // Blue giants (rare, in spiral arms)
            star.color = glm::vec3(
                frand(0.7f, 0.9f),
                frand(0.8f, 0.95f),
                frand(0.95f, 1.0f)
            );
        }

        // Size varies
        star.size = frand(1.0f, 4.0f);
        if (frand(0.f, 1.f) < 0.02f) star.size *= 2.0f;  // Some bigger stars

        star.brightness = frand(0.5f, 1.0f);

        m_stars.push_back(star);
    }

    m_starsDirty = true;
    LOG_INFO("Generated {} galaxy stars with {} arms", static_cast<int>(m_stars.size()), numArms);
}

void Galaxy3DRenderer::addObject(const std::string& name, const std::string& type,
                                  float distanceLY, const glm::vec3& color, int id) {
    GalaxyObject obj;
    obj.name = name;
    obj.type = type;
    obj.color = color;
    obj.size = 8.0f;
    obj.id = id;
    obj.isSelected = false;

    // Position based on distance - place in a realistic location
    // For now, use a hash of the name to get a consistent position
    std::hash<std::string> hasher;
    size_t hash = hasher(name);

    // Random angle based on hash
    float angle = static_cast<float>(hash % 1000) / 1000.0f * 2.0f * 3.14159f;
    float height_angle = static_cast<float>((hash >> 10) % 1000) / 1000.0f * 0.2f - 0.1f;

    // Distance scaling: real distances can be huge, so we use log scale
    // Distance 0 = near center, larger = further out
    float scaledDist;
    if (distanceLY <= 0.0f) {
        // Unknown distance - place randomly in mid-range
        scaledDist = m_galaxyRadius * (0.3f + static_cast<float>((hash >> 20) % 1000) / 2000.0f);
    } else {
        // Log scale: 10 LY -> ~100, 100 LY -> ~300, 1000 LY -> ~500, etc.
        scaledDist = std::min(m_galaxyRadius * 0.9f,
                              100.0f + std::log10(distanceLY + 1.0f) * 200.0f);
    }

    // Place in the spiral structure (add some spiral offset based on distance)
    float spiralOffset = scaledDist * 0.003f;
    angle += spiralOffset;

    obj.position = glm::vec3(
        scaledDist * std::cos(angle),
        scaledDist * height_angle,
        scaledDist * std::sin(angle)
    );

    // Size based on planet type
    if (type.find("Gas") != std::string::npos || type.find("Giant") != std::string::npos) {
        obj.size = 12.0f;
    } else if (type.find("Neptune") != std::string::npos) {
        obj.size = 10.0f;
    } else {
        obj.size = 8.0f;
    }

    m_objects.push_back(obj);
    m_objectsDirty = true;
}

void Galaxy3DRenderer::setSelectedObject(int id) {
    if (m_selectedId != id) {
        m_selectedId = id;
        for (auto& obj : m_objects) {
            obj.isSelected = (obj.id == id);
        }
        m_objectsDirty = true;
    }
}

void Galaxy3DRenderer::clearObjects() {
    m_objects.clear();
    m_objectsDirty = true;
    m_selectedId = -1;
}

glm::vec3 Galaxy3DRenderer::getObjectPosition(int id) const {
    for (const auto& obj : m_objects) {
        if (obj.id == id) return obj.position;
    }
    return glm::vec3(0.0f);
}

int Galaxy3DRenderer::pickObject(const glm::vec2& screenPos, const glm::mat4& viewProj,
                                  const glm::vec2& screenSize) const {
    float bestDist = 30.0f;  // Picking radius in pixels
    int bestId = -1;

    for (const auto& obj : m_objects) {
        // Project to screen space
        glm::vec4 clip = viewProj * glm::vec4(obj.position, 1.0f);
        if (clip.w <= 0.0f) continue;  // Behind camera

        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        glm::vec2 screen = glm::vec2(
            (ndc.x + 1.0f) * 0.5f * screenSize.x,
            (1.0f - ndc.y) * 0.5f * screenSize.y  // Y is flipped
        );

        float dist = glm::length(screen - screenPos);
        if (dist < bestDist) {
            bestDist = dist;
            bestId = obj.id;
        }
    }

    return bestId;
}

void Galaxy3DRenderer::uploadStarData() {
    if (!m_starsDirty || m_stars.empty()) return;

    // Pack star data: position (3) + color (3) + size (1) + type (1) = 8 floats
    std::vector<float> data;
    data.reserve(m_stars.size() * 8);

    for (const auto& star : m_stars) {
        data.push_back(star.position.x);
        data.push_back(star.position.y);
        data.push_back(star.position.z);
        data.push_back(star.color.r * star.brightness);
        data.push_back(star.color.g * star.brightness);
        data.push_back(star.color.b * star.brightness);
        data.push_back(star.size);
        data.push_back(0.0f);  // Type = star
    }

    glBindVertexArray(m_starVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_starVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.size() * sizeof(float)),
                 data.data(), GL_STATIC_DRAW);

    // Position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), nullptr);
    // Color + size
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          reinterpret_cast<void*>(3 * sizeof(float)));
    // Type
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          reinterpret_cast<void*>(7 * sizeof(float)));

    glBindVertexArray(0);
    m_starsDirty = false;
}

void Galaxy3DRenderer::uploadObjectData() {
    if (!m_objectsDirty) return;

    if (m_objects.empty()) {
        return;
    }

    std::vector<float> data;
    data.reserve(m_objects.size() * 8);

    for (const auto& obj : m_objects) {
        data.push_back(obj.position.x);
        data.push_back(obj.position.y);
        data.push_back(obj.position.z);
        data.push_back(obj.color.r);
        data.push_back(obj.color.g);
        data.push_back(obj.color.b);
        data.push_back(obj.size);
        data.push_back(obj.isSelected ? 2.0f : 1.0f);  // Type = planet or selected
    }

    glBindVertexArray(m_objectVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_objectVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.size() * sizeof(float)),
                 data.data(), GL_DYNAMIC_DRAW);

    // Position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), nullptr);
    // Color + size
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          reinterpret_cast<void*>(3 * sizeof(float)));
    // Type
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          reinterpret_cast<void*>(7 * sizeof(float)));

    glBindVertexArray(0);
    m_objectsDirty = false;
}

void Galaxy3DRenderer::render(const glm::mat4& viewProjection, const glm::vec3& cameraPos, float time) {
    if (!m_initialized) return;

    // Upload data if needed
    uploadStarData();
    uploadObjectData();

    // Enable point sprites and blending
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // Additive blending for glow
    glDepthMask(GL_FALSE);  // Don't write to depth buffer

    m_shader.use();
    m_shader.setMat4("uViewProjection", viewProjection);
    m_shader.setVec3("uCameraPos", cameraPos);
    m_shader.setFloat("uTime", time);

    // Draw stars first (background)
    if (!m_stars.empty()) {
        glBindVertexArray(m_starVAO);
        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(m_stars.size()));
    }

    // Draw objects (planets) - on top
    if (!m_objects.empty()) {
        glBindVertexArray(m_objectVAO);
        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(m_objects.size()));
    }

    glBindVertexArray(0);
    m_shader.unuse();

    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_PROGRAM_POINT_SIZE);
}

}  // namespace astrocore
