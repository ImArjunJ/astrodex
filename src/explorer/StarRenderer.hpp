#pragma once

#include <glad/gl.h>
#include "render/ShaderProgram.hpp"
#include <vector>
#include <cstdint>

struct GLFWwindow;

namespace astrocore {

struct StarVertex;
class FreeFlyCamera;

class StarRenderer {
public:
    StarRenderer();
    ~StarRenderer();

    StarRenderer(const StarRenderer&) = delete;
    StarRenderer& operator=(const StarRenderer&) = delete;

    void init(int width, int height, GLFWwindow* window);
    void uploadStars(const std::vector<StarVertex>& stars);      // static upload (HYG fallback)
    void updateStars(const std::vector<StarVertex>& stars);      // dynamic per-frame update
    void resize(int width, int height);

    void beginFrame();
    void render(const FreeFlyCamera& camera, float time, float pointScale, float brightnessBoost, bool debugMode = false, float warpFactor = 0.0f);
    void endFrame();

private:
    ShaderProgram m_shader;

    // Static vertex buffer (HYG fallback)
    GLuint m_staticVAO = 0;
    GLuint m_staticVBO = 0;
    uint32_t m_staticStarCount = 0;

    // Dynamic vertex buffer (per-frame LOD updates)
    GLuint m_dynamicVAO = 0;
    GLuint m_dynamicVBO = 0;
    uint32_t m_dynamicStarCount = 0;
    bool m_useDynamic = false;

    static constexpr uint32_t MAX_DYNAMIC_STARS = 2'000'000;

    int m_width = 0;
    int m_height = 0;

    void setupVAO(GLuint vao, GLuint vbo);
};

} // namespace astrocore
