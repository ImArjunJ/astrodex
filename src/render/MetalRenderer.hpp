#pragma once

#ifdef ASTRO_METAL

#include "render/IRenderer.hpp"
#include <memory>

namespace astrocore {

class Camera;

// Pimpl hides all Objective-C / Metal API types from C++ translation units.
class MetalRenderer : public IRenderer {
public:
    MetalRenderer();
    ~MetalRenderer();

    MetalRenderer(const MetalRenderer&) = delete;
    MetalRenderer& operator=(const MetalRenderer&) = delete;

    void init(int width, int height, void* glfwWindow = nullptr) override;
    void resize(int width, int height) override;

    void beginFrame() override;
    void render(const Camera& camera) override;
    void endFrame() override;

    PlanetParams& params() override;

    MetalFrameContext getMetalContext() override;
    void* getMetalDevice() override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace astrocore

#endif  // ASTRO_METAL
