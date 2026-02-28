#pragma once

#include <glm/glm.hpp>

namespace astrocore {

class Camera;

// All the knobs for procedural planet generation.
// Shared between OpenGL Renderer and MetalRenderer.
struct PlanetParams {
    float radius = 2.0f;
    float rotationSpeed = 0.1f;

    float noiseStrength = 0.2f;
    float terrainScale = 0.8f;

    int   fbmOctaves = 6;
    float fbmPersistence = 0.5f;
    float fbmLacunarity = 2.0f;
    float fbmExponentiation = 5.0f;
    float domainWarpStrength = 0.0f;

    float ridgedStrength = 0.0f;
    float craterStrength = 0.0f;
    float continentScale = 0.0f;

    float waterLevel = 0.0f;

    float polarCapSize = 0.0f;
    float bandingStrength = 0.0f;
    float bandingFrequency = 20.0f;

    glm::vec3 waterColorDeep    = {0.01f, 0.05f, 0.15f};
    glm::vec3 waterColorSurface = {0.02f, 0.12f, 0.27f};
    glm::vec3 sandColor         = {1.0f,  1.0f,  0.85f};
    glm::vec3 treeColor         = {0.02f, 0.1f,  0.06f};
    glm::vec3 rockColor         = {0.15f, 0.12f, 0.12f};
    glm::vec3 iceColor          = {0.8f,  0.9f,  0.9f};

    float sandLevel  = 0.028f;
    float treeLevel  = 0.03f;
    float rockLevel  = 0.1f;
    float iceLevel   = 0.15f;
    float transition = 0.02f;

    float cloudsDensity  = 0.5f;
    float cloudsScale    = 1.0f;
    float cloudsSpeed    = 1.5f;
    float cloudAltitude  = 0.15f;
    float cloudThickness = 0.1f;

    glm::vec3 atmosphereColor   = {0.05f, 0.3f, 0.9f};
    float     atmosphereDensity = 0.3f;

    glm::vec3 cloudColor = {1.0f, 1.0f, 1.0f};

    glm::vec3 sunDirection = {1.0f, 1.0f, 0.5f};
    float     sunIntensity = 3.0f;
    float     ambientLight = 0.01f;
    glm::vec3 sunColor     = {1.0f, 1.0f, 0.9f};
    glm::vec3 deepSpaceColor = {0.0f, 0.0f, 0.001f};

    float rotationOffset = 0.6f;
    float quality = 1.0f;
};

// Optional Metal frame context — populated by MetalRenderer each frame.
// OpenGL renderer leaves this zeroed.
struct MetalFrameContext {
    void* commandBuffer      = nullptr;
    void* commandEncoder     = nullptr;
    void* renderPassDescriptor = nullptr;
};

class IRenderer {
public:
    virtual ~IRenderer() = default;

    // nativeWindow: GLFWwindow* — used by MetalRenderer to attach CAMetalLayer.
    // Ignored by the OpenGL renderer.
    virtual void init(int width, int height, void* nativeWindow = nullptr) = 0;
    virtual void resize(int width, int height) = 0;

    virtual void beginFrame() = 0;
    virtual void render(const Camera& camera) = 0;
    virtual void endFrame() = 0;

    virtual PlanetParams& params() = 0;

    // Metal-only: returns current frame's command buffer + encoder.
    virtual MetalFrameContext getMetalContext() { return {}; }
    virtual void* getMetalDevice() { return nullptr; }
};

}  // namespace astrocore
