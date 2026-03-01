#include "explorer/StarRenderer.hpp"
#include "explorer/StarData.hpp"
#include "explorer/FreeFlyCamera.hpp"
#include "core/Logger.hpp"

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstring>
#include <stdexcept>

namespace astrocore {

StarRenderer::StarRenderer() = default;

StarRenderer::~StarRenderer() {
    if (m_staticVBO)  glDeleteBuffers(1, &m_staticVBO);
    if (m_staticVAO)  glDeleteVertexArrays(1, &m_staticVAO);
    if (m_dynamicVBO) glDeleteBuffers(1, &m_dynamicVBO);
    if (m_dynamicVAO) glDeleteVertexArrays(1, &m_dynamicVAO);
}

void StarRenderer::setupVAO(GLuint vao, GLuint vbo) {
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    // StarVertex: position(vec3) + magnitude(float) + color(vec3) = 28 bytes
    GLsizei stride = 28;

    // location 0: position (vec3, offset 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(0));

    // location 1: magnitude (float, offset 12)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(12));

    // location 2: color (vec3, offset 16)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(16));

    glBindVertexArray(0);
}

void StarRenderer::init(int width, int height, GLFWwindow* /*window*/) {
    m_width = width;
    m_height = height;

    // Load shaders
    if (!m_shader.loadFromFiles("shaders/star.vert", "shaders/star.frag")) {
        throw std::runtime_error("Failed to load star shaders");
    }

    // Static VAO/VBO (for HYG fallback)
    glGenVertexArrays(1, &m_staticVAO);
    glGenBuffers(1, &m_staticVBO);
    setupVAO(m_staticVAO, m_staticVBO);

    // Dynamic VAO/VBO (for per-frame LOD updates)
    glGenVertexArrays(1, &m_dynamicVAO);
    glGenBuffers(1, &m_dynamicVBO);

    // Pre-allocate dynamic buffer
    glBindBuffer(GL_ARRAY_BUFFER, m_dynamicVBO);
    glBufferData(GL_ARRAY_BUFFER, MAX_DYNAMIC_STARS * sizeof(StarVertex),
                 nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    setupVAO(m_dynamicVAO, m_dynamicVBO);

    LOG_INFO("Star renderer initialized (OpenGL, {}x{})", width, height);
}

void StarRenderer::uploadStars(const std::vector<StarVertex>& stars) {
    if (stars.empty()) return;

    m_staticStarCount = static_cast<uint32_t>(stars.size());

    glBindBuffer(GL_ARRAY_BUFFER, m_staticVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(stars.size() * sizeof(StarVertex)),
                 stars.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    LOG_INFO("Uploaded {} stars to GPU ({} KB)", m_staticStarCount,
             stars.size() * sizeof(StarVertex) / 1024);
}

void StarRenderer::updateStars(const std::vector<StarVertex>& stars) {
    uint32_t count = std::min(static_cast<uint32_t>(stars.size()), MAX_DYNAMIC_STARS);
    if (count == 0) {
        m_dynamicStarCount = 0;
        m_useDynamic = true;
        return;
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_dynamicVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<GLsizeiptr>(count * sizeof(StarVertex)),
                    stars.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    m_dynamicStarCount = count;
    m_useDynamic = true;
}

void StarRenderer::resize(int width, int height) {
    m_width = width;
    m_height = height;
}

void StarRenderer::beginFrame() {
    glViewport(0, 0, m_width, m_height);
    glClearColor(0.0f, 0.0f, 0.005f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void StarRenderer::render(const FreeFlyCamera& camera, float time,
                           float pointScale, float brightnessBoost, bool debugMode, float warpFactor) {
    // Enable point sprites and additive blending
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); // additive blending
    glDisable(GL_DEPTH_TEST);

    m_shader.use();

    // Set uniforms
    m_shader.setMat4("uView", camera.getViewMatrix());
    m_shader.setMat4("uProjection", camera.getProjectionMatrix());
    m_shader.setFloat("uPointScale", pointScale);
    m_shader.setFloat("uBrightnessBoost", brightnessBoost);
    m_shader.setFloat("uDebugMode", debugMode ? 1.0f : 0.0f);
    m_shader.setFloat("uWarpFactor", warpFactor);

    // Choose dynamic (LOD) or static (HYG) vertex buffer
    if (m_useDynamic && m_dynamicStarCount > 0) {
        glBindVertexArray(m_dynamicVAO);
        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(m_dynamicStarCount));
    } else if (m_staticStarCount > 0) {
        glBindVertexArray(m_staticVAO);
        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(m_staticStarCount));
    }

    glBindVertexArray(0);
    m_shader.unuse();

    // Restore state
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void StarRenderer::endFrame() {
    // Nothing needed — GLFW handles swap
}

} // namespace astrocore
