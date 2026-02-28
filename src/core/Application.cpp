#include "core/Application.hpp"
#include "core/Logger.hpp"
#include "render/Renderer.hpp"
#include "render/Camera.hpp"
#include <imgui.h>

namespace astrocore {

Application::Application() {
    init();
}

Application::~Application() {
    shutdown();
}

void Application::init() {
    Logger::init();
    LOG_INFO("AstroSplat starting...");

    // Window
    WindowConfig config;
    config.title = "AstroSplat - Procedural Planet Generator";
    config.width = 1280;
    config.height = 720;
    config.vsync = true;
    m_window = std::make_unique<Window>(config);

    // Camera — target at planet position (0,0,-10), camera at (0,0,6) matching reference
    m_camera = std::make_unique<Camera>();
    m_camera->setTarget(glm::vec3(0.0f, 0.0f, -10.0f));
    m_camera->setPosition(glm::vec3(0.0f, 0.0f, 6.0f));

    m_window->setScrollCallback([this](double, double yoffset) {
        if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse) return;
        m_camera->zoom(static_cast<float>(-yoffset) * 0.3f);
    });

    m_window->setResizeCallback([this](int width, int height) {
        m_renderer->resize(width, height);
        m_camera->setAspectRatio(static_cast<float>(width) / static_cast<float>(height));
    });

    // Renderer
    m_renderer = std::make_unique<Renderer>();
    m_renderer->init(m_window->getWidth(), m_window->getHeight());

    // UI
    m_ui = std::make_unique<UIManager>();
    m_ui->init(m_window->getHandle());

    m_lastFrameTime = m_window->getTime();
    LOG_INFO("Ready");
}

void Application::run() {
    while (!m_window->shouldClose() && m_running) {
        double currentTime = m_window->getTime();
        float deltaTime = static_cast<float>(currentTime - m_lastFrameTime);
        m_lastFrameTime = currentTime;

        m_window->pollEvents();
        update(deltaTime);
        render();
        m_window->swapBuffers();
    }
}

void Application::update(float deltaTime) {
    // Mouse drag for camera orbit — skip when ImGui wants the mouse
    static bool dragging = false;
    static double lastX = 0, lastY = 0;

    if (ImGui::GetIO().WantCaptureMouse) {
        dragging = false;
    } else if (m_window->isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
        double x, y;
        m_window->getCursorPos(x, y);

        if (!dragging) {
            dragging = true;
            lastX = x;
            lastY = y;
        } else {
            float dx = static_cast<float>(x - lastX);
            float dy = static_cast<float>(y - lastY);
            m_camera->rotate(dx * 0.005f, dy * 0.005f);
            lastX = x;
            lastY = y;
        }
    } else {
        dragging = false;
    }

    m_camera->update(deltaTime);
}

void Application::render() {
    m_renderer->beginFrame();
    m_renderer->render(*m_camera);
    m_renderer->endFrame();

    m_ui->beginFrame();
    m_ui->render(m_renderer->params());
    m_ui->endFrame();
}

void Application::shutdown() {
    m_ui.reset();
    m_renderer.reset();
    m_camera.reset();
    m_window.reset();
}

}  // namespace astrocore
