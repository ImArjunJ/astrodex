#pragma once

#include <glm/glm.hpp>
#include <vector>

namespace astrocore {

// A single ring band
struct RingBand {
    float innerRadius = 1.5f;
    float outerRadius = 2.0f;
    glm::vec3 innerColor{0.8f, 0.75f, 0.65f};
    glm::vec3 outerColor{0.6f, 0.55f, 0.5f};
    float opacity = 0.8f;
    float density = 1.0f;
};

struct RingParams {
    bool enabled = false;
    int totalParticles = 30000;
    float thickness = 0.02f;

    std::vector<RingBand> bands;

    void addBand(float inner, float outer, glm::vec3 color1, glm::vec3 color2, float opacity = 0.8f) {
        bands.push_back({inner, outer, color1, color2, opacity, 1.0f});
    }

    void clearBands() { bands.clear(); }
};

}  // namespace astrocore
