#pragma once

#include <imgui.h>

struct GLFWwindow;

namespace astrocore {

struct PlanetParams;
class VulkanRenderer;

class UIManager {
public:
    UIManager();
    ~UIManager();

    void init(GLFWwindow* window, VulkanRenderer* renderer);
    void beginFrame();
    void endFrame(VulkanRenderer* renderer);

    void shutdown();
    void render(PlanetParams& params);

private:
    void setupStyle();

    bool m_initialized = false;
    int  m_presetIndex = 0;
};

}  // namespace astrocore
