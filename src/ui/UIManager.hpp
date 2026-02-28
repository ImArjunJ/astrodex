#pragma once

#include <imgui.h>

struct GLFWwindow;

namespace astrocore {

struct PlanetParams;

class UIManager {
public:
    UIManager();
    ~UIManager();

#ifdef ASTRO_METAL
    // Metal path — also needs the MTLDevice to init ImGui Metal backend.
    void init(GLFWwindow* window, void* metalDevice);

    // beginFrame needs the MTLRenderPassDescriptor for ImGui_ImplMetal_NewFrame.
    void beginFrame(void* renderPassDescriptor);

    // endFrame renders ImGui draw data into the active Metal encoder.
    void endFrame(void* commandBuffer, void* commandEncoder);
#else
    // OpenGL path
    void init(GLFWwindow* window);
    void beginFrame();
    void endFrame();
#endif

    void shutdown();
    void render(PlanetParams& params);

private:
    void setupStyle();

    bool m_initialized = false;
    int  m_presetIndex = 0;
};

}  // namespace astrocore
