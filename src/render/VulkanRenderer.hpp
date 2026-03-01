#pragma once

#include "render/IRenderer.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <cstdint>

namespace astrocore {

class Camera;

class VulkanRenderer : public IRenderer {
public:
    VulkanRenderer();
    ~VulkanRenderer();

    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;

    void init(int width, int height, void* glfwWindow = nullptr) override;
    void resize(int width, int height) override;

    void beginFrame() override;
    void render(const Camera& camera) override;
    void endFrame() override;

    PlanetParams& params() override;

    // Planet position for multi-body rendering
    void setPlanetPosition(const glm::vec3& pos);

    // Time control for rotation animation
    void setPaused(bool paused);
    bool isPaused() const;
    void setTimeScale(float scale);
    float timeScale() const;

    // Emissive flag for star rendering
    void setEmissive(bool emissive);

    // Vulkan accessors for ImGui integration (opaque void* to avoid vulkan.h in header)
    void* getInstance();
    void* getPhysicalDevice();
    void* getDevice();
    uint32_t getGraphicsQueueFamily();
    void* getGraphicsQueue();
    void* getRenderPass();
    void* getDescriptorPool();
    void* getCurrentCommandBuffer();
    uint32_t getSwapchainImageCount();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace astrocore
