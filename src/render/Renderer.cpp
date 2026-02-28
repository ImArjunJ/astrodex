#include "render/Renderer.hpp"
#include "render/Camera.hpp"
#include "core/Logger.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <cstdint>

namespace astrocore {

Renderer::Renderer() = default;

Renderer::~Renderer() {
    if (m_quadVAO) glDeleteVertexArrays(1, &m_quadVAO);
    if (m_quadVBO) glDeleteBuffers(1, &m_quadVBO);
    if (m_noiseTexture) glDeleteTextures(1, &m_noiseTexture);
}

void Renderer::init(int width, int height, void* /*nativeWindow*/) {
    m_width = width;
    m_height = height;

    createQuad();

    // Load shader
    if (!m_shader.loadFromFiles("shaders/planet.vert", "shaders/planet.frag")) {
        LOG_ERROR("Failed to load planet shader");
    }

    // Generate high-res 3D noise texture procedurally
    generateNoiseTexture(512);

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

void Renderer::render(const Camera& camera) {
    m_time += 0.016f;  // ~60fps tick

    m_shader.use();

    // Resolution and time
    m_shader.setVec2("uResolution", glm::vec2(m_width, m_height));
    m_shader.setFloat("uTime", m_time);
    m_shader.setFloat("uRotationSpeed", m_params.rotationSpeed);
    m_shader.setFloat("uRotationOffset", m_params.rotationOffset);

    // Precompute planet rotation matrix on CPU
    float angle = m_time * m_params.rotationSpeed + m_params.rotationOffset;
    float c = glm::cos(angle), s = glm::sin(angle);
    glm::mat3 planetRotation(
        glm::vec3( c, 0, s),
        glm::vec3( 0, 1, 0),
        glm::vec3(-s, 0, c)
    );
    m_shader.setMat3("uPlanetRotation", planetRotation);
    m_shader.setFloat("uQuality", m_params.quality);

    // Camera
    glm::vec3 camPos = camera.getPosition();
    m_shader.setVec3("uCameraPosition", camPos);
    m_shader.setMat4("uInvView", glm::inverse(camera.getViewMatrix()));

    // Planet
    m_shader.setVec3("uPlanetPosition", glm::vec3(0.0f, 0.0f, -10.0f));
    m_shader.setFloat("uPlanetRadius", m_params.radius);
    m_shader.setFloat("uNoiseStrength", m_params.noiseStrength);
    m_shader.setFloat("uTerrainScale", m_params.terrainScale);

    // FBM terrain shape
    m_shader.setInt("uFbmOctaves", m_params.fbmOctaves);
    m_shader.setFloat("uFbmPersistence", m_params.fbmPersistence);
    m_shader.setFloat("uFbmLacunarity", m_params.fbmLacunarity);
    m_shader.setFloat("uFbmExponentiation", m_params.fbmExponentiation);
    m_shader.setFloat("uDomainWarpStrength", m_params.domainWarpStrength);
    m_shader.setFloat("uRidgedStrength", m_params.ridgedStrength);
    m_shader.setFloat("uCraterStrength", m_params.craterStrength);
    m_shader.setFloat("uContinentScale", m_params.continentScale);
    m_shader.setFloat("uWaterLevel", m_params.waterLevel);
    m_shader.setFloat("uPolarCapSize", m_params.polarCapSize);
    m_shader.setFloat("uBandingStrength", m_params.bandingStrength);
    m_shader.setFloat("uBandingFrequency", m_params.bandingFrequency);

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

    // Black hole
    m_shader.setBool("uIsBlackHole", m_params.isBlackHole);
    m_shader.setFloat("uBhMass", m_params.bhMass);
    m_shader.setFloat("uBhAccretionInner", m_params.bhAccretionInner);
    m_shader.setFloat("uBhAccretionOuter", m_params.bhAccretionOuter);
    m_shader.setFloat("uBhDiskSpeed", m_params.bhDiskSpeed);
    m_shader.setFloat("uBhDiskTurbulence", m_params.bhDiskTurbulence);
    m_shader.setFloat("uBhDiskBrightness", m_params.bhDiskBrightness);
    m_shader.setFloat("uBhTempInner", m_params.bhDiskTemperatureInner);
    m_shader.setFloat("uBhTempOuter", m_params.bhDiskTemperatureOuter);
    m_shader.setVec3("uBhDiskTint", m_params.bhDiskTint);
    m_shader.setInt("uBhRaySteps", m_params.bhRaySteps);
    m_shader.setFloat("uBhDopplerStrength", m_params.bhDopplerStrength);

    // Bind noise texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, m_noiseTexture);
    m_shader.setInt("uNoiseTexture", 0);

    // Draw
    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    m_shader.unuse();
}

void Renderer::endFrame() {
    // Nothing
}

}  // namespace astrocore
