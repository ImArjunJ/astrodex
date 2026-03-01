#pragma once

#include <imgui.h>
#include <glm/glm.hpp>

struct GLFWwindow;

namespace astrocore {

class FreeFlyCamera;
struct StarInfo;

class ExplorerUI {
public:
    ExplorerUI();
    ~ExplorerUI();

    void init(GLFWwindow* window);
    void beginFrame();
    void endFrame();
    void shutdown();

    // Returns true if reset button was pressed
    bool render(const FreeFlyCamera& camera, const StarInfo& nearest,
                float& pointScale, float& brightnessBoost, float& speed,
                int totalStars, bool useLOD, int visibleStars,
                float& shellMultiplier, bool& showMarkers, bool& debugRender);

private:
    void setupStyle();
    bool m_initialized = false;
};

} // namespace astrocore
