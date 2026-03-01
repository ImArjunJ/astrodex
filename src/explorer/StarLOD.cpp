#include "explorer/StarLOD.hpp"
#include "core/Logger.hpp"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace astrocore {

StarLOD::StarLOD() = default;

StarLOD::~StarLOD() {
    if (m_mappedData && m_mappedData != MAP_FAILED) {
        munmap(m_mappedData, m_mappedSize);
    }
    if (m_fd >= 0) {
        close(m_fd);
    }
}

bool StarLOD::load(const std::string& binPath) {
    m_fd = open(binPath.c_str(), O_RDONLY);
    if (m_fd < 0) {
        LOG_ERROR("Failed to open LOD file: {}", binPath);
        return false;
    }

    struct stat st;
    if (fstat(m_fd, &st) < 0) {
        LOG_ERROR("Failed to stat LOD file: {}", binPath);
        close(m_fd); m_fd = -1;
        return false;
    }
    m_mappedSize = static_cast<size_t>(st.st_size);

    m_mappedData = mmap(nullptr, m_mappedSize, PROT_READ, MAP_PRIVATE, m_fd, 0);
    if (m_mappedData == MAP_FAILED) {
        LOG_ERROR("Failed to mmap LOD file: {}", binPath);
        close(m_fd); m_fd = -1;
        m_mappedData = nullptr;
        return false;
    }

    if (m_mappedSize < sizeof(LODHeader)) {
        LOG_ERROR("LOD file too small for header");
        return false;
    }

    std::memcpy(&m_header, m_mappedData, sizeof(LODHeader));

    if (std::memcmp(m_header.magic, "GLOD", 4) != 0) {
        LOG_ERROR("Invalid LOD magic bytes");
        return false;
    }
    if (m_header.version != 1) {
        LOG_ERROR("Unsupported LOD version: {}", m_header.version);
        return false;
    }

    size_t expectedSize = sizeof(LODHeader) + size_t(m_header.totalStars) * sizeof(PackedStar);
    if (m_mappedSize < expectedSize) {
        LOG_ERROR("LOD file truncated: {} < {} bytes", m_mappedSize, expectedSize);
        return false;
    }

    auto* base = static_cast<const char*>(m_mappedData);
    m_stars = reinterpret_cast<const PackedStar*>(base + sizeof(LODHeader));

    LOG_INFO("Loaded multi-LOD: {} total stars, {:.2f} GB",
             m_header.totalStars, m_mappedSize / 1e9);
    for (int i = NUM_LOD_LEVELS - 1; i >= 0; i--) {
        LOG_INFO("  LOD{}: stars[0..{}), cellSize={:.0f}pc",
                 i, m_header.levels[i].cumulativeCount, m_header.levels[i].cellSize);
    }

    return true;
}

uint32_t StarLOD::starsAtLevel(int level) const {
    if (level < 0 || level >= NUM_LOD_LEVELS) return 0;
    return m_header.levels[level].cumulativeCount;
}

StarVertex StarLOD::unpackStar(const PackedStar& ps) const {
    StarVertex sv;
    sv.position  = glm::vec3(ps.x, ps.y, ps.z);
    sv.magnitude = ps.magnitude;
    sv.color     = glm::vec3(ps.r / 255.0f, ps.g / 255.0f, ps.b / 255.0f);
    return sv;
}

// ── Distance-shell collection ───────────────────────────────────────────────

void StarLOD::collectVisible(const glm::vec3& cameraPos,
                              const float shellDistances[NUM_LOD_LEVELS],
                              std::vector<StarVertex>& output, int maxPoints) const {
    if (!m_stars || m_header.totalStars == 0) return;
    output.clear();

    // LOD7 (coarsest) = always rendered entirely, no distance check
    uint32_t lod7Count = m_header.levels[NUM_LOD_LEVELS - 1].cumulativeCount;
    output.reserve(std::min(static_cast<int>(lod7Count) + maxPoints / 2, maxPoints));

    for (uint32_t i = 0; i < lod7Count && static_cast<int>(output.size()) < maxPoints; i++) {
        output.push_back(unpackStar(m_stars[i]));
    }

    // LOD6 down to LOD0: each tranche adds stars within its shell distance.
    // Tranche for level L = stars[cumulativeCount[L+1] .. cumulativeCount[L])
    // These are the stars that exist at level L but NOT at level L+1.
    for (int level = NUM_LOD_LEVELS - 2; level >= 0; level--) {
        if (static_cast<int>(output.size()) >= maxPoints) break;

        float maxDist = shellDistances[level];
        float maxDist2 = maxDist * maxDist;

        uint32_t trancheStart = m_header.levels[level + 1].cumulativeCount;
        uint32_t trancheEnd   = m_header.levels[level].cumulativeCount;

        // Scan this tranche, emit stars within distance
        for (uint32_t i = trancheStart; i < trancheEnd; i++) {
            if (static_cast<int>(output.size()) >= maxPoints) break;

            const PackedStar& ps = m_stars[i];
            float dx = ps.x - cameraPos.x;
            float dy = ps.y - cameraPos.y;
            float dz = ps.z - cameraPos.z;
            float dist2 = dx * dx + dy * dy + dz * dz;

            if (dist2 <= maxDist2) {
                output.push_back(unpackStar(ps));
            }
        }
    }
}

// ── Nearest star lookup (brute force on LOD7, fast enough) ──────────────────

StarInfo StarLOD::findNearest(const glm::vec3& pos) const {
    StarInfo info{};
    if (!m_stars || m_header.totalStars == 0) return info;

    float bestDist2 = 1e30f;
    uint32_t bestIdx = 0;

    // Only scan LOD7 (~1.4M stars) for speed
    uint32_t count = m_header.levels[NUM_LOD_LEVELS - 1].cumulativeCount;
    for (uint32_t i = 0; i < count; i++) {
        const PackedStar& ps = m_stars[i];
        float dx = ps.x - pos.x;
        float dy = ps.y - pos.y;
        float dz = ps.z - pos.z;
        float d2 = dx * dx + dy * dy + dz * dz;
        if (d2 < bestDist2) {
            bestDist2 = d2;
            bestIdx = i;
        }
    }

    if (bestDist2 < 1e30f) {
        const PackedStar& ps = m_stars[bestIdx];
        info.position  = glm::vec3(ps.x, ps.y, ps.z);
        info.magnitude = ps.magnitude;
        info.distance  = std::sqrt(bestDist2);
    }
    return info;
}

} // namespace astrocore
