#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>
#include "render/ShaderProgram.hpp"
#include "render/SphereRenderer.hpp"
#include "render/OrbitRenderer.hpp"
#include "render/RingRenderer.hpp"
#include <memory>
#include <string>
#include <unordered_map>

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

// All the knobs for procedural planet generation
// Default values match the reference "Procedural Blue Planet" highest quality preset
struct PlanetParams {
    // Planet geometry
    float radius = 2.0f;
    float rotationSpeed = 0.1f;

    // Noise type selection
    NoiseType noiseType = NoiseType::Standard;

    // Terrain - realistic Earth-like values (Everest is only 0.14% of Earth radius)
    float noiseStrength = 0.02f;  // ~1% of radius for visible but realistic terrain
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
    float continentBlend = 0.15f;   // smoothness of continent edges (0.01 = sharp, 0.3 = very smooth)

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

    // Biome thresholds (altitude-based) - scaled for realistic terrain
    float sandLevel = 0.003f;
    float treeLevel = 0.004f;
    float rockLevel = 0.02f;
    float iceLevel = 0.04f;
    float transition = 0.01f;  // Smooth biome transitions

    // Volumetric clouds
    float cloudsDensity = 0.01f;    // MAX 0.02! Higher values look bad
    float cloudsScale = 0.8f;       // Lower = larger cloud shapes
    float cloudsSpeed = 1.5f;
    float cloudAltitude = 0.25f;    // Height above surface (elevated for better separation)
    float cloudThickness = 0.2f;    // Vertical extent of cloud layer (thicker for more volume)

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
    void render(const Camera& camera, bool isEmissive = false);
    void endFrame();

    PlanetParams& params() { return m_params; }
    SphereRenderer& sphereRenderer() { return m_sphereRenderer; }
    OrbitRenderer& orbitRenderer() { return m_orbitRenderer; }

    // Set planet position for detailed rendering
    void setPlanetPosition(const glm::vec3& pos) { m_planetPosition = pos; }

    // Render starfield background
    void renderStarfield(const Camera& camera);

    // Ring rendering
    void setupRing(uint64_t bodyId, const RingParams& params, float planetRadius, float planetMass);
    void updateRings(float deltaTime);
    void renderRing(uint64_t bodyId, const Camera& camera, const glm::vec3& planetPos);
    RingParams* getRingParams(uint64_t bodyId);
    bool hasRing(uint64_t bodyId) const;
    void clearRings();

private:
    void createQuad();
    void generateNoiseTexture(int size);

    int m_width = 0;
    int m_height = 0;

    GLuint m_quadVAO = 0;
    GLuint m_quadVBO = 0;
    GLuint m_noiseTexture = 0;

    ShaderProgram m_shader;
    ShaderProgram m_starfieldShader;
    PlanetParams m_params;
    float m_time = 0.0f;
    glm::vec3 m_planetPosition{0.0f, 0.0f, -10.0f};

    SphereRenderer m_sphereRenderer;
    OrbitRenderer m_orbitRenderer;

    // Ring rendering per body
    std::unordered_map<uint64_t, std::unique_ptr<RingRenderer>> m_ringRenderers;
    std::unordered_map<uint64_t, RingParams> m_ringParams;
};

}  // namespace astrocore
