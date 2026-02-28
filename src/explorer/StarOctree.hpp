#pragma once

#include "explorer/StarData.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <cstdint>

namespace astrocore {

// ── Binary file structures (must match gen_star_octree.py exactly) ───────────

#pragma pack(push, 1)

struct OctreeHeader {
    char     magic[4];       // "GAIA"
    uint32_t version;        // 1
    uint32_t numStars;
    uint32_t numNodes;
    float    rootHalfSize;
    float    rootCx, rootCy, rootCz;
    char     padding[32];
};
static_assert(sizeof(OctreeHeader) == 64, "Header must be 64 bytes");

struct PackedStar {
    float    x, y, z;
    float    magnitude;
    uint8_t  r, g, b;
    uint8_t  pad;
};
static_assert(sizeof(PackedStar) == 20, "PackedStar must be 20 bytes");

struct OctreeNode {
    float    cx, cy, cz;
    float    halfSize;
    int32_t  children[8];
    uint32_t starOffset;
    uint32_t starCount;
    float    aggX, aggY, aggZ;
    float    aggFlux;
    float    aggR, aggG, aggB;
    float    padFloat;
    uint32_t totalDescendants;
    uint32_t padInt;
};
static_assert(sizeof(OctreeNode) == 96, "OctreeNode must be 96 bytes");

#pragma pack(pop)

// ── Octree runtime ──────────────────────────────────────────────────────────

class StarOctree {
public:
    StarOctree();
    ~StarOctree();

    bool load(const std::string& binPath);
    bool isLoaded() const { return m_nodes != nullptr; }

    // LOD traversal — fills output with visible stars for this frame
    void collectVisible(const glm::vec3& cameraPos, float lodThreshold,
                        std::vector<StarVertex>& output, int maxPoints) const;

    // Spatial nearest-star lookup (O(log n))
    StarInfo findNearest(const glm::vec3& pos) const;

    uint32_t totalStars() const { return m_header.numStars; }
    uint32_t totalNodes() const { return m_header.numNodes; }

private:
    void emitAggregate(const OctreeNode& node, std::vector<StarVertex>& output) const;

    void traverse(int nodeIdx, const glm::vec3& cameraPos, float lodThreshold,
                  std::vector<StarVertex>& output, int maxPoints) const;

    void searchNearest(int nodeIdx, const glm::vec3& pos,
                       float& bestDist2, int& bestStarIdx) const;

    StarVertex unpackStar(const PackedStar& ps) const;

    OctreeHeader       m_header{};
    const PackedStar*  m_stars = nullptr;
    const OctreeNode*  m_nodes = nullptr;

    // Memory-mapped file
    void*   m_mappedData = nullptr;
    size_t  m_mappedSize = 0;
    int     m_fd = -1;
};

} // namespace astrocore
