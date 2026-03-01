#pragma once

#include <string>
#include <vector>
#include <memory>
#include <future>
#include "ExoplanetData.hpp"
#include "NasaApiClient.hpp"
#include "ExoMastClient.hpp"
#include "ExoplanetEuClient.hpp"

namespace astrocore {

// Sources that were queried
struct DataSources {
    bool nasa_tap = false;
    bool exomast = false;
    bool exoplanet_eu = false;

    // What we got from each
    int nasa_fields_populated = 0;
    int exomast_detections = 0;
    int eu_fields_populated = 0;
};

// Aggregated exoplanet data with provenance tracking
struct AggregatedExoplanetData {
    ExoplanetData data;
    DataSources sources;

    // Atmospheric detections from ExoMAST (actual observations!)
    std::vector<AtmosphericDetection> atmospheric_detections;
    bool has_observed_atmosphere = false;
    std::string best_spectral_instrument;

    // Conflicts between sources (for UI display)
    std::vector<std::string> data_conflicts;
};

struct AggregatorConfig {
    bool query_nasa = true;
    bool query_exomast = true;       // For atmospheric spectra
    bool query_exoplanet_eu = true;  // For molecules, albedo, measured temps

    // Timeouts
    int nasa_timeout_ms = 10000;
    int exomast_timeout_ms = 5000;
    int eu_timeout_ms = 30000;  // EU catalog download can be slow first time

    // Prefer NASA data when there are conflicts (but EU observations win!)
    bool prefer_nasa_on_conflict = true;
};

class ExoplanetDataAggregator {
public:
    explicit ExoplanetDataAggregator(const AggregatorConfig& config = {});
    ~ExoplanetDataAggregator();

    ExoplanetDataAggregator(const ExoplanetDataAggregator&) = delete;
    ExoplanetDataAggregator& operator=(const ExoplanetDataAggregator&) = delete;

    // Query all configured sources for a planet
    std::future<AggregatedExoplanetData> queryPlanet(const std::string& planetName);
    AggregatedExoplanetData queryPlanetSync(const std::string& planetName);

    // Search across all sources
    std::future<std::vector<AggregatedExoplanetData>> searchPlanets(const std::string& query);

    // Get planets with atmospheric observations (from ExoMAST)
    std::future<std::vector<std::string>> getPlanetsWithAtmosphereData();

    // Quick check if planet exists in any database
    bool planetExists(const std::string& planetName);

    // Get underlying clients for direct access
    NasaApiClient& getNasaClient();
    ExoMastClient& getExoMastClient();
    ExoplanetEuClient& getExoplanetEuClient();

    // Preload Exoplanet.eu catalog (call at startup for faster queries later)
    std::future<bool> preloadExoplanetEuCatalog();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    // Merge atmospheric detections into ExoplanetData for AI prompting
    void mergeAtmosphericData(ExoplanetData& data, const AtmosphericData& atmos);
};

}  // namespace astrocore
