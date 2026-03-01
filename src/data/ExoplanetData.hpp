#pragma once

#include <string>
#include <optional>
#include <cmath>
#include <nlohmann/json.hpp>

namespace astrocore {

// Data source tracking for visualization color-coding
enum class DataSource {
    NASA_TAP,       // NASA Exoplanet Archive (ground truth - white)
    EXOATMOS,       // ExoAtmospheres database (ground truth - white)
    AI_INFERRED,    // Claude Bedrock inference (cyan)
    CALCULATED,     // Derived from other values
    UNKNOWN
};

// Template for tracking measured values with uncertainty and provenance
template<typename T>
struct MeasuredValue {
    T value{};
    std::optional<T> uncertainty;
    DataSource source = DataSource::UNKNOWN;
    std::optional<std::string> ai_reasoning;  // For AI-inferred values
    float confidence = 1.0f;  // 0-1 confidence level

    bool hasValue() const {
        if constexpr (std::is_floating_point_v<T>) {
            return !std::isnan(value);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return !value.empty();
        } else {
            return true;
        }
    }

    bool isAIInferred() const {
        return source == DataSource::AI_INFERRED;
    }

    // Initialize with NaN for floating point
    MeasuredValue() {
        if constexpr (std::is_floating_point_v<T>) {
            value = std::numeric_limits<T>::quiet_NaN();
        }
    }

    explicit MeasuredValue(T val, DataSource src = DataSource::UNKNOWN)
        : value(val), source(src) {}
};

// Host star data
struct HostStarData {
    std::string name;
    std::string gaia_dr3_id;                    // Gaia DR3 source ID for cross-matching
    MeasuredValue<double> effective_temp_k;     // Stellar effective temperature
    MeasuredValue<double> radius_solar;         // Stellar radius in solar radii
    MeasuredValue<double> mass_solar;           // Stellar mass in solar masses
    MeasuredValue<double> luminosity_solar;     // Stellar luminosity
    MeasuredValue<double> metallicity;          // [Fe/H]
    std::string spectral_type;                  // e.g., "G2V", "M4"
    MeasuredValue<double> distance_pc;          // Distance in parsecs
    MeasuredValue<double> age_gyr;              // Age in billion years
    MeasuredValue<double> rotation_period_days; // Stellar rotation period (activity indicator)
    MeasuredValue<double> surface_gravity_logg; // Stellar surface gravity (log g)
};

// Main exoplanet data structure
struct ExoplanetData {
    std::string name;
    std::string discovery_method;
    int discovery_year = 0;

    // Host star
    HostStarData host_star;

    // Position in sky (for galaxy visualization)
    MeasuredValue<double> ra_hours;              // Right ascension in hours (0-24)
    MeasuredValue<double> dec_degrees;           // Declination in degrees (-90 to +90)
    MeasuredValue<double> distance_ly;           // Distance from Earth in light years

    // Orbital parameters
    MeasuredValue<double> orbital_period_days;
    MeasuredValue<double> semi_major_axis_au;
    MeasuredValue<double> eccentricity;
    MeasuredValue<double> inclination_deg;
    MeasuredValue<double> omega_deg;            // Argument of periastron

    // Physical parameters
    MeasuredValue<double> mass_earth;           // Planet mass in Earth masses
    MeasuredValue<double> mass_jupiter;         // Planet mass in Jupiter masses
    MeasuredValue<double> radius_earth;         // Planet radius in Earth radii
    MeasuredValue<double> radius_jupiter;       // Planet radius in Jupiter radii
    MeasuredValue<double> density_gcc;          // Bulk density g/cm³
    MeasuredValue<double> surface_gravity_g;    // Surface gravity in Earth g
    MeasuredValue<double> equilibrium_temp_k;   // Equilibrium temperature
    MeasuredValue<double> insolation_flux;      // Stellar flux received (Earth = 1.0)

    // Transit/observation parameters (atmosphere indicators)
    MeasuredValue<double> transit_depth;        // Transit depth (fraction of stellar light blocked)
    MeasuredValue<double> transit_duration_hr;  // Transit duration in hours
    MeasuredValue<double> tsm;                  // Transmission Spectroscopy Metric (atmosphere observability)
    MeasuredValue<double> esm;                  // Emission Spectroscopy Metric

    // Atmospheric parameters (often AI-inferred)
    MeasuredValue<double> surface_pressure_atm;
    MeasuredValue<double> albedo;
    MeasuredValue<double> greenhouse_effect;
    MeasuredValue<std::string> atmosphere_composition;  // JSON string of gas percentages

    // Habitability indicators (often AI-inferred)
    MeasuredValue<double> habitable_zone_distance;  // Fraction of HZ (0.5-1.5 is in HZ)
    MeasuredValue<double> earth_similarity_index;
    MeasuredValue<std::string> planet_type;     // "Rocky", "Gas Giant", "Ice Giant", "Super-Earth"

    // Rendering parameters (AI-inferred for visualization)
    MeasuredValue<double> ocean_coverage_fraction;
    MeasuredValue<double> cloud_coverage_fraction;
    MeasuredValue<double> ice_coverage_fraction;
    MeasuredValue<std::string> biome_classification;
    MeasuredValue<std::string> surface_color_hint;  // Dominant color description

    // Calculate derived values
    void calculateDerivedValues();

    // Check if planet has sufficient data for visualization
    bool hasMinimumRenderData() const;

    // Serialize to JSON
    nlohmann::json toJson() const;

    // Deserialize from JSON
    static ExoplanetData fromJson(const nlohmann::json& j);
};

// Helper to convert DataSource to string
inline std::string dataSourceToString(DataSource source) {
    switch (source) {
        case DataSource::NASA_TAP: return "NASA";
        case DataSource::EXOATMOS: return "ExoAtmos";
        case DataSource::AI_INFERRED: return "AI";
        case DataSource::CALCULATED: return "Calculated";
        default: return "Unknown";
    }
}

}  // namespace astrocore
