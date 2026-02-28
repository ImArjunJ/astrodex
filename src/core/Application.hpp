#pragma once

#include "core/Window.hpp"
#include "render/IRenderer.hpp"
#include "ui/UIManager.hpp"
#include "data/NasaApiClient.hpp"
#include "ai/InferenceEngine.hpp"
#include "render/ExoplanetMapper.hpp"
#include <memory>
#include <future>
#include <string>
#include <optional>

namespace astrocore {

class Camera;

class Application {
public:
    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void run();

private:
    void init();
    void update(float deltaTime);
    void render();
    void shutdown();

    // Async planet load: returns (newParams, statusMessage)
    void loadPlanet(const std::string& name);

    std::unique_ptr<Window>          m_window;
    std::unique_ptr<IRenderer>       m_renderer;
    std::unique_ptr<Camera>          m_camera;
    std::unique_ptr<UIManager>       m_ui;

    // ML pipeline
    std::unique_ptr<NasaApiClient>   m_nasa;
    std::unique_ptr<InferenceEngine> m_inference;

    using LoadResult = std::pair<std::optional<PlanetParams>, std::string>;
    std::future<LoadResult> m_planetFuture;
    bool m_planetLoading = false;

    // Background validation for known solar-system planets
    std::future<std::string> m_validationFuture;
    bool m_validationRunning = false;
    std::string m_currentStatus;  // tracks displayed status for later appending

    bool   m_running       = true;
    double m_lastFrameTime = 0.0;
};

}  // namespace astrocore
