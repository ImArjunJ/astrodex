#pragma once

#include <imgui.h>
#include <functional>
#include <string>

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
    // outPos/outSize are filled with the actual ImGui window rect this frame
    void render(PlanetParams& params,
                ImVec2* outPos  = nullptr,
                ImVec2* outSize = nullptr);

    // Called by Application to register the async planet-load trigger
    void setExoplanetCallback(std::function<void(const std::string&)> onLoad);

    // Called by Application on the main thread with load status updates
    void setExoplanetStatus(const std::string& status);

private:
    void setupStyle();

    bool m_initialized = false;
    int  m_presetIndex = 0;

    // Exoplanet search state
    char  m_searchBuf[256] = {};
    std::string m_exoStatus = "Enter a planet name and press Load.";
    std::function<void(const std::string&)> m_exoCallback;
};

}  // namespace astrocore
