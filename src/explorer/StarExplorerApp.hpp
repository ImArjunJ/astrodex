#pragma once

#include "explorer/StarData.hpp"
#include <memory>
#include <vector>

namespace astrocore {

class Window;
class StarRenderer;
class FreeFlyCamera;
class ExplorerUI;
class StarData;
class StarOctree;

class StarExplorerApp {
public:
    StarExplorerApp();
    ~StarExplorerApp();

    void run();

private:
    void init();
    void update(float deltaTime);
    void render();
    void shutdown();

    std::unique_ptr<Window>        m_window;
    std::unique_ptr<StarRenderer>  m_renderer;
    std::unique_ptr<FreeFlyCamera> m_camera;
    std::unique_ptr<ExplorerUI>    m_ui;
    std::unique_ptr<StarData>      m_starData;
    std::unique_ptr<StarOctree>    m_octree;

    bool   m_running = true;
    bool   m_cursorCaptured = false;
    bool   m_useOctree = false;
    double m_lastFrameTime = 0.0;
    float  m_time = 0.0f;

    // Render settings
    float m_pointScale      = 3.0f;
    float m_brightnessBoost = 5.0f;
    float m_lodThreshold    = 0.005f;
    int   m_maxVisibleStars = 1'000'000;

    // Visible stars for octree mode (reused each frame to avoid reallocation)
    std::vector<StarVertex> m_visibleStars;
    int m_visibleCount = 0;
    int m_totalStarCount = 0;

    // Nearest star cache (updated every 0.5s)
    float    m_nearestTimer = 0.0f;
    StarInfo m_cachedNearest{};

    // Mouse state
    double m_lastMouseX = 0.0;
    double m_lastMouseY = 0.0;
    bool   m_firstMouse = true;
};

} // namespace astrocore
