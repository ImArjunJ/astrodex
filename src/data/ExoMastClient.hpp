#pragma once

#include <string>
#include <vector>
#include <future>
#include <optional>
#include <memory>

namespace astrocore {

// Detected molecular species in atmosphere
struct AtmosphericDetection {
    std::string molecule;           // e.g., "H2O", "CO2", "CH4", "Na", "K", "TiO"
    double abundance = 0.0;         // Volume mixing ratio if known
    double detection_significance;  // Sigma level of detection
    std::string instrument;         // e.g., "JWST/NIRSpec", "HST/WFC3"
    std::string reference;          // Paper reference
    bool is_upper_limit = false;    // True if this is an upper limit, not detection
};

// Spectroscopic observation data
struct SpectralObservation {
    std::string planet_name;
    std::string observation_type;   // "transmission", "emission", "reflection"
    std::string instrument;
    double wavelength_min_um;       // Wavelength range in microns
    double wavelength_max_um;
    std::string data_url;           // URL to actual spectrum data
    std::vector<AtmosphericDetection> detections;
};

// Full atmospheric characterization from ExoMAST
struct AtmosphericData {
    std::string planet_name;

    // Detected species
    std::vector<AtmosphericDetection> detections;

    // Derived atmospheric properties (if available)
    std::optional<double> cloud_top_pressure_bar;
    std::optional<double> atmospheric_metallicity;  // Relative to solar
    std::optional<double> c_to_o_ratio;             // Carbon to oxygen ratio
    std::optional<double> mean_molecular_weight;

    // Temperature structure
    std::optional<double> day_side_temp_k;
    std::optional<double> night_side_temp_k;
    std::optional<double> terminator_temp_k;

    // Observation metadata
    std::vector<SpectralObservation> observations;
    bool has_transmission_spectrum = false;
    bool has_emission_spectrum = false;

    // Quality indicators
    int num_observations = 0;
    std::string best_instrument;
};

struct ExoMastConfig {
    std::string api_endpoint = "https://exo.mast.stsci.edu/api/v0.1";
    int timeout_seconds = 30;
    bool use_cache = true;
    std::string cache_directory = ".cache/exomast";
};

class ExoMastClient {
public:
    explicit ExoMastClient(const ExoMastConfig& config = {});
    ~ExoMastClient();

    // Prevent copying
    ExoMastClient(const ExoMastClient&) = delete;
    ExoMastClient& operator=(const ExoMastClient&) = delete;

    // Query atmospheric data for a specific planet
    std::future<std::optional<AtmosphericData>> queryAtmosphere(const std::string& planetName);
    std::optional<AtmosphericData> queryAtmosphereSync(const std::string& planetName);

    // Get list of planets with atmospheric observations
    std::future<std::vector<std::string>> getPlanetsWithSpectra();

    // Check if a planet has any spectroscopic data
    bool hasSpectralData(const std::string& planetName);

    // Test connection
    bool testConnection();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    // Parse ExoMAST API responses
    AtmosphericData parseAtmosphereResponse(const std::string& json) const;
    std::vector<AtmosphericDetection> parseDetections(const std::string& json) const;
};

}  // namespace astrocore
