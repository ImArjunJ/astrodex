#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>
#include "render/ShaderProgram.hpp"
#include <memory>
#include <string>

namespace astrocore {

class Camera;

// All the knobs for procedural planet generation
// Default values match the reference "Procedural Blue Planet" highest quality preset
struct PlanetParams {
    // Planet geometry
    float radius = 2.0f;
    float rotationSpeed = 0.1f;

    // Terrain - reference values for beautiful Earth-like planet
    float noiseStrength = 0.2f;
    float terrainScale = 0.8f;

    // FBM shape — these fundamentally change terrain character
    int   fbmOctaves = 6;
    float fbmPersistence = 0.5f;    // low = smooth, high = rough/noisy
    float fbmLacunarity = 2.0f;     // frequency multiplier per octave
    float fbmExponentiation = 5.0f; // low = flat plateaus, high = sharp peaks
    float domainWarpStrength = 0.0f;// 0 = regular terrain, >0 = organic/alien shapes

    // Terrain features
    float ridgedStrength = 0.0f;    // 0 = none, 1 = full ridged mountains
    float craterStrength = 0.0f;    // 0 = none, 1 = heavy craters
    float continentScale = 0.0f;    // 0 = off, >0 = large-scale continent shaping

    // Water / ocean
    float waterLevel = 0.0f;        // sea level — higher = more ocean

    // Latitude effects
    float polarCapSize = 0.0f;      // 0 = none, 1 = huge polar ice caps
    float bandingStrength = 0.0f;   // 0 = none, 1 = strong horizontal bands (gas giants)
    float bandingFrequency = 20.0f; // how many latitude bands

    // Colors - from reference shader
    glm::vec3 waterColorDeep = {0.01f, 0.05f, 0.15f};
    glm::vec3 waterColorSurface = {0.02f, 0.12f, 0.27f};
    glm::vec3 sandColor = {1.0f, 1.0f, 0.85f};
    glm::vec3 treeColor = {0.02f, 0.1f, 0.06f};
    glm::vec3 rockColor = {0.15f, 0.12f, 0.12f};
    glm::vec3 iceColor = {0.8f, 0.9f, 0.9f};

    // Biome thresholds (altitude-based)
    float sandLevel = 0.028f;
    float treeLevel = 0.03f;
    float rockLevel = 0.1f;
    float iceLevel = 0.15f;
    float transition = 0.02f;

    // Volumetric clouds
    float cloudsDensity = 0.5f;
    float cloudsScale = 1.0f;
    float cloudsSpeed = 1.5f;
    float cloudAltitude = 0.15f;    // height above surface
    float cloudThickness = 0.1f;    // vertical extent of cloud layer

    // Atmosphere - beautiful blue glow
    glm::vec3 atmosphereColor = {0.05f, 0.3f, 0.9f};
    float atmosphereDensity = 0.3f;

    // Cloud color
    glm::vec3 cloudColor = {1.0f, 1.0f, 1.0f};

    // Lighting - bright sun for dramatic effect
    glm::vec3 sunDirection = {1.0f, 1.0f, 0.5f};
    float sunIntensity = 3.0f;
    float ambientLight = 0.01f;
    glm::vec3 sunColor = {1.0f, 1.0f, 0.9f};
    glm::vec3 deepSpaceColor = {0.0f, 0.0f, 0.001f};

    // Rendering
    float rotationOffset = 0.6f;
    float quality = 1.0f;
};

class Renderer {
public:
    Renderer();
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void init(int width, int height);
    void resize(int width, int height);

    void beginFrame();
    void render(const Camera& camera);
    void endFrame();

    PlanetParams& params() { return m_params; }

private:
    void createQuad();
    void generateNoiseTexture(int size);

    int m_width = 0;
    int m_height = 0;

    GLuint m_quadVAO = 0;
    GLuint m_quadVBO = 0;
    GLuint m_noiseTexture = 0;

    ShaderProgram m_shader;
    PlanetParams m_params;
    float m_time = 0.0f;
};

}  // namespace astrocore
