#include "explorer/StarOctree.hpp"
#include "core/Logger.hpp"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <utility>
#include <queue>

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

    // ── Debug: dump root node and its children ──
    if (m_header.numNodes > 0) {
        const OctreeNode& root = m_nodes[0];
        LOG_INFO("ROOT node 0: center=({:.1f},{:.1f},{:.1f}) halfSize={:.1f} totalDesc={} starCount={} aggFlux={:.6f}",
                 root.cx, root.cy, root.cz, root.halfSize,
                 root.totalDescendants, root.starCount, root.aggFlux);
        for (int c = 0; c < 8; c++) {
            int ci = root.children[c];
            if (ci >= 0 && ci < static_cast<int>(m_header.numNodes)) {
                const OctreeNode& child = m_nodes[ci];
                LOG_INFO("  child[{}] = node {}: center=({:.1f},{:.1f},{:.1f}) halfSize={:.1f} totalDesc={} starCount={} aggFlux={:.6f}",
                         c, ci, child.cx, child.cy, child.cz, child.halfSize,
                         child.totalDescendants, child.starCount, child.aggFlux);
            } else {
                LOG_INFO("  child[{}] = {} (empty)", c, ci);
            }
        }

        // Check first few stars for sanity
        if (m_header.numStars > 0) {
            LOG_INFO("First 5 stars:");
            for (uint32_t i = 0; i < std::min(m_header.numStars, 5u); i++) {
                const PackedStar& s = m_stars[i];
                LOG_INFO("  star[{}]: pos=({:.2f},{:.2f},{:.2f}) mag={:.2f} rgb=({},{},{})",
                         i, s.x, s.y, s.z, s.magnitude, s.r, s.g, s.b);
            }
        }

        // Count how many root children have descendants
        int populatedChildren = 0;
        uint32_t totalInChildren = 0;
        for (int c = 0; c < 8; c++) {
            int ci = root.children[c];
            if (ci >= 0 && ci < static_cast<int>(m_header.numNodes)) {
                if (m_nodes[ci].totalDescendants > 0) {
                    populatedChildren++;
                    totalInChildren += m_nodes[ci].totalDescendants;
                }
            }
        }
        LOG_INFO("Root has {}/8 populated children, total descendants in children: {}, root.totalDesc: {}",
                 populatedChildren, totalInChildren, root.totalDescendants);

        // ── VERIFY: check leaf nodes have stars within their bounding box ──
        LOG_INFO("=== Verifying star-to-leaf alignment ===");
        int leavesChecked = 0, leavesOK = 0, leavesBad = 0;
        int starsOutOfBounds = 0, starsNaN = 0;
        for (uint32_t n = 0; n < m_header.numNodes && leavesChecked < 5000; n++) {
            const OctreeNode& nd = m_nodes[n];
            if (nd.starCount == 0) continue; // not a leaf
            leavesChecked++;

            float hs = nd.halfSize;
            // Allow small epsilon for floating point
            float margin = hs * 0.01f + 1.0f;
            bool leafOK = true;

            uint32_t end = std::min(nd.starOffset + nd.starCount, m_header.numStars);
            for (uint32_t si = nd.starOffset; si < end; si++) {
                const PackedStar& s = m_stars[si];

                if (!std::isfinite(s.x) || !std::isfinite(s.y) || !std::isfinite(s.z)) {
                    starsNaN++;
                    leafOK = false;
                    continue;
                }

                float dx = std::abs(s.x - nd.cx);
                float dy = std::abs(s.y - nd.cy);
                float dz = std::abs(s.z - nd.cz);
                if (dx > hs + margin || dy > hs + margin || dz > hs + margin) {
                    starsOutOfBounds++;
                    leafOK = false;
                    if (starsOutOfBounds <= 5) {
                        LOG_ERROR("  OUT-OF-BOUNDS star[{}]: pos=({:.1f},{:.1f},{:.1f}) in leaf node {} "
                                  "center=({:.1f},{:.1f},{:.1f}) hs={:.1f} — offset by ({:.1f},{:.1f},{:.1f})",
                                  si, s.x, s.y, s.z, n,
                                  nd.cx, nd.cy, nd.cz, nd.halfSize,
                                  dx - hs, dy - hs, dz - hs);
                    }
                }
            }
            if (leafOK) leavesOK++;
            else leavesBad++;
        }
        LOG_INFO("  Checked {} leaves: {} OK, {} BAD ({} out-of-bounds stars, {} NaN stars)",
                 leavesChecked, leavesOK, leavesBad, starsOutOfBounds, starsNaN);
        if (starsOutOfBounds > 0) {
            LOG_ERROR("  STAR-LEAF MISALIGNMENT DETECTED — octree binary is corrupt!");
        }
    }

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

    using QEntry = std::pair<float, int>;

    // Auto-adaptive LOD: run a cheap estimation pass first.
    // Count how many points a given LOD would produce, then adjust
    // the threshold until it fits the budget. This ensures ALL nodes at
    // similar distances are treated the same (no inconsistent gaps).
    float effectiveLod = lodThreshold;

    for (int attempt = 0; attempt < 8; attempt++) {
        // Quick estimate: traverse without emitting, just count
        std::priority_queue<QEntry> estPq;
        {
            const OctreeNode& root = m_nodes[0];
            glm::vec3 c(root.cx, root.cy, root.cz);
            float dist = glm::length(cameraPos - c);
            float ang = root.halfSize / std::max(dist, 0.001f);
            estPq.push({ang, 0});
        }

        int estimate = 0;
        while (!estPq.empty() && estimate < maxPoints * 2) { // cap estimation work
            auto [ang, ni] = estPq.top();
            estPq.pop();
            if (ni < 0 || ni >= static_cast<int>(m_header.numNodes)) continue;
            const OctreeNode& nd = m_nodes[ni];
            if (nd.totalDescendants == 0) continue;

            if (ang < effectiveLod) {
                estimate++; // one aggregate point
                continue;
            }
            if (nd.starCount > 0) {
                estimate += nd.starCount + (nd.totalDescendants > 1 ? 1 : 0);
                continue;
            }
            for (int ch = 0; ch < 8; ch++) {
                int ci = nd.children[ch];
                if (ci < 0 || ci >= static_cast<int>(m_header.numNodes)) continue;
                const OctreeNode& child = m_nodes[ci];
                if (child.totalDescendants == 0) continue;
                glm::vec3 cc(child.cx, child.cy, child.cz);
                float d = glm::length(cameraPos - cc);
                float a = child.halfSize / std::max(d, 0.001f);
                estPq.push({a, ci});
            }
        }

        if (estimate <= maxPoints) break; // fits in budget, use this LOD
        // Too many points — increase LOD threshold and retry
        effectiveLod *= 2.0f;
    }

    // Track which nodes are visited (for wireframe debug coloring)
    m_visitedLeaves.assign(m_header.numNodes, false);

    // Main pass: emit actual points with the adapted LOD
    std::priority_queue<QEntry> pq;
    {
        const OctreeNode& root = m_nodes[0];
        glm::vec3 c(root.cx, root.cy, root.cz);
        float dist = glm::length(cameraPos - c);
        float ang = root.halfSize / std::max(dist, 0.001f);
        pq.push({ang, 0});
    }

    while (!pq.empty() && static_cast<int>(output.size()) < maxPoints) {
        auto [angularSize, nodeIdx] = pq.top();
        pq.pop();

        if (nodeIdx < 0 || nodeIdx >= static_cast<int>(m_header.numNodes)) continue;
        const OctreeNode& node = m_nodes[nodeIdx];
        if (node.totalDescendants == 0) continue;

        m_visitedLeaves[nodeIdx] = true;

        if (angularSize < effectiveLod) {
            emitAggregate(node, output);
            continue;
        }

        if (node.starCount > 0) {
            if (node.totalDescendants > 1) {
                emitAggregate(node, output);
            }
            uint32_t end = std::min(node.starOffset + node.starCount, m_header.numStars);
            for (uint32_t i = node.starOffset; i < end; i++) {
                output.push_back(unpackStar(m_stars[i]));
                if (static_cast<int>(output.size()) >= maxPoints) break;
            }
            continue;
        }

        for (int c = 0; c < 8; c++) {
            int ci = node.children[c];
            if (ci < 0 || ci >= static_cast<int>(m_header.numNodes)) continue;
            const OctreeNode& child = m_nodes[ci];
            if (child.totalDescendants == 0) continue;
            glm::vec3 cc(child.cx, child.cy, child.cz);
            float dist = glm::length(cameraPos - cc);
            float ang = child.halfSize / std::max(dist, 0.001f);
            pq.push({ang, ci});
        }
    }

    // Drain remaining as aggregates
    while (!pq.empty()) {
        auto [ang, nodeIdx] = pq.top();
        pq.pop();
        if (nodeIdx >= 0 && nodeIdx < static_cast<int>(m_header.numNodes)) {
            m_visitedLeaves[nodeIdx] = true;
            emitAggregate(m_nodes[nodeIdx], output);
        }
    }
}

void StarOctree::emitAggregate(const OctreeNode& node, std::vector<StarVertex>& output) const {
    if (node.aggFlux <= 0.0f || node.totalDescendants == 0) return;
    StarVertex sv;
    sv.position  = glm::vec3(node.aggX, node.aggY, node.aggZ);
    float perceivedFlux = node.aggFlux / std::sqrt(static_cast<float>(node.totalDescendants));
    sv.magnitude = -2.5f * std::log10(std::max(perceivedFlux, 1e-30f));
    sv.color     = glm::vec3(
        std::clamp(node.aggR / 255.0f, 0.0f, 1.0f),
        std::clamp(node.aggG / 255.0f, 0.0f, 1.0f),
        std::clamp(node.aggB / 255.0f, 0.0f, 1.0f));
    output.push_back(sv);
}

// traverse() is no longer used — keeping declaration satisfied
void StarOctree::traverse(int /*nodeIdx*/, const glm::vec3& /*cameraPos*/, float /*lodThreshold*/,
                           std::vector<StarVertex>& /*output*/, int /*maxPoints*/) const {
}

// ── Debug wireframe ─────────────────────────────────────────────────────────

void StarOctree::collectWireframe(const glm::vec3& pos, float radius,
                                   std::vector<StarVertex>& output, int maxDepthVis) const {
    if (!m_nodes || m_header.numNodes == 0) return;

    // Recursive lambda to walk the tree and emit edge points for nearby nodes
    struct Walker {
        const StarOctree* self;
        const glm::vec3& pos;
        float radius;
        int maxDepth;
        std::vector<StarVertex>& out;
        size_t startSize;
        size_t maxWirePoints;

        void emitBoxEdges(const OctreeNode& node, float r, float g, float b) {
            float hs = node.halfSize;
            glm::vec3 c(node.cx, node.cy, node.cz);
            glm::vec3 lo = c - hs;
            glm::vec3 hi = c + hs;

            // 12 edges, place ~6 points per edge = 72 points per box
            const int N = 6;
            auto addEdge = [&](glm::vec3 a, glm::vec3 b_) {
                for (int i = 0; i <= N; i++) {
                    float t = float(i) / float(N);
                    StarVertex sv;
                    sv.position  = a + (b_ - a) * t;
                    sv.magnitude = -8.0f; // bright
                    sv.color     = glm::vec3(r, g, b);
                    out.push_back(sv);
                }
            };

            // Bottom face (y=lo.y)
            addEdge({lo.x,lo.y,lo.z}, {hi.x,lo.y,lo.z});
            addEdge({hi.x,lo.y,lo.z}, {hi.x,lo.y,hi.z});
            addEdge({hi.x,lo.y,hi.z}, {lo.x,lo.y,hi.z});
            addEdge({lo.x,lo.y,hi.z}, {lo.x,lo.y,lo.z});
            // Top face (y=hi.y)
            addEdge({lo.x,hi.y,lo.z}, {hi.x,hi.y,lo.z});
            addEdge({hi.x,hi.y,lo.z}, {hi.x,hi.y,hi.z});
            addEdge({hi.x,hi.y,hi.z}, {lo.x,hi.y,hi.z});
            addEdge({lo.x,hi.y,hi.z}, {lo.x,hi.y,lo.z});
            // Vertical edges
            addEdge({lo.x,lo.y,lo.z}, {lo.x,hi.y,lo.z});
            addEdge({hi.x,lo.y,lo.z}, {hi.x,hi.y,lo.z});
            addEdge({hi.x,lo.y,hi.z}, {hi.x,hi.y,hi.z});
            addEdge({lo.x,lo.y,hi.z}, {lo.x,hi.y,hi.z});
        }

        void walk(int nodeIdx, int depth) {
            if (nodeIdx < 0 || nodeIdx >= static_cast<int>(self->m_header.numNodes)) return;
            if ((out.size() - startSize) > maxWirePoints) return;

            const OctreeNode& node = self->m_nodes[nodeIdx];

            // Only draw nodes within radius
            glm::vec3 c(node.cx, node.cy, node.cz);
            float dist = glm::length(pos - c) - node.halfSize * 1.732f; // subtract diagonal
            if (dist > radius) return;

            // Color by visit status from last traversal:
            //   Green = leaf visited (stars emitted)
            //   RED = leaf NOT visited (stars missing = GAP!)
            //   Blue = internal at max depth
            bool visited = (nodeIdx >= 0 &&
                           static_cast<size_t>(nodeIdx) < self->m_visitedLeaves.size() &&
                           self->m_visitedLeaves[nodeIdx]);

            if (node.totalDescendants == 0) {
                emitBoxEdges(node, 0.5f, 0.0f, 0.5f); // purple = empty
            } else if (node.starCount > 0) {
                if (visited) {
                    emitBoxEdges(node, 0.0f, 1.0f, 0.0f);  // green = visited leaf
                } else {
                    emitBoxEdges(node, 1.0f, 0.0f, 0.0f);  // RED = unvisited leaf (GAP!)
                }
            } else {
                if (depth >= maxDepth) {
                    if (visited) {
                        emitBoxEdges(node, 0.0f, 0.5f, 1.0f); // blue = visited internal
                    } else {
                        emitBoxEdges(node, 1.0f, 0.5f, 0.0f); // orange = unvisited internal
                    }
                    return;
                }
            }

            // Recurse into children
            if (node.starCount == 0) {
                for (int ch = 0; ch < 8; ch++) {
                    if (node.children[ch] >= 0) {
                        walk(node.children[ch], depth + 1);
                    }
                }
            }
        }
    };

    Walker w{this, pos, radius, maxDepthVis, output, output.size(), 500000};
    w.walk(0, 0);
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
