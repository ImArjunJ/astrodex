#include "data/DataFusionEngine.hpp"
#include <algorithm>
#include <limits>

namespace astrocore {

struct DataFusionEngine::Impl {
    NasaApiClient nasa;
    OecClient oec;
    GaiaClient gaia;
    CdsClient cds;
    CacheManager cache{"fused"};
};

DataFusionEngine::DataFusionEngine()
    : m_impl(std::make_unique<Impl>()) {
}

DataFusionEngine::~DataFusionEngine() = default;

int DataFusionEngine::getSourcePriority(DataSource source) {
    switch (source) {
        case DataSource::NASA_TAP:
            return 1;
        case DataSource::GAIA:
            return 2;
        case DataSource::CDS_VIZIER:
        case DataSource::EXOATMOS:
            return 3;
        case DataSource::OEC:
            return 4;
        case DataSource::CALCULATED:
            return 5;
        case DataSource::AI_INFERRED:
            return 6;
        case DataSource::UNKNOWN:
        default:
            return 999;
    }
}

HostStarData DataFusionEngine::mergeHostStarData(const std::vector<HostStarData>& sources) {
    if (sources.empty()) {
        return HostStarData{};
    }

    HostStarData merged;

    // Collect name from first non-empty by source priority
    for (const auto& source : sources) {
        if (!source.name.empty()) {
            merged.name = source.name;
            break;
        }
    }

    // Collect spectral_type from first non-empty
    for (const auto& source : sources) {
        if (!source.spectral_type.empty()) {
            merged.spectral_type = source.spectral_type;
            break;
        }
    }

    // Merge each MeasuredValue field
    std::vector<MeasuredValue<double>> effective_temp_candidates;
    std::vector<MeasuredValue<double>> radius_candidates;
    std::vector<MeasuredValue<double>> mass_candidates;
    std::vector<MeasuredValue<double>> luminosity_candidates;
    std::vector<MeasuredValue<double>> metallicity_candidates;
    std::vector<MeasuredValue<double>> distance_candidates;
    std::vector<MeasuredValue<double>> age_candidates;
    std::vector<MeasuredValue<double>> ra_candidates;
    std::vector<MeasuredValue<double>> dec_candidates;

    for (const auto& source : sources) {
        if (source.effective_temp_k.hasValue()) {
            effective_temp_candidates.push_back(source.effective_temp_k);
        }
        if (source.radius_solar.hasValue()) {
            radius_candidates.push_back(source.radius_solar);
        }
        if (source.mass_solar.hasValue()) {
            mass_candidates.push_back(source.mass_solar);
        }
        if (source.luminosity_solar.hasValue()) {
            luminosity_candidates.push_back(source.luminosity_solar);
        }
        if (source.metallicity.hasValue()) {
            metallicity_candidates.push_back(source.metallicity);
        }
        if (source.distance_pc.hasValue()) {
            distance_candidates.push_back(source.distance_pc);
        }
        if (source.age_gyr.hasValue()) {
            age_candidates.push_back(source.age_gyr);
        }
        if (source.ra_deg.hasValue()) {
            ra_candidates.push_back(source.ra_deg);
        }
        if (source.dec_deg.hasValue()) {
            dec_candidates.push_back(source.dec_deg);
        }
    }

    merged.effective_temp_k = selectBestMeasurement(effective_temp_candidates);
    merged.radius_solar = selectBestMeasurement(radius_candidates);
    merged.mass_solar = selectBestMeasurement(mass_candidates);
    merged.luminosity_solar = selectBestMeasurement(luminosity_candidates);
    merged.metallicity = selectBestMeasurement(metallicity_candidates);
    merged.distance_pc = selectBestMeasurement(distance_candidates);
    merged.age_gyr = selectBestMeasurement(age_candidates);
    merged.ra_deg = selectBestMeasurement(ra_candidates);
    merged.dec_deg = selectBestMeasurement(dec_candidates);

    return merged;
}

ExoplanetData DataFusionEngine::mergeExoplanetData(const std::vector<ExoplanetData>& sources) {
    if (sources.empty()) {
        return ExoplanetData{};
    }

    ExoplanetData merged;

    // Prefer NASA for string fields (highest priority)
    for (const auto& source : sources) {
        if (!source.name.empty() && merged.name.empty()) {
            merged.name = source.name;
        }
        if (!source.discovery_method.empty() && merged.discovery_method.empty()) {
            merged.discovery_method = source.discovery_method;
        }
        if (source.discovery_year > 0 && merged.discovery_year == 0) {
            merged.discovery_year = source.discovery_year;
        }
    }

    // Merge host star data
    std::vector<HostStarData> host_star_sources;
    for (const auto& source : sources) {
        // Only add if it has some data
        if (!source.host_star.name.empty() ||
            source.host_star.effective_temp_k.hasValue() ||
            source.host_star.metallicity.hasValue()) {
            host_star_sources.push_back(source.host_star);
        }
    }
    merged.host_star = mergeHostStarData(host_star_sources);

    // Merge each planet MeasuredValue field
    #define MERGE_FIELD(field_name) \
        do { \
            std::vector<decltype(merged.field_name)> candidates; \
            for (const auto& source : sources) { \
                if (source.field_name.hasValue()) { \
                    candidates.push_back(source.field_name); \
                } \
            } \
            merged.field_name = selectBestMeasurement(candidates); \
        } while(0)

    MERGE_FIELD(orbital_period_days);
    MERGE_FIELD(semi_major_axis_au);
    MERGE_FIELD(eccentricity);
    MERGE_FIELD(inclination_deg);
    MERGE_FIELD(omega_deg);
    MERGE_FIELD(mass_earth);
    MERGE_FIELD(mass_jupiter);
    MERGE_FIELD(radius_earth);
    MERGE_FIELD(radius_jupiter);
    MERGE_FIELD(density_gcc);
    MERGE_FIELD(surface_gravity_g);
    MERGE_FIELD(equilibrium_temp_k);
    MERGE_FIELD(surface_pressure_atm);
    MERGE_FIELD(albedo);
    MERGE_FIELD(greenhouse_effect);
    MERGE_FIELD(habitable_zone_distance);
    MERGE_FIELD(earth_similarity_index);
    MERGE_FIELD(ocean_coverage_fraction);
    MERGE_FIELD(cloud_coverage_fraction);
    MERGE_FIELD(ice_coverage_fraction);

    #undef MERGE_FIELD

    // String MeasuredValue fields
    std::vector<MeasuredValue<std::string>> atmo_candidates;
    std::vector<MeasuredValue<std::string>> planet_type_candidates;
    std::vector<MeasuredValue<std::string>> biome_candidates;
    std::vector<MeasuredValue<std::string>> color_candidates;

    for (const auto& source : sources) {
        if (source.atmosphere_composition.hasValue()) {
            atmo_candidates.push_back(source.atmosphere_composition);
        }
        if (source.planet_type.hasValue()) {
            planet_type_candidates.push_back(source.planet_type);
        }
        if (source.biome_classification.hasValue()) {
            biome_candidates.push_back(source.biome_classification);
        }
        if (source.surface_color_hint.hasValue()) {
            color_candidates.push_back(source.surface_color_hint);
        }
    }

    merged.atmosphere_composition = selectBestMeasurement(atmo_candidates);
    merged.planet_type = selectBestMeasurement(planet_type_candidates);
    merged.biome_classification = selectBestMeasurement(biome_candidates);
    merged.surface_color_hint = selectBestMeasurement(color_candidates);

    // Calculate derived values
    merged.calculateDerivedValues();

    return merged;
}

ExoplanetData DataFusionEngine::fetchAndFuseSync(const std::string& planetName) {
    // Check fused cache first
    auto cached = m_impl->cache.retrieve(planetName);
    if (cached.has_value()) {
        return *cached;
    }

    std::vector<ExoplanetData> sources;

    // Query NASA (primary source)
    try {
        auto nasa_results = m_impl->nasa.queryByNameSync(planetName);
        if (!nasa_results.empty()) {
            sources.push_back(nasa_results[0]);
        }
    } catch (...) {
        // NASA query failed, continue with other sources
    }

    // Query OEC (supplemental)
    try {
        auto oec_results = m_impl->oec.queryByNameSync(planetName);
        if (!oec_results.empty()) {
            sources.push_back(oec_results[0]);
        }
    } catch (...) {
        // OEC query failed
    }

    // Resolve host star name via CDS (for coordinates)
    std::string starName = planetName;
    // Strip planet designation (e.g., "Kepler-442 b" -> "Kepler-442")
    size_t lastSpace = starName.find_last_of(' ');
    if (lastSpace != std::string::npos) {
        starName = starName.substr(0, lastSpace);
    }

    double ra = 0.0, dec = 0.0;
    try {
        auto resolved = m_impl->cds.resolveStarNameSync(starName);
        if (resolved.found) {
            ra = resolved.ra_deg;
            dec = resolved.dec_deg;
        }
    } catch (...) {
        // CDS resolution failed
    }

    // Query Gaia for stellar enrichment (if we have coordinates)
    if (ra > 0.0 || dec > 0.0) {
        try {
            auto gaia_star = m_impl->gaia.queryHostStarByCoordsSync(ra, dec);
            // Add to first source's host_star
            if (!sources.empty() && gaia_star.effective_temp_k.hasValue()) {
                sources[0].host_star = gaia_star;
            }
        } catch (...) {
            // Gaia query failed
        }

        // Query CDS for VizieR enrichment
        try {
            auto cds_star = m_impl->cds.queryHostStarByCoordsSync(ra, dec);
            // Merge with existing host star data
            if (!sources.empty() && cds_star.metallicity.hasValue()) {
                // Add CDS data as a separate host star source for merging
                if (sources.size() > 1) {
                    sources[1].host_star = cds_star;
                }
            }
        } catch (...) {
            // CDS query failed
        }
    }

    // Merge all sources
    ExoplanetData fused = mergeExoplanetData(sources);

    // Store in cache
    m_impl->cache.store(planetName, fused);

    return fused;
}

std::future<ExoplanetData> DataFusionEngine::fetchAndFuse(const std::string& planetName) {
    return std::async(std::launch::async, [this, planetName]() {
        return fetchAndFuseSync(planetName);
    });
}

std::optional<ExoplanetData> DataFusionEngine::getFromCache(const std::string& planetName) {
    return m_impl->cache.retrieve(planetName);
}

std::future<std::vector<ExoplanetData>> DataFusionEngine::prefetchNotable(int count) {
    return std::async(std::launch::async, [this, count]() {
        std::vector<ExoplanetData> results;

        // Get notable exoplanets from NASA
        auto notable = m_impl->nasa.getNotableExoplanets();

        // Limit to requested count
        int to_fetch = std::min(count, static_cast<int>(notable.size()));

        for (int i = 0; i < to_fetch; ++i) {
            try {
                auto fused = fetchAndFuseSync(notable[i].name);
                results.push_back(fused);
            } catch (...) {
                // Skip failed planets
            }
        }

        return results;
    });
}

}  // namespace astrocore
