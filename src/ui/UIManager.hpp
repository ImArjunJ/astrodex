#pragma once

#include <imgui.h>
#include <functional>
#include <string>
#include <vector>

struct GLFWwindow;

namespace astrocore {

struct PlanetParams;
struct ExoplanetData;
class VulkanRenderer;

enum class Theme { Dark, Light };

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

    // Called by Application to populate autocomplete suggestions
    void setCachedNames(const std::vector<std::string>& names);

    // Called by Application to disable input during pipeline execution
    void setLoading(bool loading);

    // Called by Application to pass loaded exoplanet data for info panel
    void setExoplanetData(const ExoplanetData* data);

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

    // Autocomplete state
    std::vector<std::string> m_cachedNames;
    bool m_isLoading = false;
    bool m_acOpen = false;

    // Exoplanet data for info panel (owned by Application)
    const ExoplanetData* m_exoData = nullptr;
};

}  // namespace astrocore
