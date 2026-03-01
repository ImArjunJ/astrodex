#pragma once

#include <imgui.h>
#include <functional>
#include <string>

struct GLFWwindow;

namespace astrocore {

struct PlanetParams;

enum class Theme { Dark, Light };

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

    // Returns preset PlanetParams by index (0=Earth … 7=Alien)
    static PlanetParams getPreset(int index);

    // True for exactly one call after the "← Galaxy" back button is pressed
    bool wasBackPressed();

    // Sync the current theme to a GalaxyView (call after init)
    void applyThemeTo(Theme t);

    // Persistent top-right pill toggle — call every frame (galaxy + planet screens)
    void renderThemeToggle();

    Theme getTheme() const { return m_theme; }

private:
    void setupStyle();

    bool  m_initialized  = false;
    int   m_presetIndex  = 0;
    Theme m_theme        = Theme::Dark;
    bool  m_backPressed  = false;
    float m_toggleAnimT  = 0.f;   // 0 = dark side, 1 = light side (animated)

    // Exoplanet search state
    char  m_searchBuf[256] = {};
    std::string m_exoStatus = "Select a planet from the Galaxy view.";
    std::function<void(const std::string&)> m_exoCallback;
};

}  // namespace astrocore
