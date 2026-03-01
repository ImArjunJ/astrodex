#pragma once

#include <imgui.h>
#include <glm/glm.hpp>

struct GLFWwindow;

namespace astrocore {

class StarRenderer;
class FreeFlyCamera;
struct StarInfo;

class ExplorerUI {
public:
    ExplorerUI();
    ~ExplorerUI();

    void init(GLFWwindow* window, StarRenderer* renderer);
    void beginFrame();
    void endFrame(StarRenderer* renderer);
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
