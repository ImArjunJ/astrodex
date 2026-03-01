#pragma once

#include "render/RendererBase.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <cstdint>

namespace astrocore {

class Camera;

class VulkanRenderer : public RendererBase {
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
