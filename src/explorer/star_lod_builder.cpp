/**
 * star_lod_builder — Build multi-LOD star binary from raw Gaia data.
 *
 * Reads gaia_raw.bin (from Python), assigns LOD levels per spatial cell
 * using OpenMP parallelism, sorts, and writes gaia_multilod.bin.
 *
 * Usage:
 *   ./build/star_lod_builder assets/starmap/gaia_raw.bin assets/starmap/gaia_multilod.bin
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <vector>
#include <numeric>
#include <chrono>
#include <unordered_map>

#ifdef _OPENMP
#include <omp.h>
#endif

// ── Data structures (must match Python + C++ runtime) ───────────────────────

static constexpr int NUM_LEVELS = 4;

struct PackedStar {
    float x, y, z;
    float magnitude;
    uint8_t r, g, b;
    uint8_t pad;
};
static_assert(sizeof(PackedStar) == 20, "PackedStar must be 20 bytes");

// LOD params: cell_size (pc), keep_fraction
struct LODParam { float cellSize; float keepFrac; };
static constexpr LODParam LOD_PARAMS[NUM_LEVELS] = {
    {0,     1.00f},   // LOD0: full catalog (everything)
    {16,    0.10f},   // LOD1: 10% brightest per 16pc cell
    {64,    0.02f},   // LOD2: 2% per 64pc
    {256,   0.005f},  // LOD3: 0.5% per 256pc (always rendered)
};

// Output header
struct LODHeader {
    char magic[4];           // "GLOD"
    uint32_t version;        // 1
    uint32_t numLevels;      // NUM_LEVELS
    uint32_t totalStars;
    struct {
        uint32_t cumulativeCount;
        float cellSize;
    } levels[8];             // padded to 8 even if fewer levels used
    char padding[48];
};
static_assert(sizeof(LODHeader) == 128, "LODHeader must be 128 bytes");

// ── Timing helper ───────────────────────────────────────────────────────────

static double now() {
    return std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

// ── Spatial cell hashing ────────────────────────────────────────────────────

static int64_t cellHash(float x, float y, float z, float cellSize) {
    int32_t cx = static_cast<int32_t>(std::floor(x / cellSize));
    int32_t cy = static_cast<int32_t>(std::floor(y / cellSize));
    int32_t cz = static_cast<int32_t>(std::floor(z / cellSize));
    return int64_t(cx) * 73856093LL ^ int64_t(cy) * 19349669LL ^ int64_t(cz) * 83492791LL;
}

// ── Main ────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input_raw.bin> <output_multilod.bin>\n", argv[0]);
        return 1;
    }

    const char* inputPath = argv[1];
    const char* outputPath = argv[2];

#ifdef _OPENMP
    printf("OpenMP: %d threads available\n", omp_get_max_threads());
#else
    printf("OpenMP: not available (single-threaded)\n");
#endif

    // ── Read raw input ──────────────────────────────────────────────────
    double t0 = now();

    FILE* fin = fopen(inputPath, "rb");
    if (!fin) { fprintf(stderr, "Failed to open %s\n", inputPath); return 1; }

    char magic[4];
    uint32_t numStars;
    fread(magic, 1, 4, fin);
    fread(&numStars, 4, 1, fin);

    if (memcmp(magic, "GRAW", 4) != 0) {
        fprintf(stderr, "Invalid magic in %s\n", inputPath);
        fclose(fin);
        return 1;
    }

    printf("Reading %u stars from %s...\n", numStars, inputPath);
    std::vector<PackedStar> stars(numStars);
    size_t read = fread(stars.data(), sizeof(PackedStar), numStars, fin);
    fclose(fin);

    if (read != numStars) {
        fprintf(stderr, "Short read: got %zu, expected %u\n", read, numStars);
        return 1;
    }
    printf("  Read in %.1fs\n", now() - t0);

    // ── Assign LOD levels ───────────────────────────────────────────────
    std::vector<int8_t> lodLevel(numStars, 0);  // default: LOD0

    for (int level = NUM_LEVELS - 1; level >= 1; level--) {
        double tLevel = now();
        float cellSize = LOD_PARAMS[level].cellSize;
        float keepFrac = LOD_PARAMS[level].keepFrac;

        printf("  LOD%d: cell=%.0fpc, keep=%.1f%%", level, cellSize, keepFrac * 100);
        fflush(stdout);

        // Step 1: Hash all stars into cells (parallel)
        std::vector<int64_t> cellKeys(numStars);

        #pragma omp parallel for schedule(static)
        for (uint32_t i = 0; i < numStars; i++) {
            cellKeys[i] = cellHash(stars[i].x, stars[i].y, stars[i].z, cellSize);
        }

        // Step 2: Build cell -> star index mapping
        std::unordered_map<int64_t, std::vector<uint32_t>> cells;
        cells.reserve(numStars / 100);
        for (uint32_t i = 0; i < numStars; i++) {
            cells[cellKeys[i]].push_back(i);
        }

        // Step 3: For each cell, find brightest stars (parallel over cells)
        std::vector<std::pair<int64_t, std::vector<uint32_t>*>> cellList;
        cellList.reserve(cells.size());
        for (auto& [key, indices] : cells) {
            cellList.push_back({key, &indices});
        }

        #pragma omp parallel for schedule(dynamic, 1000)
        for (size_t ci = 0; ci < cellList.size(); ci++) {
            auto& indices = *cellList[ci].second;
            size_t cellCount = indices.size();
            size_t keepCount = std::max(size_t(1), size_t(cellCount * keepFrac));

            if (keepCount >= cellCount) {
                for (uint32_t idx : indices) {
                    lodLevel[idx] = static_cast<int8_t>(level);
                }
            } else {
                std::partial_sort(indices.begin(), indices.begin() + keepCount, indices.end(),
                    [&](uint32_t a, uint32_t b) {
                        return stars[a].magnitude < stars[b].magnitude;
                    });
                for (size_t k = 0; k < keepCount; k++) {
                    lodLevel[indices[k]] = static_cast<int8_t>(level);
                }
            }
        }

        uint32_t count = 0;
        for (uint32_t i = 0; i < numStars; i++) {
            if (lodLevel[i] >= level) count++;
        }
        printf(" -> %u stars (%.1fs)\n", count, now() - tLevel);
    }

    // ── Sort by LOD level descending, then magnitude ascending ──────────
    printf("Sorting stars by LOD level...\n");
    double tSort = now();

    std::vector<uint32_t> sortIdx(numStars);
    std::iota(sortIdx.begin(), sortIdx.end(), 0);

    std::stable_sort(sortIdx.begin(), sortIdx.end(), [&](uint32_t a, uint32_t b) {
        if (lodLevel[a] != lodLevel[b]) return lodLevel[a] > lodLevel[b];
        return stars[a].magnitude < stars[b].magnitude;
    });

    printf("  Sorted in %.1fs\n", now() - tSort);

    // ── Compute cumulative counts ───────────────────────────────────────
    uint32_t cumulativeCounts[NUM_LEVELS] = {};
    {
        uint32_t running = 0;
        for (int level = NUM_LEVELS - 1; level >= 0; level--) {
            for (uint32_t i = running; i < numStars; i++) {
                if (lodLevel[sortIdx[i]] >= level) running++;
                else break;
            }
            cumulativeCounts[level] = running;
        }
    }

    printf("Cumulative counts:\n");
    for (int i = NUM_LEVELS - 1; i >= 0; i--) {
        printf("  LOD%d: stars[0..%u)\n", i, cumulativeCounts[i]);
    }

    // ── Write output ────────────────────────────────────────────────────
    printf("Writing %s...\n", outputPath);
    double tWrite = now();

    FILE* fout = fopen(outputPath, "wb");
    if (!fout) { fprintf(stderr, "Failed to open %s for writing\n", outputPath); return 1; }

    LODHeader header{};
    memcpy(header.magic, "GLOD", 4);
    header.version = 1;
    header.numLevels = NUM_LEVELS;
    header.totalStars = numStars;
    for (int i = 0; i < NUM_LEVELS; i++) {
        header.levels[i].cumulativeCount = cumulativeCounts[i];
        header.levels[i].cellSize = LOD_PARAMS[i].cellSize;
    }
    fwrite(&header, sizeof(header), 1, fout);

    static constexpr size_t CHUNK = 1024 * 1024;
    std::vector<PackedStar> writeBuf(CHUNK);
    for (uint32_t i = 0; i < numStars; i += CHUNK) {
        uint32_t end = std::min(i + uint32_t(CHUNK), numStars);
        for (uint32_t j = i; j < end; j++) {
            writeBuf[j - i] = stars[sortIdx[j]];
        }
        fwrite(writeBuf.data(), sizeof(PackedStar), end - i, fout);
    }

    fclose(fout);
    printf("  Written in %.1fs\n", now() - tWrite);
    printf("Total: %.1fs\n", now() - t0);

    return 0;
}
