#include "explorer/StarExplorerApp.hpp"
#include "explorer/StarRenderer.hpp"
#include "explorer/FreeFlyCamera.hpp"
#include "explorer/ExplorerUI.hpp"
#include "explorer/StarData.hpp"
#include "explorer/StarLOD.hpp"
#include "core/Window.hpp"
#include "core/Logger.hpp"

#include <GLFW/glfw3.h>

namespace astrocore {

// Default shell distances per LOD level (pc)
// LOD3 = farthest (always rendered), LOD0 = nearest
static constexpr float BASE_SHELL_DISTANCES[NUM_LOD_LEVELS] = {
    100.0f,    // LOD0: full detail within 100pc
    1000.0f,   // LOD1: 10% detail within 1000pc
    5000.0f,   // LOD2: 2% detail within 5000pc
    100000.0f, // LOD3: 0.5% everywhere (background sky)
};

StarExplorerApp::StarExplorerApp() {
    init();
}

StarExplorerApp::~StarExplorerApp() {
    shutdown();
}

void StarExplorerApp::init() {
    Logger::init();
    LOG_INFO("Star Explorer starting...");

    WindowConfig cfg;
    cfg.title  = "Star Explorer - 3D Star Navigation";
    cfg.width  = 1280;
    cfg.height = 720;
    m_window = std::make_unique<Window>(cfg);

    m_camera = std::make_unique<FreeFlyCamera>();
    m_camera->setAspectRatio(m_window->getAspectRatio());

    m_renderer = std::make_unique<StarRenderer>();
    m_renderer->init(m_window->getWidth(), m_window->getHeight(), m_window->getHandle());

    // Always load HYG as base layer (full-sky coverage, fills Gaia gaps)
    m_starData = std::make_unique<StarData>();
    m_starData->load("assets/starmap/.hyg_cache.csv");

    // Try loading multi-LOD Gaia on top
    m_starLOD = std::make_unique<StarLOD>();
    if (m_starLOD->load("assets/starmap/gaia_multilod.bin")) {
        m_useLOD = true;
        m_totalStarCount = static_cast<int>(m_starLOD->totalStars());
        LOG_INFO("Using multi-LOD ({} stars) + HYG base ({} stars)",
                 m_totalStarCount, m_starData->count());
    } else {
        LOG_INFO("No multi-LOD found, using HYG catalog only");
        m_starLOD.reset();
        if (m_starData && m_starData->count() > 0) {
            m_renderer->uploadStars(m_starData->vertices());
            m_totalStarCount = static_cast<int>(m_starData->count());
        }
    }

    m_ui = std::make_unique<ExplorerUI>();
    m_ui->init(m_window->getHandle());

    m_window->setResizeCallback([this](int w, int h) {
        m_renderer->resize(w, h);
        m_camera->setAspectRatio(static_cast<float>(w) / static_cast<float>(std::max(h, 1)));
    });

    m_window->setScrollCallback([this](double, double yOffset) {
        m_camera->processScroll(static_cast<float>(yOffset));
    });

    m_window->setKeyCallback([this](int key, int, int action, int) {
        if (key == GLFW_KEY_TAB && action == GLFW_PRESS) {
            m_cursorCaptured = !m_cursorCaptured;
            glfwSetInputMode(m_window->getHandle(), GLFW_CURSOR,
                             m_cursorCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
            m_firstMouse = true;
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

    bool forward = m_window->isKeyPressed(GLFW_KEY_W);
    bool back    = m_window->isKeyPressed(GLFW_KEY_S);
    bool left    = m_window->isKeyPressed(GLFW_KEY_A);
    bool right   = m_window->isKeyPressed(GLFW_KEY_D);
    bool up      = m_window->isKeyPressed(GLFW_KEY_SPACE);
    bool down    = m_window->isKeyPressed(GLFW_KEY_LEFT_SHIFT) ||
                   m_window->isKeyPressed(GLFW_KEY_RIGHT_SHIFT);
    m_camera->processKeyboard(forward, back, left, right, up, down, deltaTime);

    bool lookLeft  = m_window->isKeyPressed(GLFW_KEY_LEFT);
    bool lookRight = m_window->isKeyPressed(GLFW_KEY_RIGHT);
    bool lookUp    = m_window->isKeyPressed(GLFW_KEY_UP);
    bool lookDown  = m_window->isKeyPressed(GLFW_KEY_DOWN);
    m_camera->processArrowKeys(lookLeft, lookRight, lookUp, lookDown, deltaTime);

    // Multi-LOD collection
    if (m_useLOD && m_starLOD) {
        // Scale shell distances by multiplier
        float shells[NUM_LOD_LEVELS];
        for (int i = 0; i < NUM_LOD_LEVELS; i++) {
            shells[i] = BASE_SHELL_DISTANCES[i] * m_shellMultiplier;
        }

        // Start with HYG as base layer (119k stars, full-sky, fills Gaia gaps)
        if (m_starData && m_starData->count() > 0) {
            m_visibleStars = m_starData->vertices();
        } else {
            m_visibleStars.clear();
        }

        // Add Gaia LOD stars on top (budget minus HYG count)
        int gaiabudget = 2'000'000 - static_cast<int>(m_visibleStars.size());
        if (gaiabudget > 0) {
            std::vector<StarVertex> gaiaStars;
            m_starLOD->collectVisible(m_camera->getPosition(), shells,
                                       gaiaStars, gaiabudget);
            m_visibleStars.insert(m_visibleStars.end(), gaiaStars.begin(), gaiaStars.end());
        }

        // Inject debug markers at front
        if (m_showMarkers) {
            StarVertex sol;
            sol.position  = glm::vec3(0.0f);
            sol.magnitude = -10.0f;
            sol.color     = glm::vec3(1.0f, 1.0f, 0.0f);
            m_visibleStars.insert(m_visibleStars.begin(), sol);
        }

        m_visibleCount = static_cast<int>(m_visibleStars.size());
        m_renderer->updateStars(m_visibleStars);
    }

    // Nearest star lookup
    m_nearestTimer += deltaTime;
    if (m_nearestTimer >= 0.5f) {
        m_nearestTimer = 0.0f;
        if (m_useLOD && m_starLOD) {
            m_cachedNearest = m_starLOD->findNearest(m_camera->getPosition());
        } else if (m_starData) {
            m_cachedNearest = m_starData->findNearest(m_camera->getPosition());
        }
    }
}

void StarExplorerApp::render() {
    m_renderer->beginFrame();
    m_renderer->render(*m_camera, m_time, m_pointScale, m_brightnessBoost, m_debugRender);

    m_ui->beginFrame();
    float speed = m_camera->getSpeed();
    bool reset = m_ui->render(*m_camera, m_cachedNearest, m_pointScale, m_brightnessBoost, speed,
                              m_totalStarCount, m_useLOD, m_visibleCount,
                              m_shellMultiplier, m_showMarkers, m_debugRender);
    m_camera->setSpeed(speed);
    if (reset) m_camera->reset();
    m_ui->endFrame();

    m_renderer->endFrame();
}

void StarExplorerApp::shutdown() {
    LOG_INFO("Star Explorer shutting down");
}

} // namespace astrocore
