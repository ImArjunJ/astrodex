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

private:
    void setupStyle();

    bool m_initialized = false;
    int  m_presetIndex = 0;

    // Exoplanet search state
    char  m_searchBuf[256] = {};
    std::string m_exoStatus = "Enter a planet name and press Load.";
    std::function<void(const std::string&)> m_exoCallback;

    // Autocomplete state
    std::vector<std::string> m_cachedNames;  // sorted list of planet names for autocomplete

    // Loading state
    bool m_isLoading = false;  // disables search input and Load button when true

    // Exoplanet data for info panel (owned by Application)
    const ExoplanetData* m_exoData = nullptr;
};

}  // namespace astrocore
