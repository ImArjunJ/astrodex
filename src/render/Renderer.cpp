#include "render/Renderer.hpp"
#include "render/Camera.hpp"
#include "core/Logger.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <cstdint>

namespace astrocore {

Renderer::Renderer() = default;

Renderer::~Renderer() {
    m_sphereRenderer.shutdown();
    m_orbitRenderer.shutdown();
    if (m_quadVAO) glDeleteVertexArrays(1, &m_quadVAO);
    if (m_quadVBO) glDeleteBuffers(1, &m_quadVBO);
    if (m_noiseTexture) glDeleteTextures(1, &m_noiseTexture);
}

void Renderer::init(int width, int height) {
    m_width = width;
    m_height = height;

    createQuad();

    // Load shaders
    if (!m_shader.loadFromFiles("shaders/planet.vert", "shaders/planet.frag")) {
        LOG_ERROR("Failed to load planet shader");
    }

    if (!m_starfieldShader.loadFromFiles("shaders/starfield.vert", "shaders/starfield.frag")) {
        LOG_ERROR("Failed to load starfield shader");
    }

    // Generate high-res 3D noise texture procedurally
    generateNoiseTexture(512);

    // Initialize sphere renderer for multi-body rendering
    m_sphereRenderer.init();

    // Initialize orbit renderer for trail visualization
    m_orbitRenderer.init();

    glEnable(GL_DEPTH_TEST);
}

void Renderer::createQuad() {
    float vertices[] = {
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
    };

    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);

    glBindVertexArray(m_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);
}


void Renderer::generateNoiseTexture(int size) {
    LOG_INFO("Generating {}x{}x{} random 3D noise texture ({} MB)...",
             size, size, size, (size_t(size) * size * size) / (1024 * 1024));

    size_t totalSize = size_t(size) * size * size;
    std::vector<uint8_t> data(totalSize);

    // Fill with random bytes — same concept as the original 32^3 binary file.
    // The shader's FBM builds structure; the texture just needs to be random.
    // GPU trilinear filtering (GL_LINEAR) smooths between texels.
    // Using a simple LCG for speed — quality doesn't matter, just needs to be random.
    uint32_t seed = 0x12345678;
    for (size_t i = 0; i < totalSize; ++i) {
        seed = seed * 1664525u + 1013904223u; // LCG
        data[i] = static_cast<uint8_t>(seed >> 24);
    }

    glGenTextures(1, &m_noiseTexture);
    glBindTexture(GL_TEXTURE_3D, m_noiseTexture);

    glTexImage3D(GL_TEXTURE_3D, 0, GL_R8, size, size, size, 0,
                 GL_RED, GL_UNSIGNED_BYTE, data.data());

    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    LOG_INFO("Generated 3D noise texture: {}x{}x{}", size, size, size);
}

void Renderer::resize(int width, int height) {
    m_width = width;
    m_height = height;
    glViewport(0, 0, width, height);
}

void Renderer::beginFrame() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::render(const Camera& camera, bool isEmissive) {
    // Update time with pause and scale support
    if (!m_paused) {
        m_time += 0.016f * m_timeScale;  // ~60fps tick, scaled
    }

    // Enable depth test so planets occlude each other properly
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    // Enable alpha blending for atmosphere glow
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_shader.use();

    // Tell shader if this is an emissive body (star/sun)
    m_shader.setInt("uIsEmissive", isEmissive ? 1 : 0);

    // Resolution and time
    m_shader.setVec2("uResolution", glm::vec2(m_width, m_height));
    m_shader.setFloat("uTime", m_time);
    m_shader.setFloat("uRotationSpeed", m_params.rotationSpeed);
    m_shader.setFloat("uRotationOffset", m_params.rotationOffset);
    m_shader.setFloat("uQuality", m_params.quality);

    // Camera
    glm::vec3 camPos = camera.getPosition();
    m_shader.setVec3("uCameraPosition", camPos);
    m_shader.setMat4("uInvView", glm::inverse(camera.getViewMatrix()));
    m_shader.setMat4("uInvProjection", glm::inverse(camera.getProjectionMatrix()));
    m_shader.setMat4("uViewProjection", camera.getViewProjectionMatrix());

    // Planet
    m_shader.setVec3("uPlanetPosition", m_planetPosition);
    m_shader.setFloat("uPlanetRadius", m_params.radius);
    m_shader.setFloat("uNoiseStrength", m_params.noiseStrength);
    m_shader.setFloat("uTerrainScale", m_params.terrainScale);

    // Noise type
    m_shader.setInt("uNoiseType", static_cast<int>(m_params.noiseType));

    // FBM terrain shape
    m_shader.setInt("uFbmOctaves", m_params.fbmOctaves);
    m_shader.setFloat("uFbmPersistence", m_params.fbmPersistence);
    m_shader.setFloat("uFbmLacunarity", m_params.fbmLacunarity);
    m_shader.setFloat("uFbmExponentiation", m_params.fbmExponentiation);
    m_shader.setFloat("uDomainWarpStrength", m_params.domainWarpStrength);
    m_shader.setFloat("uRidgedStrength", m_params.ridgedStrength);
    m_shader.setFloat("uCraterStrength", m_params.craterStrength);
    m_shader.setFloat("uContinentScale", m_params.continentScale);
    m_shader.setFloat("uContinentBlend", m_params.continentBlend);
    m_shader.setFloat("uWaterLevel", m_params.waterLevel);
    m_shader.setFloat("uPolarCapSize", m_params.polarCapSize);
    m_shader.setFloat("uBandingStrength", m_params.bandingStrength);
    m_shader.setFloat("uBandingFrequency", m_params.bandingFrequency);

    // Gas giant storm system
    m_shader.setFloat("uStormCount", m_params.stormCount);
    m_shader.setFloat("uStormSize", m_params.stormSize);
    m_shader.setFloat("uStormIntensity", m_params.stormIntensity);
    m_shader.setFloat("uStormSeed", m_params.stormSeed);
    m_shader.setVec3("uStormColor", m_params.stormColor);
    m_shader.setFloat("uFlowSpeed", m_params.flowSpeed);
    m_shader.setFloat("uTurbulenceScale", m_params.turbulenceScale);
    m_shader.setFloat("uVortexTightness", m_params.vortexTightness);

    // Colors
    m_shader.setVec3("uWaterColorDeep", m_params.waterColorDeep);
    m_shader.setVec3("uWaterColorSurface", m_params.waterColorSurface);
    m_shader.setVec3("uSandColor", m_params.sandColor);
    m_shader.setVec3("uTreeColor", m_params.treeColor);
    m_shader.setVec3("uRockColor", m_params.rockColor);
    m_shader.setVec3("uIceColor", m_params.iceColor);

    // Biome levels
    m_shader.setFloat("uSandLevel", m_params.sandLevel);
    m_shader.setFloat("uTreeLevel", m_params.treeLevel);
    m_shader.setFloat("uRockLevel", m_params.rockLevel);
    m_shader.setFloat("uIceLevel", m_params.iceLevel);
    m_shader.setFloat("uTransition", m_params.transition);

    // Clouds
    m_shader.setFloat("uCloudsDensity", m_params.cloudsDensity);
    m_shader.setFloat("uCloudsScale", m_params.cloudsScale);
    m_shader.setFloat("uCloudsSpeed", m_params.cloudsSpeed);
    m_shader.setFloat("uCloudAltitude", m_params.cloudAltitude);
    m_shader.setFloat("uCloudThickness", m_params.cloudThickness);
    m_shader.setVec3("uCloudColor", m_params.cloudColor);

    // Atmosphere
    m_shader.setVec3("uAtmosphereColor", m_params.atmosphereColor);
    m_shader.setFloat("uAtmosphereDensity", m_params.atmosphereDensity);

    // Lighting
    m_shader.setVec3("uSunDirection", glm::normalize(m_params.sunDirection));
    m_shader.setFloat("uSunIntensity", m_params.sunIntensity);
    m_shader.setFloat("uAmbientLight", m_params.ambientLight);
    m_shader.setVec3("uSunColor", m_params.sunColor);
    m_shader.setVec3("uDeepSpaceColor", m_params.deepSpaceColor);

    // Bind noise texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, m_noiseTexture);
    m_shader.setInt("uNoiseTexture", 0);

    // Draw
    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    m_shader.unuse();

    // Disable blending after planet rendering
    glDisable(GL_BLEND);
}

void Renderer::endFrame() {
    // Nothing
}

void Renderer::renderStarfield(const Camera& camera) {
    // Render starfield as background (before other geometry)
    glDepthMask(GL_FALSE);  // Don't write to depth buffer
    glDepthFunc(GL_LEQUAL); // Render at far plane

    m_starfieldShader.use();
    m_starfieldShader.setMat4("uInvViewProj", glm::inverse(camera.getViewProjectionMatrix()));
    m_starfieldShader.setFloat("uTime", m_time);

    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    m_starfieldShader.unuse();

    glDepthMask(GL_TRUE);   // Re-enable depth writing
    glDepthFunc(GL_LESS);   // Normal depth test
}

void Renderer::setupRing(uint64_t bodyId, const RingParams& params, float planetRadius, float planetMass) {
    m_ringParams[bodyId] = params;

    if (params.enabled) {
        if (m_ringRenderers.find(bodyId) == m_ringRenderers.end()) {
            m_ringRenderers[bodyId] = std::make_unique<RingRenderer>();
            m_ringRenderers[bodyId]->init();
        }
        m_ringRenderers[bodyId]->generateRing(params, planetRadius, planetMass);
    }
}

void Renderer::updateRings(float deltaTime) {
    for (auto& [id, renderer] : m_ringRenderers) {
        renderer->update(deltaTime, 1.0f);  // Mass not used currently
    }
}

void Renderer::renderRing(uint64_t bodyId, const Camera& camera, const glm::vec3& planetPos) {
    auto it = m_ringRenderers.find(bodyId);
    if (it == m_ringRenderers.end()) return;

    auto paramsIt = m_ringParams.find(bodyId);
    if (paramsIt == m_ringParams.end()) return;

    it->second->render(camera.getViewProjectionMatrix(), planetPos,
                       camera.getPosition(), paramsIt->second);
}

RingParams* Renderer::getRingParams(uint64_t bodyId) {
    auto it = m_ringParams.find(bodyId);
    return (it != m_ringParams.end()) ? &it->second : nullptr;
}

bool Renderer::hasRing(uint64_t bodyId) const {
    auto it = m_ringParams.find(bodyId);
    return it != m_ringParams.end() && it->second.enabled;
}

void Renderer::clearRings() {
    m_ringRenderers.clear();
    m_ringParams.clear();
}

}  // namespace astrocore
