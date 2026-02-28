#pragma once
#include "data/ExoplanetData.hpp"
#include "data/NasaApiClient.hpp"
#include "data/OecClient.hpp"
#include "data/GaiaClient.hpp"
#include "data/CdsClient.hpp"
#include "data/CacheManager.hpp"
#include "ai/InferenceEngine.hpp"
#include <memory>
#include <future>

namespace astrocore {

class DataFusionEngine {
public:
    DataFusionEngine();
    ~DataFusionEngine();

    // Main entry point: fetch and fuse data for a planet from all sources
    std::future<ExoplanetData> fetchAndFuse(const std::string& planetName);
    ExoplanetData fetchAndFuseSync(const std::string& planetName);

    // Pre-fetch top N planets for offline browsing (per user decision: 500)
    std::future<std::vector<ExoplanetData>> prefetchNotable(int count = 500);

    // Get from cache (offline mode)
    std::optional<ExoplanetData> getFromCache(const std::string& planetName);

    // ---- Static fusion methods (public for unit testing) ----

    // Select best measurement from multiple sources
    template<typename T>
    static MeasuredValue<T> selectBestMeasurement(
        const std::vector<MeasuredValue<T>>& candidates);

    // Get source priority (lower = higher priority)
    // User decision: NASA > Gaia > CDS > OEC
    static int getSourcePriority(DataSource source);

    // Merge host star data from multiple sources
    static HostStarData mergeHostStarData(const std::vector<HostStarData>& sources);

    // Merge complete ExoplanetData from multiple sources
    static ExoplanetData mergeExoplanetData(const std::vector<ExoplanetData>& sources);

    // Apply deterministic rule-based defaults for missing fields
    // Public static for testability; fills only NaN fields with physics-based values
    static void applyDeterministicDefaults(ExoplanetData& data);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// Template implementation must be in header
template<typename T>
MeasuredValue<T> DataFusionEngine::selectBestMeasurement(
    const std::vector<MeasuredValue<T>>& candidates) {

    // Filter to values that actually have data
    std::vector<MeasuredValue<T>> valid;
    for (const auto& candidate : candidates) {
        if (candidate.hasValue()) {
            valid.push_back(candidate);
        }
    }

    if (valid.empty()) {
        return MeasuredValue<T>();  // Return default (NaN for numeric types)
    }

    if (valid.size() == 1) {
        return valid[0];
    }

    // Sort by quality:
    // 1. Has uncertainty beats no uncertainty
    // 2. Lower uncertainty wins
    // 3. Tie-break by source priority
    std::sort(valid.begin(), valid.end(),
        [](const MeasuredValue<T>& a, const MeasuredValue<T>& b) {
            // Prefer values with uncertainty
            bool a_has_unc = a.uncertainty.has_value();
            bool b_has_unc = b.uncertainty.has_value();

            if (a_has_unc != b_has_unc) {
                return a_has_unc > b_has_unc;  // has uncertainty wins
            }

            // Both have uncertainty or both don't
            if (a_has_unc && b_has_unc) {
                // Compare uncertainty values (lower is better)
                if constexpr (std::is_floating_point_v<T>) {
                    if (std::abs(*a.uncertainty - *b.uncertainty) > 1e-9) {
                        return *a.uncertainty < *b.uncertainty;
                    }
                }
            }

            // Tie-break by source priority (lower number = higher priority)
            return getSourcePriority(a.source) < getSourcePriority(b.source);
        });

    return valid[0];  // Return the best one
}

}  // namespace astrocore
