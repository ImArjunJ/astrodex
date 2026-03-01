#pragma once

#include <imgui.h>
#include <string>
#include <vector>
#include <functional>
#include "render/RingRenderer.hpp"
#include "data/ExoplanetData.hpp"
#include "ai/InferenceEngine.hpp"

struct GLFWwindow;

namespace astrocore {

struct PlanetParams;
class PresetManager;

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

    // Render system presets panel
    // Returns true if a preset was selected (index in selectedPresetIndex)
    bool renderSystemPresets(PresetManager& presetManager, int& selectedPresetIndex);

    // Render exoplanet search panel
    // Returns selected exoplanet data if user clicks "View", otherwise nullopt
    struct ExoplanetSearchResult {
        bool searchRequested = false;
        std::string searchQuery;
        bool viewRequested = false;
        int selectedIndex = -1;
    };
    ExoplanetSearchResult renderExoplanetSearch(const std::vector<ExoplanetData>& results,
                                                 bool isSearching);

    // Render inference backend selector
    // Returns true if backend was changed
    bool renderInferenceSettings(InferenceEngine* engine);

private:
    void setupStyle();

    bool m_initialized = false;
    int m_presetIndex = 0;
    int m_systemPresetIndex = 0;

    // Exoplanet search state
    char m_exoSearchBuffer[256] = "";
    int m_exoSelectedIndex = -1;
};

}  // namespace astrocore
