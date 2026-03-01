#pragma once

#include <imgui.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "render/RingRenderer.hpp"
#include "render/ExoplanetConverter.hpp"
#include "data/ExoplanetData.hpp"
#include "ai/InferenceEngine.hpp"

namespace astrocore {
class DataVisualization;
}

struct GLFWwindow;

namespace astrocore {

struct PlanetParams;
class PresetManager;

enum class Theme { Dark, Light };

class UIManager {
public:
    UIManager();
    ~UIManager();

    void init(GLFWwindow* window);
    void beginFrame();
    void endFrame();

    void shutdown();

    // Render planet editor UI with tab-based layout
    // outPos/outSize are filled with the actual ImGui window rect this frame
    void render(PlanetParams& params,
                ImVec2* outPos  = nullptr,
                ImVec2* outSize = nullptr);

    // Legacy render method for compatibility
    void renderPlanetEditor(const std::string& bodyName, PlanetParams& params,
                            RingParams* ringParams = nullptr,
                            std::function<void()> onRingChanged = nullptr);

    // Called by Application to register the async planet-load trigger
    void setExoplanetCallback(std::function<void(const std::string&)> onLoad);

    // Called by Application on the main thread with load status updates
    void setExoplanetStatus(const std::string& status);

    // Set current exoplanet data for the DATA tab
    void setCurrentExoplanetData(const ExoplanetData& data);
    void clearCurrentExoplanetData();

    // Returns preset PlanetParams by index (0=Earth ... 7=Alien)
    static PlanetParams getPreset(int index);

    // True for exactly one call after the "Back to Galaxy" button is pressed
    bool wasBackPressed();

    // Sync the current theme (call after init)
    void applyThemeTo(Theme t);

    // Persistent top-right pill toggle - call every frame
    void renderThemeToggle();

    Theme getTheme() const { return m_theme; }

    // Render system presets panel
    bool renderSystemPresets(PresetManager& presetManager, int& selectedPresetIndex);

    // Render exoplanet search panel
    struct ExoplanetSearchResult {
        bool searchRequested = false;
        std::string searchQuery;
        bool viewRequested = false;
        int selectedIndex = -1;
        bool cachedPlanetSelected = false;
        std::string cachedPlanetName;
    };
    ExoplanetSearchResult renderExoplanetSearch(const std::vector<ExoplanetData>& results,
                                                 bool isSearching);

    // Refresh the list of cached planets
    void refreshCachedPlanets();

    // Render inference backend selector
    bool renderInferenceSettings(InferenceEngine* engine);

    // Render simulation controls (pause/play, time scale, focus lock)
    struct SimulationControlsResult {
        bool pauseToggled = false;
        bool timeScaleChanged = false;
        double newTimeScale = 1.0;
        bool clearTrails = false;
        bool lockToggled = false;
        bool unlockRequested = false;
    };
    SimulationControlsResult renderSimulationControls(bool isPaused, double timeScale,
                                                       bool isLocked, const std::string& lockedBodyName);

private:
    void setupStyle();

    bool  m_initialized  = false;
    int   m_presetIndex  = 0;
    int   m_systemPresetIndex = 0;
    Theme m_theme        = Theme::Dark;
    bool  m_backPressed  = false;
    float m_toggleAnimT  = 0.f;   // 0 = dark side, 1 = light side (animated)

    // Exoplanet search state
    char  m_searchBuf[256] = {};
    std::string m_exoStatus = "Select a planet from the Galaxy view.";
    std::function<void(const std::string&)> m_exoCallback;

    // Legacy exoplanet search state
    char m_exoSearchBuffer[256] = "";
    int m_exoSelectedIndex = -1;

    // Cached planets list
    std::vector<ExoplanetConverter::CachedPlanetInfo> m_cachedPlanets;
    int m_cachedSelectedIndex = -1;
    char m_cachedFilterBuffer[256] = "";

    // Current exoplanet data for DATA tab
    ExoplanetData m_currentExoData;
    bool m_hasExoData = false;

    // Data visualization for GRAPHS tab
    std::unique_ptr<DataVisualization> m_dataViz;
};

}  // namespace astrocore
