#pragma once

#include "core/Window.hpp"
#include "ui/UIManager.hpp"
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

    std::unique_ptr<Window> m_window;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<Camera> m_camera;
    std::unique_ptr<UIManager> m_ui;

    bool m_running = true;
    double m_lastFrameTime = 0.0;
};

}  // namespace astrocore
