#include "render/OrbitRenderer.hpp"
#include <glm/gtc/type_ptr.hpp>

namespace astrocore {

OrbitRenderer::OrbitRenderer() = default;

OrbitRenderer::~OrbitRenderer() {
    shutdown();
}

void OrbitRenderer::init() {
    if (m_initialized) return;

    createShader();

    // Create VAO and VBO for trail rendering
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    // Reserve space for trail points (position + color + alpha)
    glBufferData(GL_ARRAY_BUFFER, MAX_TRAIL_POINTS * MAX_BODIES * 7 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    // Position (vec3)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), nullptr);

    // Color (vec3)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));

    // Alpha (float)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 7 * sizeof(float), reinterpret_cast<void*>(6 * sizeof(float)));

    glBindVertexArray(0);

    m_initialized = true;
}

void OrbitRenderer::shutdown() {
    if (!m_initialized) return;

    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);

    m_vao = m_vbo = 0;
    m_trails.clear();
    m_initialized = false;
}

void OrbitRenderer::createShader() {
    const char* vertexSource = R"(
#version 410 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
layout(location = 2) in float aAlpha;

uniform mat4 uViewProjection;

out vec3 vColor;
out float vAlpha;

void main() {
    vColor = aColor;
    vAlpha = aAlpha;
    gl_Position = uViewProjection * vec4(aPos, 1.0);
}
)";

    const char* fragmentSource = R"(
#version 410 core

in vec3 vColor;
in float vAlpha;

out vec4 FragColor;

void main() {
    // Bright glowing trails
    vec3 glowColor = vColor * 4.0;
    float glow = vAlpha;
    FragColor = vec4(glowColor * glow, glow);
}
)";

    m_shader.loadFromSource(vertexSource, fragmentSource);
}

void OrbitRenderer::recordPosition(uint64_t bodyId, const glm::dvec3& physicsPosition, const glm::vec3& color) {
    auto& trail = m_trails[bodyId];
    trail.color = color;

    // Add new point (store physics position, convert at render time)
    TrailPoint point;
    point.physicsPosition = physicsPosition;
    point.color = color;
    point.alpha = 1.0f;
    trail.points.push_back(point);

    // Limit trail length
    while (trail.points.size() > MAX_TRAIL_POINTS) {
        trail.points.pop_front();
    }

    // Update alpha values (fade out older points)
    float alphaStep = 1.0f / static_cast<float>(trail.points.size());
    float alpha = alphaStep;
    for (auto& p : trail.points) {
        p.alpha = alpha;
        alpha += alphaStep;
    }
}

void OrbitRenderer::clearTrails() {
    m_trails.clear();
}

void OrbitRenderer::render(const glm::mat4& viewProjection, const glm::dvec3& focusPos, double scaleFactor) {
    if (!m_initialized || m_trails.empty()) return;

    // Build vertex data - convert physics positions to render positions
    std::vector<float> vertices;
    vertices.reserve(MAX_TRAIL_POINTS * MAX_BODIES * 7);

    for (const auto& [bodyId, trail] : m_trails) {
        if (trail.points.size() < 2) continue;

        for (const auto& point : trail.points) {
            // Convert physics position to render position (relative to current focus)
            // Use exact same calculation as planet rendering in Application::render()
            glm::dvec3 relativePos = point.physicsPosition - focusPos;
            glm::vec3 renderPos = glm::vec3(relativePos * scaleFactor);

            vertices.push_back(renderPos.x);
            vertices.push_back(renderPos.y);
            vertices.push_back(renderPos.z);
            vertices.push_back(point.color.r);
            vertices.push_back(point.color.g);
            vertices.push_back(point.color.b);
            vertices.push_back(point.alpha);
        }
    }

    if (vertices.empty()) return;

    // Upload vertex data
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(float), vertices.data());

    // Render trails with glow effect
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // Additive blending for glow
    glEnable(GL_LINE_SMOOTH);
    glLineWidth(4.0f);

    m_shader.use();
    m_shader.setMat4("uViewProjection", viewProjection);

    glBindVertexArray(m_vao);

    // Draw each trail as a line strip
    int offset = 0;
    for (const auto& [bodyId, trail] : m_trails) {
        if (trail.points.size() < 2) continue;
        int count = static_cast<int>(trail.points.size());
        glDrawArrays(GL_LINE_STRIP, offset, count);
        offset += count;
    }

    glBindVertexArray(0);
    m_shader.unuse();

    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_BLEND);
}

} // namespace astrocore
