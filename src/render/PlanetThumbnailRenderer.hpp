#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <deque>
#include "render/ShaderProgram.hpp"
#include "render/Renderer.hpp"

namespace astrocore {

// A cached thumbnail with its texture and metadata
struct ThumbnailCache {
    GLuint textureId = 0;
    std::string planetName;
    PlanetParams params;
    float lastAccessTime = 0.0f;
    bool valid = false;

    // Color analysis for filtering
    glm::vec3 dominantHue{0.0f};
    float avgBrightness = 0.0f;
    bool hasAtmosphere = false;
    bool isEmissive = false;
};

// Renders planet thumbnails to framebuffer textures for catalog display
class PlanetThumbnailRenderer {
public:
    PlanetThumbnailRenderer();
    ~PlanetThumbnailRenderer();

    // Initialize with thumbnail size (square)
    void init(int thumbnailSize = 128);
    void shutdown();

    // Update time for rotation animation
    void update(float deltaTime);

    // Get or create a thumbnail for a planet
    // Returns texture ID (0 if not yet ready)
    GLuint getThumbnail(const std::string& planetName, const PlanetParams& params);

    // Request a thumbnail to be rendered (async-ish, renders one per frame)
    void requestThumbnail(const std::string& planetName, const PlanetParams& params);

    // Process render queue (call once per frame, renders up to maxPerFrame thumbnails)
    void processRenderQueue(int maxPerFrame = 2);

    // Check if a thumbnail is ready
    bool hasThumbnail(const std::string& planetName) const;

    // Get cached thumbnail info (for filtering)
    const ThumbnailCache* getThumbnailInfo(const std::string& planetName) const;

    // Clear unused thumbnails (call periodically to free memory)
    void clearUnused(float maxAge = 30.0f);

    // Clear all thumbnails
    void clearAll();

    // Get current cache stats
    int getCacheSize() const { return static_cast<int>(m_cache.size()); }
    int getQueueSize() const { return static_cast<int>(m_renderQueue.size()); }

private:
    void renderThumbnail(const std::string& planetName, const PlanetParams& params);
    void analyzeColors(ThumbnailCache& cache);

    int m_thumbnailSize = 128;
    float m_time = 0.0f;
    bool m_initialized = false;

    // Framebuffer for rendering
    GLuint m_fbo = 0;
    GLuint m_depthRbo = 0;

    // Shared resources
    GLuint m_quadVAO = 0;
    GLuint m_quadVBO = 0;
    GLuint m_noiseTexture = 0;
    ShaderProgram m_shader;

    // Thumbnail cache
    std::unordered_map<std::string, ThumbnailCache> m_cache;

    // Render queue
    struct RenderRequest {
        std::string planetName;
        PlanetParams params;
    };
    std::deque<RenderRequest> m_renderQueue;
};

}  // namespace astrocore
