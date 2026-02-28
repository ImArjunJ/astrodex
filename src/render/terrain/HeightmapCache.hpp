#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace astrocore {

// Manages GPU heightmap textures for terrain nodes
// Caches heightmaps to avoid regenerating them every frame
class HeightmapCache {
public:
    HeightmapCache();
    ~HeightmapCache();

    // Initialize with noise parameters
    void init(int resolution = 512);

    // Get or generate heightmap for a node's UV region
    // Returns texture ID and updates cache if needed
    GLuint getHeightmap(int face, const glm::vec2& minUV, const glm::vec2& maxUV, int level);

    // Generate the main 3D noise texture (same as Renderer)
    GLuint generateNoiseTexture(int size);

    // Get the 3D noise texture handle
    GLuint getNoiseTexture() const { return m_noiseTexture; }

    // Clear all cached heightmaps
    void clear();

    // Memory management - evict least recently used entries
    void evictLRU(size_t maxEntries = 1000);

private:
    struct CacheKey {
        int face;
        glm::vec2 minUV;
        glm::vec2 maxUV;
        int level;

        bool operator==(const CacheKey& other) const {
            return face == other.face &&
                   minUV == other.minUV &&
                   maxUV == other.maxUV &&
                   level == other.level;
        }
    };

    struct CacheKeyHash {
        size_t operator()(const CacheKey& key) const {
            size_t h = std::hash<int>()(key.face);
            h ^= std::hash<float>()(key.minUV.x) << 1;
            h ^= std::hash<float>()(key.minUV.y) << 2;
            h ^= std::hash<float>()(key.maxUV.x) << 3;
            h ^= std::hash<float>()(key.maxUV.y) << 4;
            h ^= std::hash<int>()(key.level) << 5;
            return h;
        }
    };

    struct CacheEntry {
        GLuint texture = 0;
        uint64_t lastUsed = 0;
    };

    GLuint generateHeightmap(int face, const glm::vec2& minUV, const glm::vec2& maxUV, int level);

    std::unordered_map<CacheKey, CacheEntry, CacheKeyHash> m_cache;
    GLuint m_noiseTexture = 0;
    int m_resolution = 512;
    uint64_t m_frameCounter = 0;
};

}  // namespace astrocore
