#include "data/ExoplanetDataAggregator.hpp"
#include "core/Logger.hpp"
#include <algorithm>

namespace astrocore {

struct ExoplanetDataAggregator::Impl {
    AggregatorConfig config;
    std::unique_ptr<NasaApiClient> nasaClient;
    std::unique_ptr<ExoMastClient> exomastClient;
    std::unique_ptr<ExoplanetEuClient> euClient;
};

ExoplanetDataAggregator::ExoplanetDataAggregator(const AggregatorConfig& config)
    : m_impl(std::make_unique<Impl>())
{
    m_impl->config = config;

    // Initialize clients
    if (config.query_nasa) {
        NasaApiConfig nasaConfig;
        nasaConfig.timeout_seconds = config.nasa_timeout_ms / 1000;
        m_impl->nasaClient = std::make_unique<NasaApiClient>(nasaConfig);
    }

    if (config.query_exomast) {
        ExoMastConfig exomastConfig;
        exomastConfig.timeout_seconds = config.exomast_timeout_ms / 1000;
        m_impl->exomastClient = std::make_unique<ExoMastClient>(exomastConfig);
    }

    if (config.query_exoplanet_eu) {
        ExoplanetEuConfig euConfig;
        euConfig.timeout_seconds = config.eu_timeout_ms / 1000;
        m_impl->euClient = std::make_unique<ExoplanetEuClient>(euConfig);
    }

    LOG_INFO("ExoplanetDataAggregator initialized (NASA: {}, ExoMAST: {}, Exoplanet.eu: {})",
             config.query_nasa ? "enabled" : "disabled",
             config.query_exomast ? "enabled" : "disabled",
             config.query_exoplanet_eu ? "enabled" : "disabled");
}

ExoplanetDataAggregator::~ExoplanetDataAggregator() = default;

std::future<bool> ExoplanetDataAggregator::preloadExoplanetEuCatalog() {
    if (m_impl->config.query_exoplanet_eu && m_impl->euClient) {
        return m_impl->euClient->downloadCatalog();
    }
    return std::async(std::launch::async, []() { return false; });
}

std::future<AggregatedExoplanetData> ExoplanetDataAggregator::queryPlanet(const std::string& planetName) {
    return std::async(std::launch::async, [this, planetName]() {
        return queryPlanetSync(planetName);
    });
}

AggregatedExoplanetData ExoplanetDataAggregator::queryPlanetSync(const std::string& planetName) {
    AggregatedExoplanetData result;
    result.data.name = planetName;

    LOG_INFO("Aggregating data for {} from multiple sources...", planetName);

    // 1. Query NASA TAP API (primary source for basic properties)
    if (m_impl->config.query_nasa && m_impl->nasaClient) {
        LOG_DEBUG("Querying NASA TAP for {}...", planetName);
        auto nasaResults = m_impl->nasaClient->queryByNameSync(planetName);

        if (!nasaResults.empty()) {
            // Find exact match if possible
            for (const auto& planet : nasaResults) {
                if (planet.name == planetName) {
                    result.data = planet;
                    result.sources.nasa_tap = true;
                    break;
                }
            }
            // If no exact match, use first result
            if (!result.sources.nasa_tap && !nasaResults.empty()) {
                result.data = nasaResults[0];
                result.sources.nasa_tap = true;
            }

            // Count populated fields
            if (result.data.mass_earth.hasValue()) result.sources.nasa_fields_populated++;
            if (result.data.radius_earth.hasValue()) result.sources.nasa_fields_populated++;
            if (result.data.equilibrium_temp_k.hasValue()) result.sources.nasa_fields_populated++;
            if (result.data.density_gcc.hasValue()) result.sources.nasa_fields_populated++;
            if (result.data.orbital_period_days.hasValue()) result.sources.nasa_fields_populated++;
            if (result.data.semi_major_axis_au.hasValue()) result.sources.nasa_fields_populated++;
            if (result.data.insolation_flux.hasValue()) result.sources.nasa_fields_populated++;
            if (result.data.host_star.effective_temp_k.hasValue()) result.sources.nasa_fields_populated++;

            LOG_INFO("NASA TAP: Found {} with {} data fields", planetName, result.sources.nasa_fields_populated);
        } else {
            LOG_WARN("NASA TAP: No results for {}", planetName);
        }
    }

    // 2. Query Exoplanet.eu for molecules, albedo, measured temperature (CRITICAL FOR COLOR!)
    if (m_impl->config.query_exoplanet_eu && m_impl->euClient) {
        LOG_DEBUG("Querying Exoplanet.eu for {}...", planetName);
        auto euData = m_impl->euClient->queryByNameSync(planetName);

        if (euData.has_value()) {
            result.sources.exoplanet_eu = true;

            // Merge EU data into result (this overwrites with observed data where available)
            m_impl->euClient->mergeIntoExoplanetData(result.data, *euData);

            // Count what we got
            if (euData->molecules_detected.has_value()) {
                result.sources.eu_fields_populated += 10;  // High value for molecules!
                result.has_observed_atmosphere = true;
                LOG_INFO("Exoplanet.eu: DETECTED MOLECULES: {}", *euData->molecules_detected);
            }
            if (euData->geometric_albedo.has_value()) {
                result.sources.eu_fields_populated += 5;  // High value for albedo!
                LOG_INFO("Exoplanet.eu: MEASURED ALBEDO: {:.3f}", *euData->geometric_albedo);
            }
            if (euData->temp_measured_k.has_value()) {
                result.sources.eu_fields_populated += 5;  // High value for measured temp!
                LOG_INFO("Exoplanet.eu: MEASURED TEMP: {:.0f} K", *euData->temp_measured_k);
            }
        } else {
            LOG_DEBUG("Exoplanet.eu: No data for {}", planetName);
        }
    }

    // 3. Query ExoMAST for additional atmospheric spectra data
    if (m_impl->config.query_exomast && m_impl->exomastClient) {
        LOG_DEBUG("Querying ExoMAST for {} atmospheric data...", planetName);
        auto atmosData = m_impl->exomastClient->queryAtmosphereSync(planetName);

        if (atmosData.has_value() && !atmosData->detections.empty()) {
            result.sources.exomast = true;
            result.atmospheric_detections = atmosData->detections;
            result.has_observed_atmosphere = true;
            result.best_spectral_instrument = atmosData->best_instrument;
            result.sources.exomast_detections = static_cast<int>(atmosData->detections.size());

            // Merge atmospheric data into the main ExoplanetData structure
            mergeAtmosphericData(result.data, *atmosData);

            // Log what molecules were detected
            std::string molecules;
            for (const auto& det : atmosData->detections) {
                if (!molecules.empty()) molecules += ", ";
                molecules += det.molecule;
            }
            LOG_INFO("ExoMAST: DETECTED MOLECULES for {}: {} (via {})",
                     planetName, molecules, result.best_spectral_instrument);
        } else {
            LOG_DEBUG("ExoMAST: No atmospheric spectra for {}", planetName);
        }
    }

    // Log summary
    LOG_INFO("Data aggregation complete for {}: NASA={}, Exoplanet.eu={}, ExoMAST={}",
             planetName,
             result.sources.nasa_tap ? "yes" : "no",
             result.sources.exoplanet_eu ? "yes" : "no",
             result.sources.exomast ? "yes" : "no");

    if (result.has_observed_atmosphere) {
        LOG_INFO("  -> HAS OBSERVED ATMOSPHERE DATA (critical for accurate coloring!)");
    }

    return result;
}

void ExoplanetDataAggregator::mergeAtmosphericData(ExoplanetData& data, const AtmosphericData& atmos) {
    // Build atmosphere composition string from detections
    // Only use if we don't already have molecule data from Exoplanet.eu
    if (!atmos.detections.empty() && data.atmosphere_composition.source != DataSource::EXOATMOS) {
        std::string composition;
        for (size_t i = 0; i < atmos.detections.size(); ++i) {
            const auto& det = atmos.detections[i];
            if (i > 0) composition += ", ";
            composition += det.molecule;
        }

        data.atmosphere_composition.value = composition;
        data.atmosphere_composition.source = DataSource::EXOATMOS;
        data.atmosphere_composition.confidence = 0.95f;  // High confidence - actual detection!
    }

    // Use measured temperatures if available
    if (atmos.day_side_temp_k.has_value()) {
        // Day-side temp is a real measurement, very valuable
        data.equilibrium_temp_k.ai_reasoning = "Day-side temperature measured via emission spectroscopy";
    }
}

std::future<std::vector<AggregatedExoplanetData>> ExoplanetDataAggregator::searchPlanets(const std::string& query) {
    return std::async(std::launch::async, [this, query]() {
        std::vector<AggregatedExoplanetData> results;

        if (m_impl->config.query_nasa && m_impl->nasaClient) {
            auto nasaResults = m_impl->nasaClient->queryByNameSync(query);

            for (const auto& planet : nasaResults) {
                // Get full aggregated data for each result
                results.push_back(queryPlanetSync(planet.name));
            }
        }

        return results;
    });
}

std::future<std::vector<std::string>> ExoplanetDataAggregator::getPlanetsWithAtmosphereData() {
    if (m_impl->config.query_exomast && m_impl->exomastClient) {
        return m_impl->exomastClient->getPlanetsWithSpectra();
    }
    return std::async(std::launch::async, []() {
        return std::vector<std::string>{};
    });
}

bool ExoplanetDataAggregator::planetExists(const std::string& planetName) {
    if (m_impl->config.query_nasa && m_impl->nasaClient) {
        auto results = m_impl->nasaClient->queryByNameSync(planetName);
        return !results.empty();
    }
    return false;
}

NasaApiClient& ExoplanetDataAggregator::getNasaClient() {
    return *m_impl->nasaClient;
}

ExoMastClient& ExoplanetDataAggregator::getExoMastClient() {
    return *m_impl->exomastClient;
}

ExoplanetEuClient& ExoplanetDataAggregator::getExoplanetEuClient() {
    return *m_impl->euClient;
}

}  // namespace astrocore
