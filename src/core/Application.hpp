#pragma once

#include "core/Window.hpp"
#include "render/IRenderer.hpp"
#include "ui/UIManager.hpp"
#include "ui/GalaxyView.hpp"
#include "data/NasaApiClient.hpp"
#include "ai/InferenceEngine.hpp"
#include "render/ExoplanetMapper.hpp"
#include <memory>
#include <future>
#include <string>
#include <optional>

namespace astrocore {

class Camera;

enum class AppScreen { Galaxy, PlanetDetail };

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
    void renderGalaxy(float dt);   // update + render combined (ImGui input needs NewFrame first)
    void update(float deltaTime);
    void render(float dt);
    void shutdown();

    // Async planet load: returns (newParams, statusMessage)
    void loadPlanet(const std::string& name);

    std::unique_ptr<Window>          m_window;
    std::unique_ptr<IRenderer>       m_renderer;
    std::unique_ptr<Camera>          m_camera;
    std::unique_ptr<UIManager>       m_ui;
    std::unique_ptr<GalaxyView>      m_galaxy;

    // ML pipeline
    std::unique_ptr<NasaApiClient>   m_nasa;
    std::unique_ptr<InferenceEngine> m_inference;

    using LoadResult = std::pair<std::optional<PlanetParams>, std::string>;
    std::future<LoadResult> m_planetFuture;
    bool m_planetLoading = false;

    // Background validation for known solar-system planets
    std::future<std::string> m_validationFuture;
    bool m_validationRunning = false;
    std::string m_currentStatus;

    AppScreen m_screen        = AppScreen::Galaxy;
    bool      m_running       = true;
    double    m_lastFrameTime = 0.0;
    float     m_galaxyFadeTimer = 0.f;   // drives the black→transparent galaxy fade-in

    // Galaxy → PlanetDetail border overlay transition
    float m_borderFadeTimer    = -1.f;  // >= 0 while border particles are visible
    bool  m_borderReleased     = false;
    float m_planetDetailFadeIn = 1.f;   // 0→1 fade-in for planet detail UI

    // Saved planet params so we can hide the planet during galaxy view
    PlanetParams m_savedParams{};
};

}  // namespace astrocore
