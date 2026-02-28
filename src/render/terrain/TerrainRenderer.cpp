#include "render/terrain/TerrainRenderer.hpp"
#include "render/Camera.hpp"
#include "render/Renderer.hpp"
#include "core/Logger.hpp"
#include <glm/gtc/matrix_transform.hpp>

namespace astrocore {

TerrainRenderer::TerrainRenderer() = default;
TerrainRenderer::~TerrainRenderer() = default;

void TerrainRenderer::init(const glm::vec3& planetCenter, float planetRadius) {
    m_planetCenter = planetCenter;
    m_planetRadius = planetRadius;

    LOG_INFO("Initializing TerrainRenderer (optimized)");

    // Load tessellation shaders
    if (!m_shader.loadTessellationFromFiles(
            "shaders/terrain.vert",
            "shaders/terrain.tcs",
            "shaders/terrain.tes",
            "shaders/terrain.frag")) {
        LOG_ERROR("Failed to load terrain tessellation shaders");
        return;
    }

    // Initialize heightmap cache (generates noise texture)
    m_heightmapCache.init();

    // Initialize quadtree (now with shared mesh)
    m_quadtree.init(planetCenter, planetRadius);

    m_initialized = true;
    LOG_INFO("TerrainRenderer initialized - instanced rendering enabled");
}

void TerrainRenderer::update(const Camera& camera) {
    if (!m_initialized) return;
    m_quadtree.update(camera);
}

void TerrainRenderer::render(const Camera& camera, const PlanetParameters& params) {
    if (!m_initialized) return;

    if (m_wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    m_shader.use();
    setupUniforms(camera, params);

    // Bind noise texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, m_heightmapCache.getNoiseTexture());
    m_shader.setInt("uNoiseTexture", 0);

    // Render all terrain in one instanced draw call
    m_quadtree.render(m_shader);

    m_shader.unuse();

    if (m_wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
}

void TerrainRenderer::setupUniforms(const Camera& camera, const PlanetParameters& params) {
    // Update planet radius from params
    m_planetRadius = params.planetRadius;

    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 viewProjection = camera.getProjectionMatrix() * camera.getViewMatrix();

    m_shader.setMat4("uModel", model);
    m_shader.setMat4("uViewProjection", viewProjection);
    m_shader.setVec3("uCameraPosition", camera.getPosition());
    m_shader.setVec3("uPlanetCenter", m_planetCenter);
    m_shader.setFloat("uPlanetRadius", params.planetRadius);
    m_shader.setFloat("uTessellationFactor", m_tessellationFactor);

    // Terrain shape - map new params to shader uniforms
    float noiseStrength = 0.1f + params.terrainRoughness * 0.2f;
    m_shader.setFloat("uNoiseStrength", noiseStrength);
    m_shader.setFloat("uTerrainScale", 0.5f + params.continentScale * 0.5f);

    int octaves = 4 + int(params.terrainRoughness * 4.0f);
    m_shader.setInt("uFbmOctaves", octaves);
    m_shader.setFloat("uFbmPersistence", params.terrainRoughness);
    m_shader.setFloat("uFbmLacunarity", params.terrainLacunarity);
    m_shader.setFloat("uFbmExponentiation", params.terrainExponent);
    m_shader.setFloat("uDomainWarpStrength", params.erosionModifier * 0.5f);
    m_shader.setFloat("uRidgedStrength", params.tectonicActivity);
    m_shader.setFloat("uCraterStrength", params.craterDensity);
    m_shader.setFloat("uContinentScale", params.continentScale);

    // Biome levels - map from new altitude-based thresholds
    m_shader.setFloat("uWaterLevel", params.seaLevel);
    m_shader.setFloat("uSandLevel", params.vegetationLine * 0.1f);
    m_shader.setFloat("uTreeLevel", params.vegetationLine * 0.15f);
    m_shader.setFloat("uRockLevel", params.treeLine * 0.2f);
    m_shader.setFloat("uIceLevel", params.snowLine * 0.25f);
    m_shader.setFloat("uTransition", params.biomeTransition);

    // Latitude effects
    m_shader.setFloat("uPolarCapSize", params.iceCapCoverage);
    m_shader.setFloat("uBandingStrength", params.bandingContrast);
    m_shader.setFloat("uBandingFrequency", params.bandingFrequency);

    // Colors - use new organized names
    m_shader.setVec3("uWaterColorDeep", params.colorDeepLiquid);
    m_shader.setVec3("uWaterColorSurface", params.colorShallowLiquid);
    m_shader.setVec3("uSandColor", params.colorLowland);
    m_shader.setVec3("uTreeColor", params.colorMidland);
    m_shader.setVec3("uRockColor", params.colorHighland);
    m_shader.setVec3("uIceColor", params.colorPeak);

    // Atmosphere - derive color from Rayleigh coefficients
    glm::vec3 rayleigh = params.rayleighCoefficients;
    float maxCoeff = glm::max(rayleigh.x, glm::max(rayleigh.y, rayleigh.z));
    glm::vec3 atmosphereColor = (maxCoeff > 0.0f) ? rayleigh / maxCoeff : glm::vec3(0.3f, 0.5f, 1.0f);
    m_shader.setVec3("uAtmosphereColor", atmosphereColor);
    m_shader.setFloat("uAtmosphereDensity", 1.0f / params.atmosphereDensityFalloff);

    // Lighting
    m_shader.setVec3("uSunDirection", glm::normalize(params.sunDirection));
    m_shader.setFloat("uSunIntensity", params.sunIntensity);
    m_shader.setFloat("uAmbientLight", params.ambientLight);
    m_shader.setVec3("uSunColor", params.sunColor);
}

TerrainRenderer::Stats TerrainRenderer::getStats() const {
    Stats stats;
    auto qtStats = m_quadtree.getStats();
    stats.totalNodes = qtStats.visibleNodes;
    stats.leafNodes = qtStats.totalInstances;
    stats.maxLevel = 0;  // Could track this in quadtree
    stats.trianglesRendered = qtStats.totalInstances * TerrainMesh::GRID_SIZE * TerrainMesh::GRID_SIZE * 2;
    return stats;
}

}  // namespace astrocore
