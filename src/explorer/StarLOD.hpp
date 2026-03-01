#pragma once

#include "explorer/StarData.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <cstdint>

namespace astrocore {

// Must match gen_star_lod.py exactly
static constexpr int NUM_LOD_LEVELS = 4;

#pragma pack(push, 1)

struct LODHeader {
    char     magic[4];       // "GLOD"
    uint32_t version;        // 1
    uint32_t numLevels;      // 8
    uint32_t totalStars;
    struct {
        uint32_t cumulativeCount;  // stars[0..count) for this LOD
        float    cellSize;
    } levels[8];
    char padding[48];
};
static_assert(sizeof(LODHeader) == 128, "LODHeader must be 128 bytes");

struct PackedStar {
    float    x, y, z;
    float    magnitude;
    uint8_t  r, g, b;
    uint8_t  pad;
};
static_assert(sizeof(PackedStar) == 20, "PackedStar must be 20 bytes");

#pragma pack(pop)

class StarLOD {
public:
    StarLOD();
    ~StarLOD();

    bool load(const std::string& binPath);
    bool isLoaded() const { return m_stars != nullptr; }

    // Distance-shell based star collection.
    // shellDistances[0..7] define max distance for each LOD level (7=coarsest=farthest).
    void collectVisible(const glm::vec3& cameraPos,
                        const float shellDistances[NUM_LOD_LEVELS],
                        std::vector<StarVertex>& output, int maxPoints) const;

    StarInfo findNearest(const glm::vec3& pos) const;

    uint32_t totalStars() const { return m_header.totalStars; }
    uint32_t starsAtLevel(int level) const;

private:
    StarVertex unpackStar(const PackedStar& ps) const;

    LODHeader         m_header{};
    const PackedStar* m_stars = nullptr;

    // Memory-mapped file
    void*   m_mappedData = nullptr;
    size_t  m_mappedSize = 0;
    int     m_fd = -1;
};

} // namespace astrocore
