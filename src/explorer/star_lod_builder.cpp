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
#include <unordered_set>

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

    // ── Detect and fill coverage gaps ───────────────────────────────────
    // Grid space into cells, find empty cells surrounded by populated
    // neighbors, fill with synthetic stars interpolated from neighbors.
    {
        printf("Detecting coverage gaps...\n");
        double tGap = now();

        constexpr float GAP_CELL = 32.0f;  // parsecs per cell
        constexpr int GRID_HALF = 800;     // covers ±25600 pc
        constexpr int GRID_SIZE = GRID_HALF * 2;
        constexpr int MIN_NEIGHBORS = 3;   // need at least 3 populated neighbors to fill

        // Simple 3D grid: flattened to 1D. Each cell stores star count.
        // Grid is too large for full 3D (1600^3 = 4 billion). Use hash map instead.
        struct CellData {
            uint32_t count = 0;
            float avgMag = 0;
            float avgR = 0, avgG = 0, avgB = 0;
        };

        // Key: (ix, iy, iz) packed into int64
        auto packKey = [](int ix, int iy, int iz) -> int64_t {
            return int64_t(ix + GRID_HALF) * int64_t(GRID_SIZE) * GRID_SIZE +
                   int64_t(iy + GRID_HALF) * GRID_SIZE +
                   int64_t(iz + GRID_HALF);
        };

        std::unordered_map<int64_t, CellData> grid;
        grid.reserve(numStars / 50);

        for (uint32_t i = 0; i < numStars; i++) {
            int ix = static_cast<int>(std::floor(stars[i].x / GAP_CELL));
            int iy = static_cast<int>(std::floor(stars[i].y / GAP_CELL));
            int iz = static_cast<int>(std::floor(stars[i].z / GAP_CELL));
            if (std::abs(ix) >= GRID_HALF || std::abs(iy) >= GRID_HALF || std::abs(iz) >= GRID_HALF)
                continue;

            int64_t key = packKey(ix, iy, iz);
            auto& cell = grid[key];
            cell.count++;
            cell.avgMag += stars[i].magnitude;
            cell.avgR += stars[i].r;
            cell.avgG += stars[i].g;
            cell.avgB += stars[i].b;
        }

        // Finalize averages
        for (auto& [key, cell] : grid) {
            if (cell.count > 0) {
                cell.avgMag /= cell.count;
                cell.avgR /= cell.count;
                cell.avgG /= cell.count;
                cell.avgB /= cell.count;
            }
        }

        printf("  Grid built: %zu populated cells (%.1fs)\n", grid.size(), now() - tGap);

        // Find gap cells: empty cells with >= MIN_NEIGHBORS populated neighbors
        std::vector<PackedStar> synthetic;
        uint32_t gapCells = 0;

        // Simple RNG
        uint32_t rng = 0xDEADBEEF;
        auto rngFloat = [&]() -> float {
            rng = rng * 1664525u + 1013904223u;
            return float(rng >> 8) / float(1 << 24);  // [0, 1)
        };

        // Scan all populated cell neighborhoods for gaps
        // For each populated cell, check its 26 neighbors
        std::unordered_set<int64_t> checkedGaps;
        for (auto& [key, cell] : grid) {
            // Decode key back to ix,iy,iz
            int64_t rem = key;
            int iz = static_cast<int>(rem % GRID_SIZE) - GRID_HALF;
            rem /= GRID_SIZE;
            int iy = static_cast<int>(rem % GRID_SIZE) - GRID_HALF;
            rem /= GRID_SIZE;
            int ix = static_cast<int>(rem) - GRID_HALF;

            for (int dx = -1; dx <= 1; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dz = -1; dz <= 1; dz++) {
                        if (dx == 0 && dy == 0 && dz == 0) continue;
                        int nx = ix + dx, ny = iy + dy, nz = iz + dz;
                        if (std::abs(nx) >= GRID_HALF || std::abs(ny) >= GRID_HALF || std::abs(nz) >= GRID_HALF)
                            continue;

                        int64_t nkey = packKey(nx, ny, nz);
                        if (grid.count(nkey) > 0) continue;  // not a gap
                        if (checkedGaps.count(nkey) > 0) continue;  // already processed
                        checkedGaps.insert(nkey);

                        // Count populated neighbors of this gap cell
                        int popNeighbors = 0;
                        float totalMag = 0, totalR = 0, totalG = 0, totalB = 0;
                        uint32_t totalCount = 0;

                        for (int dx2 = -1; dx2 <= 1; dx2++) {
                            for (int dy2 = -1; dy2 <= 1; dy2++) {
                                for (int dz2 = -1; dz2 <= 1; dz2++) {
                                    if (dx2 == 0 && dy2 == 0 && dz2 == 0) continue;
                                    int64_t nnkey = packKey(nx + dx2, ny + dy2, nz + dz2);
                                    auto it = grid.find(nnkey);
                                    if (it != grid.end() && it->second.count > 0) {
                                        popNeighbors++;
                                        totalMag += it->second.avgMag * it->second.count;
                                        totalR += it->second.avgR * it->second.count;
                                        totalG += it->second.avgG * it->second.count;
                                        totalB += it->second.avgB * it->second.count;
                                        totalCount += it->second.count;
                                    }
                                }
                            }
                        }

                        if (popNeighbors < MIN_NEIGHBORS) continue;

                        gapCells++;
                        // Generate synthetic stars for this gap cell
                        float avgMag = totalMag / totalCount;
                        uint8_t avgR = static_cast<uint8_t>(std::clamp(totalR / totalCount, 0.0f, 255.0f));
                        uint8_t avgG = static_cast<uint8_t>(std::clamp(totalG / totalCount, 0.0f, 255.0f));
                        uint8_t avgB = static_cast<uint8_t>(std::clamp(totalB / totalCount, 0.0f, 255.0f));

                        // Average neighbor density per cell
                        uint32_t avgCount = totalCount / popNeighbors;
                        uint32_t fillCount = std::max(1u, avgCount / 2);  // fill at half density

                        float cellX = nx * GAP_CELL;
                        float cellY = ny * GAP_CELL;
                        float cellZ = nz * GAP_CELL;

                        for (uint32_t s = 0; s < fillCount; s++) {
                            PackedStar ps;
                            ps.x = cellX + rngFloat() * GAP_CELL;
                            ps.y = cellY + rngFloat() * GAP_CELL;
                            ps.z = cellZ + rngFloat() * GAP_CELL;
                            // Vary magnitude slightly around average
                            ps.magnitude = avgMag + (rngFloat() - 0.5f) * 2.0f;
                            ps.r = avgR;
                            ps.g = avgG;
                            ps.b = avgB;
                            ps.pad = 0;
                            synthetic.push_back(ps);
                        }
                    }
                }
            }
        }

        printf("  Found %u gap cells, generated %zu synthetic stars (%.1fs)\n",
               gapCells, synthetic.size(), now() - tGap);

        if (!synthetic.empty()) {
            stars.insert(stars.end(), synthetic.begin(), synthetic.end());
            numStars = static_cast<uint32_t>(stars.size());
            printf("  Total stars after gap fill: %u\n", numStars);
        }
    }

    // ── Assign LOD levels ───────────────────────────────────────────────
    // For each level (coarsest first), hash stars into cells and mark
    // the brightest keep_fraction per cell.

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
        // Group by cell key using a hash map
        std::unordered_map<int64_t, std::vector<uint32_t>> cells;
        cells.reserve(numStars / 100);  // rough estimate of unique cells
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
                // Keep all
                for (uint32_t idx : indices) {
                    lodLevel[idx] = static_cast<int8_t>(level);
                }
            } else {
                // Partial sort to find the keepCount brightest (lowest magnitude)
                std::partial_sort(indices.begin(), indices.begin() + keepCount, indices.end(),
                    [&](uint32_t a, uint32_t b) {
                        return stars[a].magnitude < stars[b].magnitude;
                    });
                for (size_t k = 0; k < keepCount; k++) {
                    lodLevel[indices[k]] = static_cast<int8_t>(level);
                }
            }
        }

        // Count
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

    // Parallel sort using sections for each LOD level (counting sort on level, then partial sorts)
    // Simple approach: stable sort by level descending
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

    // Header
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

    // Stars in sorted order (write in chunks for efficiency)
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
