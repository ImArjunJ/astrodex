#include "explorer/StarExplorerApp.hpp"
#include "explorer/StarRenderer.hpp"
#include "explorer/FreeFlyCamera.hpp"
#include "explorer/ExplorerUI.hpp"
#include "explorer/StarData.hpp"
#include "explorer/StarOctree.hpp"
#include "core/Window.hpp"
#include "core/Logger.hpp"

#include <GLFW/glfw3.h>

namespace astrocore {

StarExplorerApp::StarExplorerApp() {
    init();
}

StarExplorerApp::~StarExplorerApp() {
    shutdown();
}

void StarExplorerApp::init() {
    Logger::init();
    LOG_INFO("Star Explorer starting...");

    // Window
    WindowConfig cfg;
    cfg.title  = "Star Explorer - 3D Star Navigation";
    cfg.width  = 1280;
    cfg.height = 720;
    m_window = std::make_unique<Window>(cfg);

    // Camera at origin (Sol)
    m_camera = std::make_unique<FreeFlyCamera>();
    m_camera->setAspectRatio(m_window->getAspectRatio());

    // Renderer
    m_renderer = std::make_unique<StarRenderer>();
    m_renderer->init(m_window->getWidth(), m_window->getHeight(), m_window->getHandle());

    // Try loading Gaia octree first, fall back to HYG CSV
    m_octree = std::make_unique<StarOctree>();
    if (m_octree->load("assets/starmap/gaia_octree.bin")) {
        m_useOctree = true;
        m_totalStarCount = static_cast<int>(m_octree->totalStars());
        LOG_INFO("Using Gaia octree ({} stars)", m_totalStarCount);
    } else {
        LOG_INFO("No octree found, falling back to HYG catalog");
        m_octree.reset();
        m_starData = std::make_unique<StarData>();
        if (m_starData->load("assets/starmap/.hyg_cache.csv")) {
            m_renderer->uploadStars(m_starData->vertices());
            m_totalStarCount = static_cast<int>(m_starData->count());
        }
    }

    // UI
    m_ui = std::make_unique<ExplorerUI>();
    m_ui->init(m_window->getHandle(), m_renderer.get());

    // Callbacks
    m_window->setResizeCallback([this](int w, int h) {
        m_renderer->resize(w, h);
        m_camera->setAspectRatio(static_cast<float>(w) / static_cast<float>(std::max(h, 1)));
    });

    m_window->setScrollCallback([this](double /*xOffset*/, double yOffset) {
        m_camera->processScroll(static_cast<float>(yOffset));
    });

    m_window->setKeyCallback([this](int key, int /*scancode*/, int action, int /*mods*/) {
        if (key == GLFW_KEY_TAB && action == GLFW_PRESS) {
            m_cursorCaptured = !m_cursorCaptured;
            glfwSetInputMode(m_window->getHandle(), GLFW_CURSOR,
                             m_cursorCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
            m_firstMouse = true;
            LOG_INFO("Cursor {}", m_cursorCaptured ? "captured" : "released");
        }
    });

    m_window->setCursorPosCallback([this](double xpos, double ypos) {
        if (!m_cursorCaptured) return;

        if (m_firstMouse) {
            m_lastMouseX = xpos;
            m_lastMouseY = ypos;
            m_firstMouse = false;
            return;
        }

        float xOffset = static_cast<float>(xpos - m_lastMouseX);
        float yOffset = static_cast<float>(m_lastMouseY - ypos);
        m_lastMouseX = xpos;
        m_lastMouseY = ypos;

        m_camera->processMouseMovement(xOffset, yOffset);
    });

    m_lastFrameTime = m_window->getTime();
    LOG_INFO("Star Explorer initialized");
}

void StarExplorerApp::run() {
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

void StarExplorerApp::update(float deltaTime) {
    m_time += deltaTime;

    // WASD + Space/Shift movement
    bool forward = m_window->isKeyPressed(GLFW_KEY_W);
    bool back    = m_window->isKeyPressed(GLFW_KEY_S);
    bool left    = m_window->isKeyPressed(GLFW_KEY_A);
    bool right   = m_window->isKeyPressed(GLFW_KEY_D);
    bool up      = m_window->isKeyPressed(GLFW_KEY_SPACE);
    bool down    = m_window->isKeyPressed(GLFW_KEY_LEFT_SHIFT) ||
                   m_window->isKeyPressed(GLFW_KEY_RIGHT_SHIFT);

    m_camera->processKeyboard(forward, back, left, right, up, down, deltaTime);

    // Arrow keys for rotation
    bool lookLeft  = m_window->isKeyPressed(GLFW_KEY_LEFT);
    bool lookRight = m_window->isKeyPressed(GLFW_KEY_RIGHT);
    bool lookUp    = m_window->isKeyPressed(GLFW_KEY_UP);
    bool lookDown  = m_window->isKeyPressed(GLFW_KEY_DOWN);
    m_camera->processArrowKeys(lookLeft, lookRight, lookUp, lookDown, deltaTime);

    // Octree LOD traversal — collect visible stars each frame
    if (m_useOctree && m_octree) {
        m_octree->collectVisible(m_camera->getPosition(), m_lodThreshold,
                                  m_visibleStars, m_maxVisibleStars);
        m_visibleCount = static_cast<int>(m_visibleStars.size());
        m_renderer->updateStars(m_visibleStars);
    }

    // Update nearest star lookup every 0.5s
    m_nearestTimer += deltaTime;
    if (m_nearestTimer >= 0.5f) {
        m_nearestTimer = 0.0f;
        if (m_useOctree && m_octree) {
            m_cachedNearest = m_octree->findNearest(m_camera->getPosition());
        } else if (m_starData) {
            m_cachedNearest = m_starData->findNearest(m_camera->getPosition());
        }
    }
}

void StarExplorerApp::render() {
    m_renderer->beginFrame();
    m_renderer->render(*m_camera, m_time, m_pointScale, m_brightnessBoost);

    m_ui->beginFrame();
    float speed = m_camera->getSpeed();
    bool reset = m_ui->render(*m_camera, m_cachedNearest, m_pointScale, m_brightnessBoost, speed,
                              m_totalStarCount, m_useOctree, m_visibleCount,
                              m_lodThreshold, m_maxVisibleStars);
    m_camera->setSpeed(speed);
    if (reset) m_camera->reset();
    m_ui->endFrame(m_renderer.get());

    m_renderer->endFrame();
}

void StarExplorerApp::shutdown() {
    LOG_INFO("Star Explorer shutting down");
}

} // namespace astrocore
