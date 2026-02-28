#pragma once

#include <imgui.h>
#include <string>
#include <functional>
#include "render/RingRenderer.hpp"

struct GLFWwindow;

namespace astrocore {

struct PlanetParams;

class UIManager {
public:
    UIManager();
    ~UIManager();

    // Initialize ImGui with GLFW/OpenGL
    void init(GLFWwindow* window);
    void shutdown();

    // Frame lifecycle
    void beginFrame();
    void endFrame();

    // Render planet editor UI — edits params directly
    void render(PlanetParams& params);
    void renderPlanetEditor(const std::string& bodyName, PlanetParams& params,
                            RingParams* ringParams = nullptr,
                            std::function<void()> onRingChanged = nullptr);

private:
    void setupStyle();

    bool m_initialized = false;
    int m_presetIndex = 0;
};

}  // namespace astrocore
