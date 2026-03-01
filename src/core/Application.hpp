#pragma once

#include "core/Window.hpp"
#include "ui/UIManager.hpp"
#include "simulation/Simulation.hpp"
#include "config/PresetManager.hpp"
#include "data/ExoplanetDataAggregator.hpp"
#include "data/ExoplanetData.hpp"
#include "ai/InferenceEngine.hpp"
#include <GLFW/glfw3.h>
#include <memory>
#include <future>

namespace astrocore {

class Renderer;
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
    void handleInput();
    void loadExoplanetIntoSimulation(const ExoplanetData& exo);

    std::unique_ptr<Window> m_window;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<Camera> m_camera;
    std::unique_ptr<UIManager> m_ui;
    Simulation m_simulation;
    PresetManager m_presetManager;

    // Exoplanet data aggregator (NASA + ExoMAST + more)
    std::unique_ptr<ExoplanetDataAggregator> m_dataAggregator;
    std::vector<ExoplanetData> m_exoSearchResults;
    std::future<std::vector<ExoplanetData>> m_exoSearchFuture;
    bool m_exoSearching = false;
    // Track atmospheric detections from ExoMAST
    std::vector<AtmosphericDetection> m_currentAtmosphericDetections;

    // AI inference
    std::unique_ptr<InferenceEngine> m_inferenceEngine;
    std::future<ExoplanetData> m_inferenceFuture;
    bool m_inferring = false;
    ExoplanetData m_pendingExoplanet;  // Exoplanet being inferred

    bool m_running = true;
    double m_lastFrameTime = 0.0;
    bool m_mouseLocked = false;  // FPS-style mouse look
    double m_lastMouseX = 0.0;
    double m_lastMouseY = 0.0;
    GLFWcursor* m_blankCursor = nullptr;  // Transparent cursor for Wayland
    float m_cameraSpeed = 100.0f;  // Base camera movement speed (adjustable with +/-)
};

}  // namespace astrocore
