#include "render/terrain/HeightmapCache.hpp"
#include "core/Logger.hpp"
#include <algorithm>
#include <vector>

namespace astrocore {

HeightmapCache::HeightmapCache() = default;

HeightmapCache::~HeightmapCache() {
    clear();
    if (m_noiseTexture) {
        glDeleteTextures(1, &m_noiseTexture);
    }
}

void HeightmapCache::init(int resolution) {
    m_resolution = resolution;

    // Generate 3D noise texture
    m_noiseTexture = generateNoiseTexture(512);

    LOG_INFO("HeightmapCache initialized with resolution={}", resolution);
}

GLuint HeightmapCache::generateNoiseTexture(int size) {
    LOG_INFO("Generating {}x{}x{} noise texture for terrain...", size, size, size);

    size_t totalSize = size_t(size) * size * size;
    std::vector<uint8_t> data(totalSize);

    // Fill with random bytes using LCG
    uint32_t seed = 0x12345678;
    for (size_t i = 0; i < totalSize; ++i) {
        seed = seed * 1664525u + 1013904223u;
        data[i] = static_cast<uint8_t>(seed >> 24);
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_3D, texture);

    glTexImage3D(GL_TEXTURE_3D, 0, GL_R8, size, size, size, 0,
                 GL_RED, GL_UNSIGNED_BYTE, data.data());

    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return texture;
}

GLuint HeightmapCache::getHeightmap(int face, const glm::vec2& minUV, const glm::vec2& maxUV, int level) {
    CacheKey key{face, minUV, maxUV, level};

    auto it = m_cache.find(key);
    if (it != m_cache.end()) {
        it->second.lastUsed = m_frameCounter++;
        return it->second.texture;
    }

    // Generate new heightmap
    GLuint texture = generateHeightmap(face, minUV, maxUV, level);

    CacheEntry entry;
    entry.texture = texture;
    entry.lastUsed = m_frameCounter++;
    m_cache[key] = entry;

    return texture;
}

GLuint HeightmapCache::generateHeightmap(int face, const glm::vec2& minUV, const glm::vec2& maxUV, int level) {
    // For now, we'll use the shared 3D noise texture in the shader
    // This function can be expanded to generate per-node heightmap textures
    // for more detailed terrain at close range

    // Return the shared noise texture
    // The actual height sampling is done in the tessellation evaluation shader
    return m_noiseTexture;
}

void HeightmapCache::clear() {
    for (auto& [key, entry] : m_cache) {
        if (entry.texture && entry.texture != m_noiseTexture) {
            glDeleteTextures(1, &entry.texture);
        }
    }
    m_cache.clear();
}

void HeightmapCache::evictLRU(size_t maxEntries) {
    if (m_cache.size() <= maxEntries) return;

    // Find and evict oldest entries
    std::vector<std::pair<CacheKey, uint64_t>> entries;
    entries.reserve(m_cache.size());

    for (const auto& [key, entry] : m_cache) {
        entries.push_back({key, entry.lastUsed});
    }

    std::sort(entries.begin(), entries.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });

    size_t toEvict = m_cache.size() - maxEntries;
    for (size_t i = 0; i < toEvict && i < entries.size(); ++i) {
        auto it = m_cache.find(entries[i].first);
        if (it != m_cache.end()) {
            if (it->second.texture && it->second.texture != m_noiseTexture) {
                glDeleteTextures(1, &it->second.texture);
            }
            m_cache.erase(it);
        }
    }
}

}  // namespace astrocore
