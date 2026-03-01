#include "render/PlanetThumbnailRenderer.hpp"
#include "core/Logger.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cstdint>

namespace astrocore {

PlanetThumbnailRenderer::PlanetThumbnailRenderer() = default;

PlanetThumbnailRenderer::~PlanetThumbnailRenderer() {
    shutdown();
}

void PlanetThumbnailRenderer::init(int thumbnailSize) {
    if (m_initialized) return;

    m_thumbnailSize = thumbnailSize;

    // Create quad for fullscreen rendering
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

    // Create framebuffer
    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    // Create depth renderbuffer
    glGenRenderbuffers(1, &m_depthRbo);
    glBindRenderbuffer(GL_RENDERBUFFER, m_depthRbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, thumbnailSize, thumbnailSize);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthRbo);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Load shader
    if (!m_shader.loadFromFiles("shaders/planet.vert", "shaders/planet.frag")) {
        LOG_ERROR("PlanetThumbnailRenderer: Failed to load planet shader");
    }

    // Generate noise texture (smaller for thumbnails)
    const int noiseSize = 256;
    size_t totalSize = size_t(noiseSize) * noiseSize * noiseSize;
    std::vector<uint8_t> noiseData(totalSize);

    uint32_t seed = 0x12345678;
    for (size_t i = 0; i < totalSize; ++i) {
        seed = seed * 1664525u + 1013904223u;
        noiseData[i] = static_cast<uint8_t>(seed >> 24);
    }

    glGenTextures(1, &m_noiseTexture);
    glBindTexture(GL_TEXTURE_3D, m_noiseTexture);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R8, noiseSize, noiseSize, noiseSize, 0,
                 GL_RED, GL_UNSIGNED_BYTE, noiseData.data());
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    m_initialized = true;
    LOG_INFO("PlanetThumbnailRenderer initialized ({}x{} tiles)", thumbnailSize, thumbnailSize);
}

void PlanetThumbnailRenderer::shutdown() {
    if (!m_initialized) return;

    // Delete all cached textures
    for (auto& [name, cache] : m_cache) {
        if (cache.textureId != 0) {
            glDeleteTextures(1, &cache.textureId);
        }
    }
    m_cache.clear();
    m_renderQueue.clear();

    if (m_fbo) glDeleteFramebuffers(1, &m_fbo);
    if (m_depthRbo) glDeleteRenderbuffers(1, &m_depthRbo);
    if (m_quadVAO) glDeleteVertexArrays(1, &m_quadVAO);
    if (m_quadVBO) glDeleteBuffers(1, &m_quadVBO);
    if (m_noiseTexture) glDeleteTextures(1, &m_noiseTexture);

    m_fbo = 0;
    m_depthRbo = 0;
    m_quadVAO = 0;
    m_quadVBO = 0;
    m_noiseTexture = 0;

    m_initialized = false;
}

void PlanetThumbnailRenderer::update(float deltaTime) {
    m_time += deltaTime;
}

GLuint PlanetThumbnailRenderer::getThumbnail(const std::string& planetName, const PlanetParams& params) {
    auto it = m_cache.find(planetName);
    if (it != m_cache.end() && it->second.valid) {
        it->second.lastAccessTime = m_time;
        return it->second.textureId;
    }

    // Not cached, request render
    requestThumbnail(planetName, params);
    return 0;
}

void PlanetThumbnailRenderer::requestThumbnail(const std::string& planetName, const PlanetParams& params) {
    // Check if already in queue
    for (const auto& req : m_renderQueue) {
        if (req.planetName == planetName) return;
    }

    // Check if already cached
    if (m_cache.find(planetName) != m_cache.end() && m_cache[planetName].valid) {
        return;
    }

    m_renderQueue.push_back({planetName, params});
}

void PlanetThumbnailRenderer::processRenderQueue(int maxPerFrame) {
    if (!m_initialized || m_renderQueue.empty()) return;

    int rendered = 0;
    while (!m_renderQueue.empty() && rendered < maxPerFrame) {
        auto req = m_renderQueue.front();
        m_renderQueue.pop_front();

        renderThumbnail(req.planetName, req.params);
        ++rendered;
    }
}

bool PlanetThumbnailRenderer::hasThumbnail(const std::string& planetName) const {
    auto it = m_cache.find(planetName);
    return it != m_cache.end() && it->second.valid;
}

const ThumbnailCache* PlanetThumbnailRenderer::getThumbnailInfo(const std::string& planetName) const {
    auto it = m_cache.find(planetName);
    return (it != m_cache.end() && it->second.valid) ? &it->second : nullptr;
}

void PlanetThumbnailRenderer::clearUnused(float maxAge) {
    std::vector<std::string> toRemove;

    for (auto& [name, cache] : m_cache) {
        if (m_time - cache.lastAccessTime > maxAge) {
            if (cache.textureId != 0) {
                glDeleteTextures(1, &cache.textureId);
            }
            toRemove.push_back(name);
        }
    }

    for (const auto& name : toRemove) {
        m_cache.erase(name);
    }

    if (!toRemove.empty()) {
        LOG_DEBUG("Cleared {} unused thumbnails", toRemove.size());
    }
}

void PlanetThumbnailRenderer::clearAll() {
    for (auto& [name, cache] : m_cache) {
        if (cache.textureId != 0) {
            glDeleteTextures(1, &cache.textureId);
        }
    }
    m_cache.clear();
    m_renderQueue.clear();
}

void PlanetThumbnailRenderer::renderThumbnail(const std::string& planetName, const PlanetParams& params) {
    // Create texture for this thumbnail
    GLuint texId = 0;
    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_thumbnailSize, m_thumbnailSize, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Attach to framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texId, 0);

    // Check framebuffer completeness
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        LOG_ERROR("Thumbnail FBO incomplete for {}", planetName);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteTextures(1, &texId);
        return;
    }

    // Save current viewport
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    // Set viewport for thumbnail
    glViewport(0, 0, m_thumbnailSize, m_thumbnailSize);

    // Clear
    glClearColor(0.0f, 0.0f, 0.02f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Enable depth test and blending
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Use shader
    m_shader.use();

    // Setup camera - view looking at planet, zoomed out to fit whole planet
    float aspect = 1.0f;
    // Position camera to fit planet with atmosphere in frame
    // Use a consistent setup regardless of planet radius - shader handles scaling
    glm::vec3 camPos = glm::vec3(0.0f, 0.0f, 8.0f);
    glm::vec3 planetPos = glm::vec3(0.0f, 0.0f, -5.0f);  // Planet in front of camera

    glm::mat4 view = glm::lookAt(camPos, planetPos, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
    glm::mat4 viewProj = proj * view;

    m_shader.setInt("uIsEmissive", 0);
    m_shader.setVec2("uResolution", glm::vec2(m_thumbnailSize, m_thumbnailSize));
    m_shader.setFloat("uTime", m_time);
    m_shader.setFloat("uRotationSpeed", params.rotationSpeed);
    m_shader.setFloat("uRotationOffset", params.rotationOffset);
    m_shader.setFloat("uQuality", 0.5f);  // Lower quality for thumbnails

    m_shader.setVec3("uCameraPosition", camPos);
    m_shader.setMat4("uInvView", glm::inverse(view));
    m_shader.setMat4("uInvProjection", glm::inverse(proj));
    m_shader.setMat4("uViewProjection", viewProj);

    m_shader.setVec3("uPlanetPosition", planetPos);
    // Normalize radius to fit in thumbnail - always use radius ~1.5 for consistent framing
    float normalizedRadius = 1.5f;
    m_shader.setFloat("uPlanetRadius", normalizedRadius);
    m_shader.setFloat("uNoiseStrength", params.noiseStrength);
    m_shader.setFloat("uTerrainScale", params.terrainScale);

    m_shader.setInt("uNoiseType", static_cast<int>(params.noiseType));
    m_shader.setInt("uFbmOctaves", std::min(params.fbmOctaves, 4));  // Fewer octaves for speed
    m_shader.setFloat("uFbmPersistence", params.fbmPersistence);
    m_shader.setFloat("uFbmLacunarity", params.fbmLacunarity);
    m_shader.setFloat("uFbmExponentiation", params.fbmExponentiation);
    m_shader.setFloat("uDomainWarpStrength", params.domainWarpStrength);
    m_shader.setFloat("uRidgedStrength", params.ridgedStrength);
    m_shader.setFloat("uCraterStrength", params.craterStrength);
    m_shader.setFloat("uContinentScale", params.continentScale);
    m_shader.setFloat("uContinentBlend", params.continentBlend);
    m_shader.setFloat("uWaterLevel", params.waterLevel);
    m_shader.setFloat("uPolarCapSize", params.polarCapSize);
    m_shader.setFloat("uBandingStrength", params.bandingStrength);
    m_shader.setFloat("uBandingFrequency", params.bandingFrequency);

    m_shader.setVec3("uWaterColorDeep", params.waterColorDeep);
    m_shader.setVec3("uWaterColorSurface", params.waterColorSurface);
    m_shader.setVec3("uSandColor", params.sandColor);
    m_shader.setVec3("uTreeColor", params.treeColor);
    m_shader.setVec3("uRockColor", params.rockColor);
    m_shader.setVec3("uIceColor", params.iceColor);

    m_shader.setFloat("uSandLevel", params.sandLevel);
    m_shader.setFloat("uTreeLevel", params.treeLevel);
    m_shader.setFloat("uRockLevel", params.rockLevel);
    m_shader.setFloat("uIceLevel", params.iceLevel);
    m_shader.setFloat("uTransition", params.transition);

    m_shader.setFloat("uCloudsDensity", params.cloudsDensity);
    m_shader.setFloat("uCloudsScale", params.cloudsScale);
    m_shader.setFloat("uCloudsSpeed", params.cloudsSpeed);
    m_shader.setFloat("uCloudAltitude", params.cloudAltitude);
    m_shader.setFloat("uCloudThickness", params.cloudThickness);
    m_shader.setVec3("uCloudColor", params.cloudColor);

    m_shader.setVec3("uAtmosphereColor", params.atmosphereColor);
    m_shader.setFloat("uAtmosphereDensity", params.atmosphereDensity);

    m_shader.setVec3("uSunDirection", glm::normalize(params.sunDirection));
    m_shader.setFloat("uSunIntensity", params.sunIntensity);
    m_shader.setFloat("uAmbientLight", params.ambientLight);
    m_shader.setVec3("uSunColor", params.sunColor);
    m_shader.setVec3("uDeepSpaceColor", params.deepSpaceColor);

    // Bind noise texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, m_noiseTexture);
    m_shader.setInt("uNoiseTexture", 0);

    // Draw
    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    m_shader.unuse();
    glDisable(GL_BLEND);

    // Restore viewport and framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

    // Store in cache
    ThumbnailCache cache;
    cache.textureId = texId;
    cache.planetName = planetName;
    cache.params = params;
    cache.lastAccessTime = m_time;
    cache.valid = true;

    // Analyze colors for filtering
    analyzeColors(cache);

    m_cache[planetName] = std::move(cache);

    LOG_DEBUG("Rendered thumbnail for {}", planetName);
}

void PlanetThumbnailRenderer::analyzeColors(ThumbnailCache& cache) {
    const auto& p = cache.params;

    // Calculate dominant hue from planet colors
    // Weight surface colors more than water for land-dominant planets
    glm::vec3 avgColor = p.sandColor * 0.2f + p.treeColor * 0.3f +
                         p.rockColor * 0.2f + p.waterColorSurface * 0.3f;

    // Convert to HSV-ish for hue
    float maxC = std::max({avgColor.r, avgColor.g, avgColor.b});
    float minC = std::min({avgColor.r, avgColor.g, avgColor.b});
    float delta = maxC - minC;

    float hue = 0.0f;
    if (delta > 0.001f) {
        if (maxC == avgColor.r) {
            hue = 60.0f * std::fmod((avgColor.g - avgColor.b) / delta, 6.0f);
        } else if (maxC == avgColor.g) {
            hue = 60.0f * ((avgColor.b - avgColor.r) / delta + 2.0f);
        } else {
            hue = 60.0f * ((avgColor.r - avgColor.g) / delta + 4.0f);
        }
        if (hue < 0.0f) hue += 360.0f;
    }

    cache.dominantHue = glm::vec3(hue / 360.0f, delta / (maxC + 0.001f), maxC);
    cache.avgBrightness = (avgColor.r + avgColor.g + avgColor.b) / 3.0f;
    cache.hasAtmosphere = p.atmosphereDensity > 0.1f;
    cache.isEmissive = p.bandingStrength > 0.5f;  // Gas giants often have banding
}

}  // namespace astrocore
