#pragma once

#include "data/ExoplanetData.hpp"
#include <random>

namespace astrocore {

class InferenceEngine;

// Empirical mass-radius relationship types based on Chen & Kipping (2017)
enum class PlanetRegime {
    TERRAN,         // M < 2.0 M_Earth  (rocky)
    NEPTUNIAN,      // 2.0 - 130 M_Earth (volatile-rich)
    JOVIAN,         // > 130 M_Earth (gas giants)
    STELLAR         // > 0.08 M_Sun (brown dwarfs/stars)
};

// Scientific exoplanet parameter predictor
// Uses empirical relationships from peer-reviewed literature
class ExoplanetPredictor {
public:
    ExoplanetPredictor();
    ~ExoplanetPredictor();

    // Set AI inference engine for advanced predictions
    void setInferenceEngine(InferenceEngine* engine) { m_inference = engine; }

    // Main entry point: fill ALL missing fields
    // Returns modified data with no NaN values in key fields
    ExoplanetData fillAllMissing(ExoplanetData data);

    // Individual prediction methods (public for testing)

    // Mass-Radius relationships (Chen & Kipping 2017, Zeng+ 2016)
    static double predictMassFromRadius(double radius_earth, PlanetRegime regime = PlanetRegime::TERRAN);
    static double predictRadiusFromMass(double mass_earth);
    static PlanetRegime classifyByMass(double mass_earth);
    static PlanetRegime classifyByRadius(double radius_earth);

    // Temperature predictions
    static double predictEquilibriumTemp(double stellar_temp_k, double stellar_radius_solar,
                                         double semi_major_axis_au, double albedo = 0.3);
    static double predictTemperatureFromInsolation(double insolation_earth, double albedo = 0.3);

    // Orbital predictions (Kepler's laws)
    static double predictOrbitalPeriod(double semi_major_axis_au, double stellar_mass_solar);
    static double predictSemiMajorAxis(double period_days, double stellar_mass_solar);

    // Density and composition
    static double predictDensity(double mass_earth, double radius_earth);
    static double predictSurfaceGravity(double mass_earth, double radius_earth);
    static std::string predictComposition(double density_gcc, double mass_earth);

    // Stellar parameters (from spectral type)
    static double estimateStellarTemp(const std::string& spectral_type);
    static double estimateStellarMass(const std::string& spectral_type);
    static double estimateStellarRadius(const std::string& spectral_type);
    static double estimateStellarLuminosity(double mass_solar);

    // Habitability metrics
    static double calculateESI(double radius_earth, double density_gcc,
                               double escape_vel_earth, double surface_temp_k);
    static double calculateHZDistance(double semi_major_axis_au, double stellar_luminosity_solar);

    // Statistical estimates for completely unknown planets
    // Uses occurrence rate distributions from Kepler mission
    double sampleTypicalMass();
    double sampleTypicalRadius();
    double sampleTypicalOrbitalPeriod();

private:
    void predictPhysicalParams(ExoplanetData& data);
    void predictOrbitalParams(ExoplanetData& data);
    void predictStellarParams(ExoplanetData& data);
    void predictTemperatureParams(ExoplanetData& data);
    void predictAtmosphericParams(ExoplanetData& data);
    void predictHabitabilityParams(ExoplanetData& data);
    void ensureAllFieldsValid(ExoplanetData& data);

    InferenceEngine* m_inference = nullptr;
    std::mt19937 m_rng;
};

}  // namespace astrocore
