#include "data/DataFusionEngine.hpp"
#include "core/Logger.hpp"
#include <algorithm>
#include <limits>
#include <thread>
#include <chrono>

namespace astrocore {

struct DataFusionEngine::Impl {
    NasaApiClient nasa;
    OecClient oec;
    GaiaClient gaia;
    CdsClient cds;
    CacheManager cache{"fused"};
    InferenceEngine inference;
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

void DataFusionEngine::applyDeterministicDefaults(ExoplanetData& data) {
    double T = data.equilibrium_temp_k.hasValue() ? data.equilibrium_temp_k.value : 288.0;
    double M = data.mass_earth.hasValue() ? data.mass_earth.value : 1.0;
    double R = data.radius_earth.hasValue() ? data.radius_earth.value : 1.0;

    // Albedo: temperature-based default
    if (!data.albedo.hasValue()) {
        if (T > 700.0) {
            data.albedo = MeasuredValue<double>(0.75, DataSource::CALCULATED);  // Venus-like reflective clouds
        } else if (T > 250.0) {
            data.albedo = MeasuredValue<double>(0.3, DataSource::CALCULATED);   // Earth-like
        } else {
            data.albedo = MeasuredValue<double>(0.5, DataSource::CALCULATED);   // Ice world
        }
    }

    // Surface pressure: mass/radius-based (larger rocky planets retain more atmosphere)
    if (!data.surface_pressure_atm.hasValue()) {
        if (R > 6.0) {
            // Gas giant - no defined surface pressure
            data.surface_pressure_atm = MeasuredValue<double>(0.0, DataSource::CALCULATED);
        } else if (M > 5.0) {
            // Super-Earth with thick atmosphere
            data.surface_pressure_atm = MeasuredValue<double>(5.0 * (M / 10.0), DataSource::CALCULATED);
        } else if (M > 0.5) {
            // Earth-like
            data.surface_pressure_atm = MeasuredValue<double>(1.0 * (M / 1.0), DataSource::CALCULATED);
        } else {
            // Small body - thin atmosphere (Mars-like)
            data.surface_pressure_atm = MeasuredValue<double>(0.006 * (M / 0.1), DataSource::CALCULATED);
        }
    }

    // Atmosphere composition: based on temperature and mass
    if (!data.atmosphere_composition.hasValue() || data.atmosphere_composition.value.empty()) {
        if (R > 6.0) {
            // Gas giant: H2/He dominated
            data.atmosphere_composition = MeasuredValue<std::string>(
                R"({"H2":85,"He":14,"CH4":0.5,"NH3":0.5})", DataSource::CALCULATED);
        } else if (T > 700.0) {
            // Hot rocky: CO2/N2 Venus-like
            data.atmosphere_composition = MeasuredValue<std::string>(
                R"({"CO2":96,"N2":3.5,"SO2":0.5})", DataSource::CALCULATED);
        } else if (T > 250.0 && T < 350.0 && M > 0.5 && M < 10.0) {
            // Habitable zone rocky: N2/O2 Earth-like
            data.atmosphere_composition = MeasuredValue<std::string>(
                R"({"N2":78,"O2":21,"Ar":0.9,"CO2":0.04})", DataSource::CALCULATED);
        } else if (T < 250.0) {
            // Cold world: thin N2/CO2
            data.atmosphere_composition = MeasuredValue<std::string>(
                R"({"N2":60,"CO2":30,"Ar":10})", DataSource::CALCULATED);
        } else {
            // Hot super-Earth: CO2/N2
            data.atmosphere_composition = MeasuredValue<std::string>(
                R"({"CO2":70,"N2":25,"H2O":5})", DataSource::CALCULATED);
        }
    }

    // Biome classification: temperature-based
    if (!data.biome_classification.hasValue() || data.biome_classification.value.empty()) {
        if (R > 6.0) {
            data.biome_classification = MeasuredValue<std::string>("Gas Giant", DataSource::CALCULATED);
        } else if (T > 700.0) {
            data.biome_classification = MeasuredValue<std::string>("Lava World", DataSource::CALCULATED);
        } else if (T > 350.0) {
            data.biome_classification = MeasuredValue<std::string>("Desert", DataSource::CALCULATED);
        } else if (T > 250.0) {
            data.biome_classification = MeasuredValue<std::string>("Temperate", DataSource::CALCULATED);
        } else if (T > 150.0) {
            data.biome_classification = MeasuredValue<std::string>("Tundra", DataSource::CALCULATED);
        } else {
            data.biome_classification = MeasuredValue<std::string>("Ice World", DataSource::CALCULATED);
        }
    }

    // Ocean coverage: temperature-driven
    if (!data.ocean_coverage_fraction.hasValue()) {
        if (R > 6.0 || T > 700.0 || T < 150.0) {
            data.ocean_coverage_fraction = MeasuredValue<double>(0.0, DataSource::CALCULATED);
        } else if (T >= 250.0 && T <= 350.0) {
            data.ocean_coverage_fraction = MeasuredValue<double>(0.5, DataSource::CALCULATED);
        } else if (T > 350.0 && T <= 700.0) {
            data.ocean_coverage_fraction = MeasuredValue<double>(0.05, DataSource::CALCULATED);
        } else {
            // 150-250K: some ice coverage, minimal liquid
            data.ocean_coverage_fraction = MeasuredValue<double>(0.1, DataSource::CALCULATED);
        }
    }

    // Cloud coverage: atmosphere-density-driven
    if (!data.cloud_coverage_fraction.hasValue()) {
        double pressure = data.surface_pressure_atm.hasValue() ? data.surface_pressure_atm.value : 1.0;
        if (R > 6.0) {
            data.cloud_coverage_fraction = MeasuredValue<double>(0.8, DataSource::CALCULATED);
        } else if (pressure > 10.0) {
            data.cloud_coverage_fraction = MeasuredValue<double>(0.9, DataSource::CALCULATED);
        } else if (pressure > 0.5) {
            data.cloud_coverage_fraction = MeasuredValue<double>(0.4, DataSource::CALCULATED);
        } else {
            data.cloud_coverage_fraction = MeasuredValue<double>(0.05, DataSource::CALCULATED);
        }
    }

    // Ice coverage: temperature-driven
    if (!data.ice_coverage_fraction.hasValue()) {
        if (T < 150.0) {
            data.ice_coverage_fraction = MeasuredValue<double>(0.9, DataSource::CALCULATED);
        } else if (T < 250.0) {
            data.ice_coverage_fraction = MeasuredValue<double>(0.4, DataSource::CALCULATED);
        } else if (T < 300.0) {
            data.ice_coverage_fraction = MeasuredValue<double>(0.1, DataSource::CALCULATED);
        } else {
            data.ice_coverage_fraction = MeasuredValue<double>(0.0, DataSource::CALCULATED);
        }
    }

    // Surface color hint: biome-based
    if (!data.surface_color_hint.hasValue() || data.surface_color_hint.value.empty()) {
        if (data.biome_classification.hasValue()) {
            const std::string& biome = data.biome_classification.value;
            if (biome == "Lava World") {
                data.surface_color_hint = MeasuredValue<std::string>("dark-red-orange", DataSource::CALCULATED);
            } else if (biome == "Desert") {
                data.surface_color_hint = MeasuredValue<std::string>("rust-orange", DataSource::CALCULATED);
            } else if (biome == "Temperate") {
                data.surface_color_hint = MeasuredValue<std::string>("blue-green", DataSource::CALCULATED);
            } else if (biome == "Tundra") {
                data.surface_color_hint = MeasuredValue<std::string>("grey-white", DataSource::CALCULATED);
            } else if (biome == "Ice World") {
                data.surface_color_hint = MeasuredValue<std::string>("white-ice", DataSource::CALCULATED);
            } else if (biome == "Gas Giant") {
                data.surface_color_hint = MeasuredValue<std::string>("banded-amber", DataSource::CALCULATED);
            }
        }
    }

    // Greenhouse effect: rough estimate from atmosphere
    if (!data.greenhouse_effect.hasValue()) {
        if (T > 700.0) {
            data.greenhouse_effect = MeasuredValue<double>(0.9, DataSource::CALCULATED);
        } else if (T > 300.0) {
            data.greenhouse_effect = MeasuredValue<double>(0.3, DataSource::CALCULATED);
        } else {
            data.greenhouse_effect = MeasuredValue<double>(0.15, DataSource::CALCULATED);
        }
    }
}

ExoplanetData DataFusionEngine::fetchAndFuseSync(const std::string& planetName) {
    // Check fused cache first (includes AI-enriched data)
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
        LOG_WARN("NASA query failed for '{}'", planetName);
    }

    // Query OEC (supplemental)
    try {
        auto oec_results = m_impl->oec.queryByNameSync(planetName);
        if (!oec_results.empty()) {
            sources.push_back(oec_results[0]);
        }
    } catch (...) {
        LOG_WARN("OEC query failed for '{}'", planetName);
    }

    // Resolve host star name via CDS (for coordinates)
    std::string starName = planetName;
    // Strip planet designation (e.g., "Kepler-442 b" -> "Kepler-442")
    size_t lastSpace = starName.find_last_of(' ');
    if (lastSpace != std::string::npos) {
        starName = starName.substr(0, lastSpace);
    }

    double ra = 0.0, dec = 0.0;
    bool coordsResolved = false;
    try {
        auto resolved = m_impl->cds.resolveStarNameSync(starName);
        if (resolved.found) {
            ra = resolved.ra_deg;
            dec = resolved.dec_deg;
            coordsResolved = true;
        }
    } catch (...) {
        LOG_WARN("CDS name resolution failed for '{}'", starName);
    }

    // Query Gaia for stellar enrichment (if we have coordinates)
    if (coordsResolved) {
        try {
            auto gaia_star = m_impl->gaia.queryHostStarByCoordsSync(ra, dec);
            if (gaia_star.effective_temp_k.hasValue()) {
                ExoplanetData gaiaEntry;
                gaiaEntry.host_star = gaia_star;
                sources.push_back(gaiaEntry);
            }
        } catch (...) {
            LOG_WARN("Gaia query failed for coords ({}, {})", ra, dec);
        }

        // Query CDS for VizieR enrichment
        try {
            auto cds_star = m_impl->cds.queryHostStarByCoordsSync(ra, dec);
            if (cds_star.metallicity.hasValue()) {
                ExoplanetData cdsEntry;
                cdsEntry.host_star = cds_star;
                sources.push_back(cdsEntry);
            }
        } catch (...) {
            LOG_WARN("CDS VizieR query failed for coords ({}, {})", ra, dec);
        }
    }

    // Merge all sources
    ExoplanetData fused = mergeExoplanetData(sources);

    // AI inference gap-filling (per user decision: always run after fusion)
    if (m_impl->inference.isAvailable()) {
        try {
            fused = m_impl->inference.fillMissingParametersSync(fused);
        } catch (const std::exception& e) {
            LOG_WARN("AI inference failed for '{}': {}, applying rule-based defaults", planetName, e.what());
            applyDeterministicDefaults(fused);
        }
    } else {
        LOG_INFO("AI unavailable for {}, using rule-based defaults", planetName);
        applyDeterministicDefaults(fused);
    }

    // Store enriched record in cache
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
