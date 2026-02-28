#pragma once

#ifndef ASTRO_METAL

#include "render/IRenderer.hpp"
#include "render/ShaderProgram.hpp"
#include <glad/gl.h>

namespace astrocore {

class Renderer : public IRenderer {
public:
    Renderer();
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void init(int width, int height, void* nativeWindow = nullptr) override;
    void resize(int width, int height) override;

    void beginFrame() override;
    void render(const Camera& camera) override;
    void endFrame() override;

    PlanetParams& params() override { return m_params; }

private:
    void createQuad();
    void generateNoiseTexture(int size);

    int m_width = 0;
    int m_height = 0;

    GLuint m_quadVAO    = 0;
    GLuint m_quadVBO    = 0;
    GLuint m_noiseTexture = 0;

    ShaderProgram m_shader;
    PlanetParams  m_params;
    float         m_time = 0.0f;
};

}  // namespace astrocore

#endif  // ASTRO_METAL
