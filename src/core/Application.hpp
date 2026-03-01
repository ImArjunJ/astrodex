#pragma once

#include "core/Window.hpp"
#include "ui/UIManager.hpp"
#include "ui/GalaxyView.hpp"
#include "simulation/Simulation.hpp"
#include "config/PresetManager.hpp"
#include "data/ExoplanetData.hpp"
#ifndef __EMSCRIPTEN__
#include "data/ExoplanetDataAggregator.hpp"
#include "ai/InferenceEngine.hpp"
#endif
#include <GLFW/glfw3.h>
#include <memory>
#include <future>
#include <optional>

namespace astrocore {

class RendererBase;
class Camera;

enum class AppScreen { Intro, Galaxy, PlanetDetail, SolarSystem };

class Application {
public:
    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void run();

private:
    void init();
    void runIntro();
    void renderGalaxy(float dt);
    void renderSolarSystem(float dt);
    void handleSolarSystemInput();
    void update(float deltaTime);
    void render(float dt);
    void shutdown();
    void handleInput();
    void loadExoplanetIntoSimulation(const ExoplanetData& exo);
    void loadPlanet(const std::string& name);

#ifdef __EMSCRIPTEN__
    void tick();  // Single-frame callback for emscripten_set_main_loop
#endif

    std::unique_ptr<Window> m_window;
    std::unique_ptr<RendererBase> m_renderer;
    std::unique_ptr<Camera> m_camera;
    std::unique_ptr<UIManager> m_ui;
    std::unique_ptr<GalaxyView> m_galaxy;
    Simulation m_simulation;
    PresetManager m_presetManager;

#ifndef __EMSCRIPTEN__
    // Exoplanet data aggregator (NASA + ExoMAST + more, requires libcurl)
    std::unique_ptr<ExoplanetDataAggregator> m_dataAggregator;
    std::vector<ExoplanetData> m_exoSearchResults;
    std::future<std::vector<ExoplanetData>> m_exoSearchFuture;
    bool m_exoSearching = false;
    std::vector<AtmosphericDetection> m_currentAtmosphericDetections;

    // AI inference (requires libcurl)
    std::unique_ptr<InferenceEngine> m_inferenceEngine;
    std::future<ExoplanetData> m_inferenceFuture;
    bool m_inferring = false;
    ExoplanetData m_pendingExoplanet;
#endif

    // Async planet load
    struct LoadResult {
        std::optional<PlanetParams> params;
        std::string status;
        ExoplanetData exoData;
        bool hasExoData = false;
    };
    std::future<LoadResult> m_planetFuture;
    bool m_planetLoading = false;
    std::string m_currentStatus;

    // Screen state
    AppScreen m_screen = AppScreen::Intro;
    bool m_running = true;
    double m_lastFrameTime = 0.0;
    float m_galaxyFadeTimer = 0.f;

    // Galaxy -> PlanetDetail transition
    float m_borderFadeTimer = -1.f;
    bool m_borderReleased = false;
    float m_planetDetailFadeIn = 1.f;
    PlanetParams m_savedParams{};

    // Input state
    bool m_mouseLocked = false;
    double m_lastMouseX = 0.0;
    double m_lastMouseY = 0.0;
    GLFWcursor* m_blankCursor = nullptr;
    float m_cameraSpeed = 100.0f;
};

}  // namespace astrocore
