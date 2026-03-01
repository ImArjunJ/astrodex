#pragma once

#include <string>
#include <vector>
#include <future>
#include <optional>
#include <memory>
#include "ExoplanetData.hpp"

namespace astrocore {

// Additional fields from exoplanet.eu that NASA may not have
struct ExoplanetEuData {
    std::string name;

    // Detection info
    std::string detection_type;         // "Primary Transit", "Radial Velocity", "Imaging", etc.
    std::optional<int> num_planets_in_system;

    // Physical properties (may differ from NASA)
    std::optional<double> mass_earth;
    std::optional<double> radius_earth;
    std::optional<double> density_gcc;
    std::optional<double> temp_calculated_k;
    std::optional<double> temp_measured_k;  // If actually measured! (GOLD)

    // Orbital
    std::optional<double> period_days;
    std::optional<double> semi_major_axis_au;
    std::optional<double> eccentricity;
    std::optional<double> inclination_deg;
    std::optional<double> omega_deg;
    std::optional<double> tperi;            // Time of periastron

    // CRITICAL FOR COLOR - actual observations!
    std::optional<double> geometric_albedo;         // Measured reflectivity (GOLD)
    std::optional<std::string> molecules_detected;  // "H2O, CO2, CH4, Na" etc. (GOLD)
    std::optional<double> hot_point_lon;            // Hot spot longitude from phase curves

    // Star properties
    std::string star_name;
    std::optional<double> star_temp_k;
    std::optional<double> star_radius_solar;
    std::optional<double> star_mass_solar;
    std::optional<double> star_metallicity;
    std::optional<double> star_age_gyr;
    std::string star_spectral_type;

    // Timestamps and references
    std::string last_updated;
    std::string discovery_reference;
};

struct ExoplanetEuConfig {
    // Exoplanet.eu provides VOTable/CSV exports
    std::string api_endpoint = "http://exoplanet.eu/catalog/csv/";
    int timeout_seconds = 30;
    bool use_cache = true;
    std::string cache_directory = ".cache/exoplaneteu";
};

class ExoplanetEuClient {
public:
    explicit ExoplanetEuClient(const ExoplanetEuConfig& config = {});
    ~ExoplanetEuClient();

    ExoplanetEuClient(const ExoplanetEuClient&) = delete;
    ExoplanetEuClient& operator=(const ExoplanetEuClient&) = delete;

    // Query by planet name
    std::future<std::optional<ExoplanetEuData>> queryByName(const std::string& planetName);
    std::optional<ExoplanetEuData> queryByNameSync(const std::string& planetName);

    // Merge EU data into existing ExoplanetData (fills gaps, prefers NASA for conflicts)
    void mergeIntoExoplanetData(ExoplanetData& nasaData, const ExoplanetEuData& euData);

    // Download full catalog (for local searching)
    std::future<bool> downloadCatalog();

    // Test connection
    bool testConnection();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    ExoplanetEuData parseCSVRow(const std::string& row, const std::vector<std::string>& headers) const;
};

}  // namespace astrocore
