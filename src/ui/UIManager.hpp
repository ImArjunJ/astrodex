#pragma once

#include <imgui.h>
#include <functional>
#include <string>

struct GLFWwindow;

namespace astrocore {

struct PlanetParams;

class UIManager {
public:
    UIManager();
    ~UIManager();

#ifdef ASTRO_METAL
    void init(GLFWwindow* window, void* metalDevice);
    void beginFrame(void* renderPassDescriptor);
    void endFrame(void* commandBuffer, void* commandEncoder);
#else
    void init(GLFWwindow* window);
    void beginFrame();
    void endFrame();
#endif

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
