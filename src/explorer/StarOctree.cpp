#include "explorer/StarOctree.hpp"
#include "core/Logger.hpp"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace astrocore {

StarOctree::StarOctree() = default;

StarOctree::~StarOctree() {
    if (m_mappedData && m_mappedData != MAP_FAILED) {
        munmap(m_mappedData, m_mappedSize);
    }
    if (m_fd >= 0) {
        close(m_fd);
    }
}

bool StarOctree::load(const std::string& binPath) {
    m_fd = open(binPath.c_str(), O_RDONLY);
    if (m_fd < 0) {
        LOG_ERROR("Failed to open octree file: {}", binPath);
        return false;
    }

    struct stat st;
    if (fstat(m_fd, &st) < 0) {
        LOG_ERROR("Failed to stat octree file: {}", binPath);
        close(m_fd); m_fd = -1;
        return false;
    }
    m_mappedSize = static_cast<size_t>(st.st_size);

    m_mappedData = mmap(nullptr, m_mappedSize, PROT_READ, MAP_PRIVATE, m_fd, 0);
    if (m_mappedData == MAP_FAILED) {
        LOG_ERROR("Failed to mmap octree file: {}", binPath);
        close(m_fd); m_fd = -1;
        m_mappedData = nullptr;
        return false;
    }

    // Parse header
    if (m_mappedSize < sizeof(OctreeHeader)) {
        LOG_ERROR("Octree file too small for header");
        return false;
    }

    std::memcpy(&m_header, m_mappedData, sizeof(OctreeHeader));

    if (std::memcmp(m_header.magic, "GAIA", 4) != 0) {
        LOG_ERROR("Invalid octree magic bytes");
        return false;
    }
    if (m_header.version != 1) {
        LOG_ERROR("Unsupported octree version: {}", m_header.version);
        return false;
    }

    // Validate sizes
    size_t expectedSize = sizeof(OctreeHeader) +
                          size_t(m_header.numStars) * sizeof(PackedStar) +
                          size_t(m_header.numNodes) * sizeof(OctreeNode);
    if (m_mappedSize < expectedSize) {
        LOG_ERROR("Octree file truncated: {} < {} bytes", m_mappedSize, expectedSize);
        return false;
    }

    // Set up pointers into mapped memory
    auto* base = static_cast<const char*>(m_mappedData);
    m_stars = reinterpret_cast<const PackedStar*>(base + sizeof(OctreeHeader));
    m_nodes = reinterpret_cast<const OctreeNode*>(base + sizeof(OctreeHeader) +
                                                   size_t(m_header.numStars) * sizeof(PackedStar));

    LOG_INFO("Loaded octree: {:L} stars, {:L} nodes, {:.2f} GB",
             m_header.numStars, m_header.numNodes,
             m_mappedSize / 1e9);
    return true;
}

StarVertex StarOctree::unpackStar(const PackedStar& ps) const {
    StarVertex sv;
    sv.position  = glm::vec3(ps.x, ps.y, ps.z);
    sv.magnitude = ps.magnitude;
    sv.color     = glm::vec3(ps.r / 255.0f, ps.g / 255.0f, ps.b / 255.0f);
    return sv;
}

// ── LOD traversal ───────────────────────────────────────────────────────────

void StarOctree::collectVisible(const glm::vec3& cameraPos, float lodThreshold,
                                 std::vector<StarVertex>& output, int maxPoints) const {
    if (!m_nodes || m_header.numNodes == 0) return;
    output.clear();
    output.reserve(std::min(maxPoints, static_cast<int>(m_header.numStars)));
    traverse(0, cameraPos, lodThreshold, output, maxPoints);
}

void StarOctree::emitAggregate(const OctreeNode& node, std::vector<StarVertex>& output) const {
    if (node.aggFlux <= 0.0f || node.totalDescendants == 0) return;
    StarVertex sv;
    sv.position  = glm::vec3(node.aggX, node.aggY, node.aggZ);
    // sqrt(N) normalization: clusters appear brighter than avg star but not N× brighter
    float perceivedFlux = node.aggFlux / std::sqrt(static_cast<float>(node.totalDescendants));
    sv.magnitude = -2.5f * std::log10(std::max(perceivedFlux, 1e-30f));
    sv.color     = glm::vec3(
        std::clamp(node.aggR / 255.0f, 0.0f, 1.0f),
        std::clamp(node.aggG / 255.0f, 0.0f, 1.0f),
        std::clamp(node.aggB / 255.0f, 0.0f, 1.0f));
    output.push_back(sv);
}

void StarOctree::traverse(int nodeIdx, const glm::vec3& cameraPos, float lodThreshold,
                           std::vector<StarVertex>& output, int maxPoints) const {
    if (nodeIdx < 0 || nodeIdx >= static_cast<int>(m_header.numNodes)) return;

    const OctreeNode& node = m_nodes[nodeIdx];
    if (node.totalDescendants == 0) return;

    // If budget exceeded, still render this node as aggregate (no black holes)
    if (static_cast<int>(output.size()) >= maxPoints) {
        emitAggregate(node, output);
        return;
    }

    glm::vec3 center(node.cx, node.cy, node.cz);
    float dist = glm::length(cameraPos - center);
    float angularSize = node.halfSize / std::max(dist, 0.001f);

    // LOD: if node appears small on screen, render as single aggregate point
    if (angularSize < lodThreshold) {
        emitAggregate(node, output);
        return;
    }

    // Leaf node — emit individual stars, but also emit the aggregate so
    // clusters of very dim stars don't vanish when expanded
    if (node.starCount > 0) {
        // Always emit the aggregate as a "base glow" for the cluster
        if (node.totalDescendants > 1) {
            emitAggregate(node, output);
        }
        uint32_t end = std::min(node.starOffset + node.starCount, m_header.numStars);
        for (uint32_t i = node.starOffset; i < end; i++) {
            output.push_back(unpackStar(m_stars[i]));
            if (static_cast<int>(output.size()) >= maxPoints) return;
        }
        return;
    }

    // Internal node — emit aggregate as base glow, then recurse into children.
    // The aggregate ensures this region is never blank even if children get
    // budget-capped or contain only invisible dim stars.
    emitAggregate(node, output);

    for (int c = 0; c < 8; c++) {
        if (node.children[c] >= 0) {
            traverse(node.children[c], cameraPos, lodThreshold, output, maxPoints);
        }
    }
}

// ── Spatial nearest lookup ──────────────────────────────────────────────────

StarInfo StarOctree::findNearest(const glm::vec3& pos) const {
    StarInfo info{};
    if (!m_nodes || m_header.numStars == 0) return info;

    float bestDist2 = 1e30f;
    int bestIdx = -1;
    searchNearest(0, pos, bestDist2, bestIdx);

    if (bestIdx >= 0 && bestIdx < static_cast<int>(m_header.numStars)) {
        const PackedStar& ps = m_stars[bestIdx];
        info.position  = glm::vec3(ps.x, ps.y, ps.z);
        info.magnitude = ps.magnitude;
        info.distance  = std::sqrt(bestDist2);
        // No name/spectral/constellation in Gaia binary
    }
    return info;
}

void StarOctree::searchNearest(int nodeIdx, const glm::vec3& pos,
                                float& bestDist2, int& bestStarIdx) const {
    if (nodeIdx < 0 || nodeIdx >= static_cast<int>(m_header.numNodes)) return;

    const OctreeNode& node = m_nodes[nodeIdx];
    if (node.totalDescendants == 0) return;

    // Prune: if the closest point on the node's bounding box is farther
    // than current best, skip entirely
    glm::vec3 center(node.cx, node.cy, node.cz);
    float hs = node.halfSize;
    glm::vec3 closest = glm::clamp(pos, center - hs, center + hs);
    float boxDist2 = glm::dot(closest - pos, closest - pos);
    if (boxDist2 > bestDist2) return;

    // Leaf — check individual stars
    if (node.starCount > 0) {
        uint32_t end = std::min(node.starOffset + node.starCount, m_header.numStars);
        for (uint32_t i = node.starOffset; i < end; i++) {
            glm::vec3 sp(m_stars[i].x, m_stars[i].y, m_stars[i].z);
            float d2 = glm::dot(sp - pos, sp - pos);
            if (d2 < bestDist2) {
                bestDist2 = d2;
                bestStarIdx = static_cast<int>(i);
            }
        }
        return;
    }

    // Internal — recurse children (could sort by distance for better pruning,
    // but for findNearest called infrequently this is fine)
    for (int c = 0; c < 8; c++) {
        if (node.children[c] >= 0) {
            searchNearest(node.children[c], pos, bestDist2, bestStarIdx);
        }
    }
}

} // namespace astrocore
