#pragma once

#include <glm/glm.hpp>

namespace astrocore {

class Camera;

// Noise types for terrain generation
enum class NoiseType : int {
    Standard = 0,   // Classic FBM
    Ridged = 1,     // Sharp ridged mountains
    Billowy = 2,    // Soft, rounded hills
    Warped = 3,     // Domain-warped organic shapes
    Voronoi = 4,    // Cell-based terrain
    Swiss = 5,      // Eroded Swiss cheese look
    Hybrid = 6      // Mix of multiple types
};

// All the knobs for procedural planet generation.
struct PlanetParams {
    // Planet geometry
    float radius = 2.0f;
    float rotationSpeed = 0.1f;

    // Noise type selection
    NoiseType noiseType = NoiseType::Standard;

    // Terrain
    float noiseStrength = 0.02f;
    float terrainScale = 0.8f;

    // FBM shape
    int   fbmOctaves = 6;
    float fbmPersistence = 0.5f;
    float fbmLacunarity = 2.0f;
    float fbmExponentiation = 5.0f;
    float domainWarpStrength = 0.0f;

    // Terrain features
    float ridgedStrength = 0.0f;
    float craterStrength = 0.0f;
    float continentScale = 0.0f;
    float continentBlend = 0.15f;

    // Water / ocean
    float waterLevel = 0.0f;

    // Latitude effects
    float polarCapSize = 0.0f;
    float bandingStrength = 0.0f;
    float bandingFrequency = 20.0f;

    // Colors
    glm::vec3 waterColorDeep = {0.01f, 0.05f, 0.15f};
    glm::vec3 waterColorSurface = {0.02f, 0.12f, 0.27f};
    glm::vec3 sandColor = {1.0f, 1.0f, 0.85f};
    glm::vec3 treeColor = {0.02f, 0.1f, 0.06f};
    glm::vec3 rockColor = {0.15f, 0.12f, 0.12f};
    glm::vec3 iceColor = {0.8f, 0.9f, 0.9f};

    // Biome thresholds
    float sandLevel = 0.003f;
    float treeLevel = 0.004f;
    float rockLevel = 0.02f;
    float iceLevel = 0.04f;
    float transition = 0.01f;

    // Volumetric clouds
    float cloudsDensity = 0.01f;
    float cloudsScale = 0.8f;
    float cloudsSpeed = 1.5f;
    float cloudAltitude = 0.25f;
    float cloudThickness = 0.2f;

    // Atmosphere
    glm::vec3 atmosphereColor = {0.05f, 0.3f, 0.9f};
    float atmosphereDensity = 0.3f;

    // Cloud color
    glm::vec3 cloudColor = {1.0f, 1.0f, 1.0f};

    // Lighting
    glm::vec3 sunDirection = {1.0f, 1.0f, 0.5f};
    float sunIntensity = 3.0f;
    float ambientLight = 0.01f;
    glm::vec3 sunColor = {1.0f, 1.0f, 0.9f};
    glm::vec3 deepSpaceColor = {0.0f, 0.0f, 0.001f};

    // Black hole
    bool  isBlackHole = false;
    float bhMass = 1.0f;
    float bhAccretionInner = 3.0f;
    float bhAccretionOuter = 10.0f;
    float bhDiskSpeed = 1.0f;
    float bhDiskTurbulence = 0.3f;
    float bhDiskBrightness = 2.0f;
    float bhDiskTemperatureInner = 10000.0f;
    float bhDiskTemperatureOuter = 3000.0f;
    glm::vec3 bhDiskTint = {1.0f, 0.95f, 0.9f};
    int   bhRaySteps = 128;
    float bhDopplerStrength = 1.0f;

    // Rendering
    float rotationOffset = 0.6f;
    float quality = 1.0f;
};

class IRenderer {
public:
    virtual ~IRenderer() = default;

    virtual void init(int width, int height, void* nativeWindow = nullptr) = 0;
    virtual void resize(int width, int height) = 0;

    virtual void beginFrame() = 0;
    virtual void render(const Camera& camera) = 0;
    virtual void endFrame() = 0;

    virtual PlanetParams& params() = 0;
};

}  // namespace astrocore
