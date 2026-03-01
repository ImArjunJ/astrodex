#pragma once

#include "render/IRenderer.hpp"
#include "render/VulkanTypes.hpp"

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace astrocore {

class Camera;

class RendererBase : public IRenderer {
public:
    virtual ~RendererBase() = default;

    PlanetParams& params() override { return params_; }

    void setPlanetPosition(const glm::vec3& pos) { planetPosition_ = pos; }
    const glm::vec3& planetPosition() const { return planetPosition_; }

    void setPaused(bool paused) { paused_ = paused; }
    bool isPaused() const { return paused_; }

    void setTimeScale(float scale) { timeScale_ = scale; }
    float timeScale() const { return timeScale_; }

    void setEmissive(bool emissive) { emissive_ = emissive; }
    bool isEmissive() const { return emissive_; }

    PlanetUniforms fillUniforms(const Camera& camera);
    void advanceTime(float dt);
    float currentTime() const { return time_; }

    static std::vector<uint8_t> generateNoiseData();
    static std::vector<float> getQuadVertices();
    static std::string resolveAssetPath(const std::string& relative);

protected:
    PlanetParams params_;
    glm::vec3 planetPosition_{0.0f, 0.0f, -10.0f};
    float time_ = 0.0f;
    float timeScale_ = 1.0f;
    bool paused_ = false;
    bool emissive_ = false;
    int width_ = 0;
    int height_ = 0;
};

} // namespace astrocore
