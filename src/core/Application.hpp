#pragma once

#include "core/Window.hpp"
#include "ui/UIManager.hpp"
#include "simulation/Simulation.hpp"
#include <GLFW/glfw3.h>
#include <memory>

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

    std::unique_ptr<Window> m_window;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<Camera> m_camera;
    std::unique_ptr<UIManager> m_ui;
    Simulation m_simulation;

    bool m_running = true;
    double m_lastFrameTime = 0.0;
    bool m_mouseLocked = false;  // FPS-style mouse look
    double m_lastMouseX = 0.0;
    double m_lastMouseY = 0.0;
    GLFWcursor* m_blankCursor = nullptr;  // Transparent cursor for Wayland
    float m_cameraSpeed = 100.0f;  // Base camera movement speed (adjustable with +/-)
};

}  // namespace astrocore
