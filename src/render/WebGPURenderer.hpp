#pragma once

#include "render/RendererBase.hpp"

#ifdef __EMSCRIPTEN__
#include <webgpu/webgpu.h>
#endif

#include <memory>

namespace astrocore {

class Camera;

class WebGPURenderer : public RendererBase {
public:
    WebGPURenderer();
    ~WebGPURenderer();

    WebGPURenderer(const WebGPURenderer&) = delete;
    WebGPURenderer& operator=(const WebGPURenderer&) = delete;

    void init(int width, int height, void* glfwWindow = nullptr) override;
    void resize(int width, int height) override;

    void beginFrame() override;
    void render(const Camera& camera) override;
    void endFrame() override;

#ifdef __EMSCRIPTEN__
    // WebGPU accessors for ImGui integration
    WGPUDevice getDevice();
    WGPUTextureFormat getSurfaceFormat();
#endif

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace astrocore
