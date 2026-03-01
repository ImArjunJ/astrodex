#pragma once

#include "core/Window.hpp"
#include "render/IRenderer.hpp"
#include "ui/UIManager.hpp"
#include "data/NasaApiClient.hpp"
#include "data/CacheManager.hpp"
#include "data/ExoplanetData.hpp"
#include "ai/InferenceEngine.hpp"
#include "render/ExoplanetMapper.hpp"
#include <memory>
#include <future>
#include <string>
#include <optional>
#include <atomic>
#include <set>

namespace astrocore {

class Camera;

// Pipeline stage for thread-safe status communication
enum class PipelineStage : int {
    Idle = 0,
    QueryingNasa,
    RunningAI,
    MappingParams,
    Done,
    Failed
};

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
    void update(float deltaTime);
    void render();
    void shutdown();

    // Async planet load: returns (newParams, statusMessage, exoData)
    void loadPlanet(const std::string& name);

    // Build the known planet name list from SolarSystemDatabase + cache
    void buildPlanetNameList();

    std::unique_ptr<Window>          m_window;
    std::unique_ptr<IRenderer>       m_renderer;
    std::unique_ptr<Camera>          m_camera;
    std::unique_ptr<UIManager>       m_ui;

    // ML pipeline
    std::unique_ptr<NasaApiClient>   m_nasa;
    std::unique_ptr<InferenceEngine> m_inference;
    std::unique_ptr<CacheManager>    m_cacheManager;

    using LoadResult = std::tuple<std::optional<PlanetParams>, std::string, std::optional<ExoplanetData>>;
    std::future<LoadResult> m_planetFuture;
    bool m_planetLoading = false;

    // Pipeline stage (thread-safe communication from async lambda to main thread)
    std::atomic<int> m_pipelineStage{0};

    // Stored exoplanet data after successful load (for info panel in Plan 02)
    std::optional<ExoplanetData> m_loadedExoData;

    // Known planet names for autocomplete (grows as user searches)
    std::set<std::string> m_knownNames;

    // Background validation for known solar-system planets
    std::future<std::string> m_validationFuture;
    bool m_validationRunning = false;
    std::string m_currentStatus;  // tracks displayed status for later appending

    bool   m_running       = true;
    double m_lastFrameTime = 0.0;
};

}  // namespace astrocore
