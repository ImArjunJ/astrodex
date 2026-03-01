#pragma once

#include <memory>
#include <cstdint>
#include <vector>

struct GLFWwindow;

namespace astrocore {

struct StarVertex;
class FreeFlyCamera;

class StarRenderer {
public:
    StarRenderer();
    ~StarRenderer();

    StarRenderer(const StarRenderer&) = delete;
    StarRenderer& operator=(const StarRenderer&) = delete;

    void init(int width, int height, GLFWwindow* window);
    void uploadStars(const std::vector<StarVertex>& stars);      // static upload (HYG fallback)
    void updateStars(const std::vector<StarVertex>& stars);      // dynamic per-frame update (octree)
    void resize(int width, int height);

    void beginFrame();
    void render(const FreeFlyCamera& camera, float time, float pointScale, float brightnessBoost, bool debugMode = false);
    void endFrame();

    // Vulkan accessors for ImGui integration (opaque void*)
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
