#include "data/ExoplanetPredictor.hpp"
#include "ai/InferenceEngine.hpp"
#include "core/Logger.hpp"
#include <cmath>
#include <algorithm>
#include <chrono>

namespace astrocore {

// Physical constants
namespace constants {
    constexpr double EARTH_MASS_KG = 5.972e24;
    constexpr double EARTH_RADIUS_M = 6.371e6;
    constexpr double EARTH_DENSITY_GCC = 5.51;
    constexpr double JUPITER_MASS_EARTH = 317.8;
    constexpr double JUPITER_RADIUS_EARTH = 11.2;
    constexpr double SOLAR_MASS_KG = 1.989e30;
    constexpr double SOLAR_RADIUS_AU = 0.00465047;
    constexpr double STEFAN_BOLTZMANN = 5.67e-8;
    constexpr double G = 6.674e-11;
    constexpr double AU_TO_M = 1.496e11;
    constexpr double SECONDS_PER_DAY = 86400.0;
}

ExoplanetPredictor::ExoplanetPredictor()
    : m_rng(std::chrono::steady_clock::now().time_since_epoch().count())
{
}

ExoplanetPredictor::~ExoplanetPredictor() = default;

// Chen & Kipping (2017) mass-radius relations
// "Probabilistic Forecasting of the Masses and Radii of Other Worlds"
PlanetRegime ExoplanetPredictor::classifyByMass(double mass_earth) {
    if (mass_earth < 2.0) return PlanetRegime::TERRAN;
    if (mass_earth < 130.0) return PlanetRegime::NEPTUNIAN;
    if (mass_earth < 26000.0) return PlanetRegime::JOVIAN;  // ~0.08 M_Sun
    return PlanetRegime::STELLAR;
}

PlanetRegime ExoplanetPredictor::classifyByRadius(double radius_earth) {
    if (radius_earth < 1.23) return PlanetRegime::TERRAN;
    if (radius_earth < 4.0) return PlanetRegime::NEPTUNIAN;
    if (radius_earth < 15.0) return PlanetRegime::JOVIAN;
    return PlanetRegime::STELLAR;
}

double ExoplanetPredictor::predictMassFromRadius(double radius_earth, PlanetRegime regime) {
    // Chen & Kipping (2017) M-R relations
    // Terran: M = 0.97 * R^3.45  (rocky composition)
    // Neptunian: M = 2.0 * R^1.74
    // Jovian: M = 318 * (R/11.2)^(-0.04) (nearly constant radius)

    switch (regime) {
        case PlanetRegime::TERRAN:
            return 0.97 * std::pow(radius_earth, 3.45);
        case PlanetRegime::NEPTUNIAN:
            return 2.0 * std::pow(radius_earth, 1.74);
        case PlanetRegime::JOVIAN:
            // Gas giants have nearly constant radius
            // Use average Jupiter mass scaled slightly
            return 318.0 * std::pow(radius_earth / 11.2, -0.04);
        default:
            return 1.0;
    }
}

double ExoplanetPredictor::predictRadiusFromMass(double mass_earth) {
    // Inverse of Chen & Kipping relations
    PlanetRegime regime = classifyByMass(mass_earth);

    switch (regime) {
        case PlanetRegime::TERRAN:
            // R = (M/0.97)^(1/3.45)
            return std::pow(mass_earth / 0.97, 1.0 / 3.45);
        case PlanetRegime::NEPTUNIAN:
            // R = (M/2.0)^(1/1.74)
            return std::pow(mass_earth / 2.0, 1.0 / 1.74);
        case PlanetRegime::JOVIAN:
            // Nearly constant ~11 R_Earth
            return 11.2 * std::pow(mass_earth / 318.0, -0.04);
        default:
            return 1.0;
    }
}

double ExoplanetPredictor::predictEquilibriumTemp(double stellar_temp_k,
                                                   double stellar_radius_solar,
                                                   double semi_major_axis_au,
                                                   double albedo) {
    // T_eq = T_star * sqrt(R_star / (2*a)) * (1-A)^0.25
    if (stellar_temp_k <= 0 || stellar_radius_solar <= 0 || semi_major_axis_au <= 0) {
        return 288.0;  // Earth-like default
    }

    double R_star_au = stellar_radius_solar * constants::SOLAR_RADIUS_AU;
    return stellar_temp_k * std::sqrt(R_star_au / (2.0 * semi_major_axis_au)) *
           std::pow(1.0 - albedo, 0.25);
}

double ExoplanetPredictor::predictTemperatureFromInsolation(double insolation_earth, double albedo) {
    // T_eq = 255K * S^0.25 * (1-A)^0.25  where 255K is Earth's effective temp
    if (insolation_earth <= 0) return 288.0;
    return 255.0 * std::pow(insolation_earth, 0.25) * std::pow(1.0 - albedo, 0.25);
}

double ExoplanetPredictor::predictOrbitalPeriod(double semi_major_axis_au, double stellar_mass_solar) {
    // Kepler's 3rd law: P^2 = a^3 / M_star (in years, AU, solar masses)
    if (semi_major_axis_au <= 0 || stellar_mass_solar <= 0) return 365.25;

    double period_years = std::sqrt(std::pow(semi_major_axis_au, 3.0) / stellar_mass_solar);
    return period_years * 365.25;  // Convert to days
}

double ExoplanetPredictor::predictSemiMajorAxis(double period_days, double stellar_mass_solar) {
    // Inverse Kepler's 3rd law
    if (period_days <= 0 || stellar_mass_solar <= 0) return 1.0;

    double period_years = period_days / 365.25;
    return std::pow(period_years * period_years * stellar_mass_solar, 1.0 / 3.0);
}

double ExoplanetPredictor::predictDensity(double mass_earth, double radius_earth) {
    if (mass_earth <= 0 || radius_earth <= 0) return constants::EARTH_DENSITY_GCC;
    return constants::EARTH_DENSITY_GCC * mass_earth / std::pow(radius_earth, 3.0);
}

double ExoplanetPredictor::predictSurfaceGravity(double mass_earth, double radius_earth) {
    if (mass_earth <= 0 || radius_earth <= 0) return 1.0;
    return mass_earth / std::pow(radius_earth, 2.0);
}

std::string ExoplanetPredictor::predictComposition(double density_gcc, double mass_earth) {
    // Based on Zeng et al. (2016) composition curves
    if (density_gcc > 8.0) return "Iron-rich";
    if (density_gcc > 5.0) return "Earth-like (silicate/iron)";
    if (density_gcc > 3.0) return "Rocky with volatiles";
    if (density_gcc > 1.5) return "Water-rich";
    if (density_gcc > 0.5) return "Ice/volatile dominated";
    if (mass_earth > 50.0) return "H/He dominated";
    return "Low-density volatile";
}

// Stellar parameter estimation from spectral type
// Based on Mamajek (2013) stellar parameters table
double ExoplanetPredictor::estimateStellarTemp(const std::string& spectral_type) {
    if (spectral_type.empty()) return 5778.0;  // Sun

    char type = std::toupper(spectral_type[0]);
    int subtype = 5;  // Default middle of class

    if (spectral_type.size() > 1 && std::isdigit(spectral_type[1])) {
        subtype = spectral_type[1] - '0';
    }

    // Approximate effective temperatures
    switch (type) {
        case 'O': return 30000 + (9 - subtype) * 4000;
        case 'B': return 10000 + (9 - subtype) * 2000;
        case 'A': return 7500 + (9 - subtype) * 250;
        case 'F': return 6000 + (9 - subtype) * 150;
        case 'G': return 5200 + (9 - subtype) * 80;
        case 'K': return 3700 + (9 - subtype) * 150;
        case 'M': return 2400 + (9 - subtype) * 130;
        default: return 5778.0;
    }
}

double ExoplanetPredictor::estimateStellarMass(const std::string& spectral_type) {
    if (spectral_type.empty()) return 1.0;

    char type = std::toupper(spectral_type[0]);

    switch (type) {
        case 'O': return 30.0;
        case 'B': return 5.0;
        case 'A': return 2.0;
        case 'F': return 1.3;
        case 'G': return 1.0;
        case 'K': return 0.7;
        case 'M': return 0.3;
        default: return 1.0;
    }
}

double ExoplanetPredictor::estimateStellarRadius(const std::string& spectral_type) {
    if (spectral_type.empty()) return 1.0;

    char type = std::toupper(spectral_type[0]);

    // Main sequence radii
    switch (type) {
        case 'O': return 10.0;
        case 'B': return 4.0;
        case 'A': return 1.8;
        case 'F': return 1.3;
        case 'G': return 1.0;
        case 'K': return 0.8;
        case 'M': return 0.4;
        default: return 1.0;
    }
}

double ExoplanetPredictor::estimateStellarLuminosity(double mass_solar) {
    // Mass-luminosity relation: L ∝ M^3.5 for main sequence
    if (mass_solar <= 0) return 1.0;
    return std::pow(mass_solar, 3.5);
}

double ExoplanetPredictor::calculateESI(double radius_earth, double density_gcc,
                                         double escape_vel_earth, double surface_temp_k) {
    // Earth Similarity Index (Schulze-Makuch et al. 2011)
    // ESI = (ESI_r * ESI_ρ * ESI_v * ESI_T)^(1/4)

    auto similarity = [](double x, double x0, double w) {
        return std::pow(1.0 - std::abs((x - x0) / (x + x0)), w);
    };

    double esi_r = similarity(radius_earth, 1.0, 0.57);
    double esi_d = similarity(density_gcc, 5.51, 1.07);
    double esi_v = similarity(escape_vel_earth, 1.0, 0.70);
    double esi_t = similarity(surface_temp_k, 288.0, 5.58);

    return std::pow(esi_r * esi_d * esi_v * esi_t, 0.25);
}

double ExoplanetPredictor::calculateHZDistance(double semi_major_axis_au, double stellar_luminosity_solar) {
    if (stellar_luminosity_solar <= 0 || semi_major_axis_au <= 0) return 1.0;

    // Conservative HZ: 0.95-1.67 * sqrt(L) AU
    double hz_inner = 0.95 * std::sqrt(stellar_luminosity_solar);
    double hz_outer = 1.67 * std::sqrt(stellar_luminosity_solar);
    double hz_center = (hz_inner + hz_outer) / 2.0;

    return semi_major_axis_au / hz_center;
}

// Statistical sampling for completely unknown parameters
// Based on Kepler occurrence rates
double ExoplanetPredictor::sampleTypicalMass() {
    // Log-uniform distribution peaked at super-Earth masses
    std::lognormal_distribution<double> dist(1.0, 1.5);  // Peak ~3 M_Earth
    return std::clamp(dist(m_rng), 0.1, 1000.0);
}

double ExoplanetPredictor::sampleTypicalRadius() {
    // Most planets are 1-4 R_Earth (Fressin+ 2013)
    std::lognormal_distribution<double> dist(0.5, 0.6);  // Peak ~1.6 R_Earth
    return std::clamp(dist(m_rng), 0.3, 15.0);
}

double ExoplanetPredictor::sampleTypicalOrbitalPeriod() {
    // Detection bias toward short periods, but real distribution broader
    std::lognormal_distribution<double> dist(2.5, 1.5);  // Peak ~12 days
    return std::clamp(dist(m_rng), 0.5, 10000.0);
}

// Main prediction pipeline
ExoplanetData ExoplanetPredictor::fillAllMissing(ExoplanetData data) {
    LOG_INFO("Predicting missing parameters for {}", data.name);

    // Order matters - each step may provide inputs for the next
    predictStellarParams(data);
    predictOrbitalParams(data);
    predictPhysicalParams(data);
    predictTemperatureParams(data);
    predictHabitabilityParams(data);
    predictAtmosphericParams(data);

    // Final pass: ensure no NaN values remain
    ensureAllFieldsValid(data);

    LOG_INFO("Prediction complete for {}", data.name);
    return data;
}

void ExoplanetPredictor::predictStellarParams(ExoplanetData& data) {
    // If we have spectral type, estimate stellar parameters
    if (!data.host_star.spectral_type.empty()) {
        if (!data.host_star.effective_temp_k.hasValue()) {
            data.host_star.effective_temp_k.value = estimateStellarTemp(data.host_star.spectral_type);
            data.host_star.effective_temp_k.source = DataSource::CALCULATED;
            data.host_star.effective_temp_k.confidence = 0.7f;
        }
        if (!data.host_star.mass_solar.hasValue()) {
            data.host_star.mass_solar.value = estimateStellarMass(data.host_star.spectral_type);
            data.host_star.mass_solar.source = DataSource::CALCULATED;
            data.host_star.mass_solar.confidence = 0.6f;
        }
        if (!data.host_star.radius_solar.hasValue()) {
            data.host_star.radius_solar.value = estimateStellarRadius(data.host_star.spectral_type);
            data.host_star.radius_solar.source = DataSource::CALCULATED;
            data.host_star.radius_solar.confidence = 0.6f;
        }
    }

    // Calculate luminosity from mass
    if (!data.host_star.luminosity_solar.hasValue() && data.host_star.mass_solar.hasValue()) {
        data.host_star.luminosity_solar.value = estimateStellarLuminosity(data.host_star.mass_solar.value);
        data.host_star.luminosity_solar.source = DataSource::CALCULATED;
    }

    // If still missing key stellar params, use solar defaults
    if (!data.host_star.effective_temp_k.hasValue()) {
        data.host_star.effective_temp_k.value = 5778.0;  // Sun
        data.host_star.effective_temp_k.source = DataSource::CALCULATED;
        data.host_star.effective_temp_k.confidence = 0.3f;
    }
    if (!data.host_star.mass_solar.hasValue()) {
        data.host_star.mass_solar.value = 1.0;
        data.host_star.mass_solar.source = DataSource::CALCULATED;
        data.host_star.mass_solar.confidence = 0.3f;
    }
    if (!data.host_star.radius_solar.hasValue()) {
        data.host_star.radius_solar.value = 1.0;
        data.host_star.radius_solar.source = DataSource::CALCULATED;
        data.host_star.radius_solar.confidence = 0.3f;
    }
    if (!data.host_star.luminosity_solar.hasValue()) {
        data.host_star.luminosity_solar.value = 1.0;
        data.host_star.luminosity_solar.source = DataSource::CALCULATED;
    }
}

void ExoplanetPredictor::predictOrbitalParams(ExoplanetData& data) {
    // If we have period but not semi-major axis
    if (data.orbital_period_days.hasValue() && !data.semi_major_axis_au.hasValue()) {
        data.semi_major_axis_au.value = predictSemiMajorAxis(
            data.orbital_period_days.value,
            data.host_star.mass_solar.value
        );
        data.semi_major_axis_au.source = DataSource::CALCULATED;
    }

    // If we have semi-major axis but not period
    if (data.semi_major_axis_au.hasValue() && !data.orbital_period_days.hasValue()) {
        data.orbital_period_days.value = predictOrbitalPeriod(
            data.semi_major_axis_au.value,
            data.host_star.mass_solar.value
        );
        data.orbital_period_days.source = DataSource::CALCULATED;
    }

    // If neither, sample a typical value
    if (!data.orbital_period_days.hasValue()) {
        data.orbital_period_days.value = sampleTypicalOrbitalPeriod();
        data.orbital_period_days.source = DataSource::AI_INFERRED;
        data.orbital_period_days.confidence = 0.2f;
        data.orbital_period_days.ai_reasoning = "Sampled from Kepler occurrence distribution";
    }

    if (!data.semi_major_axis_au.hasValue()) {
        data.semi_major_axis_au.value = predictSemiMajorAxis(
            data.orbital_period_days.value,
            data.host_star.mass_solar.value
        );
        data.semi_major_axis_au.source = DataSource::CALCULATED;
    }

    // Default eccentricity to near-circular if unknown
    if (!data.eccentricity.hasValue()) {
        data.eccentricity.value = 0.05;  // Slightly eccentric is typical
        data.eccentricity.source = DataSource::CALCULATED;
        data.eccentricity.confidence = 0.4f;
    }

    // Default inclination to edge-on (transit detection bias)
    if (!data.inclination_deg.hasValue()) {
        data.inclination_deg.value = 87.0;  // Near edge-on typical for transiting planets
        data.inclination_deg.source = DataSource::CALCULATED;
        data.inclination_deg.confidence = 0.5f;
    }
}

void ExoplanetPredictor::predictPhysicalParams(ExoplanetData& data) {
    bool hasMass = data.mass_earth.hasValue();
    bool hasRadius = data.radius_earth.hasValue();

    // If we have mass but not radius
    if (hasMass && !hasRadius) {
        data.radius_earth.value = predictRadiusFromMass(data.mass_earth.value);
        data.radius_earth.source = DataSource::CALCULATED;
        data.radius_earth.confidence = 0.7f;
        LOG_DEBUG("Predicted radius {:.2f} R_E from mass {:.2f} M_E",
                  data.radius_earth.value, data.mass_earth.value);
    }

    // If we have radius but not mass
    if (!hasMass && hasRadius) {
        PlanetRegime regime = classifyByRadius(data.radius_earth.value);
        data.mass_earth.value = predictMassFromRadius(data.radius_earth.value, regime);
        data.mass_earth.source = DataSource::CALCULATED;
        data.mass_earth.confidence = 0.6f;
        LOG_DEBUG("Predicted mass {:.2f} M_E from radius {:.2f} R_E",
                  data.mass_earth.value, data.radius_earth.value);
    }

    // If neither, sample typical values
    if (!data.radius_earth.hasValue()) {
        data.radius_earth.value = sampleTypicalRadius();
        data.radius_earth.source = DataSource::AI_INFERRED;
        data.radius_earth.confidence = 0.3f;
        data.radius_earth.ai_reasoning = "Sampled from Kepler size distribution";
    }

    if (!data.mass_earth.hasValue()) {
        PlanetRegime regime = classifyByRadius(data.radius_earth.value);
        data.mass_earth.value = predictMassFromRadius(data.radius_earth.value, regime);
        data.mass_earth.source = DataSource::CALCULATED;
        data.mass_earth.confidence = 0.5f;
    }

    // Convert to Jupiter units
    if (!data.mass_jupiter.hasValue()) {
        data.mass_jupiter.value = data.mass_earth.value / constants::JUPITER_MASS_EARTH;
        data.mass_jupiter.source = DataSource::CALCULATED;
    }
    if (!data.radius_jupiter.hasValue()) {
        data.radius_jupiter.value = data.radius_earth.value / constants::JUPITER_RADIUS_EARTH;
        data.radius_jupiter.source = DataSource::CALCULATED;
    }

    // Calculate density
    if (!data.density_gcc.hasValue()) {
        data.density_gcc.value = predictDensity(data.mass_earth.value, data.radius_earth.value);
        data.density_gcc.source = DataSource::CALCULATED;
    }

    // Calculate surface gravity
    if (!data.surface_gravity_g.hasValue()) {
        data.surface_gravity_g.value = predictSurfaceGravity(data.mass_earth.value, data.radius_earth.value);
        data.surface_gravity_g.source = DataSource::CALCULATED;
    }

    // Classify planet type
    if (!data.planet_type.hasValue()) {
        double mass = data.mass_earth.value;
        double radius = data.radius_earth.value;
        double density = data.density_gcc.value;

        if (mass > 50.0 || radius > 6.0) {
            data.planet_type.value = "Gas Giant";
        } else if (mass > 10.0 || radius > 3.5) {
            if (density < 2.0) {
                data.planet_type.value = "Ice Giant";
            } else {
                data.planet_type.value = "Mini-Neptune";
            }
        } else if (mass > 1.5 || radius > 1.25) {
            data.planet_type.value = "Super-Earth";
        } else {
            data.planet_type.value = "Rocky";
        }
        data.planet_type.source = DataSource::CALCULATED;
    }
}

void ExoplanetPredictor::predictTemperatureParams(ExoplanetData& data) {
    // Calculate insolation flux
    if (!data.insolation_flux.hasValue() && data.semi_major_axis_au.hasValue()) {
        double L = data.host_star.luminosity_solar.hasValue() ?
                   data.host_star.luminosity_solar.value : 1.0;
        double a = data.semi_major_axis_au.value;
        data.insolation_flux.value = L / (a * a);
        data.insolation_flux.source = DataSource::CALCULATED;
    }

    // Calculate equilibrium temperature
    if (!data.equilibrium_temp_k.hasValue()) {
        if (data.insolation_flux.hasValue()) {
            data.equilibrium_temp_k.value = predictTemperatureFromInsolation(
                data.insolation_flux.value,
                data.albedo.hasValue() ? data.albedo.value : 0.3
            );
        } else {
            data.equilibrium_temp_k.value = predictEquilibriumTemp(
                data.host_star.effective_temp_k.value,
                data.host_star.radius_solar.value,
                data.semi_major_axis_au.value,
                data.albedo.hasValue() ? data.albedo.value : 0.3
            );
        }
        data.equilibrium_temp_k.source = DataSource::CALCULATED;
    }

    // Default albedo based on planet type
    if (!data.albedo.hasValue()) {
        std::string type = data.planet_type.hasValue() ? data.planet_type.value : "";
        if (type == "Gas Giant" || type == "Ice Giant") {
            data.albedo.value = 0.35;  // Jupiter-like
        } else if (data.equilibrium_temp_k.value > 1000) {
            data.albedo.value = 0.1;  // Hot planets, dark
        } else if (data.equilibrium_temp_k.value < 200) {
            data.albedo.value = 0.6;  // Ice-covered
        } else {
            data.albedo.value = 0.3;  // Earth-like
        }
        data.albedo.source = DataSource::CALCULATED;
    }
}

void ExoplanetPredictor::predictHabitabilityParams(ExoplanetData& data) {
    // Habitable zone distance
    if (!data.habitable_zone_distance.hasValue()) {
        data.habitable_zone_distance.value = calculateHZDistance(
            data.semi_major_axis_au.value,
            data.host_star.luminosity_solar.value
        );
        data.habitable_zone_distance.source = DataSource::CALCULATED;
    }

    // Earth Similarity Index
    if (!data.earth_similarity_index.hasValue()) {
        double escape_vel = std::sqrt(data.mass_earth.value / data.radius_earth.value);
        data.earth_similarity_index.value = calculateESI(
            data.radius_earth.value,
            data.density_gcc.value,
            escape_vel,
            data.equilibrium_temp_k.value
        );
        data.earth_similarity_index.source = DataSource::CALCULATED;
    }
}

void ExoplanetPredictor::predictAtmosphericParams(ExoplanetData& data) {
    // Use AI inference if available for atmospheric parameters
    if (m_inference && m_inference->isAvailable()) {
        if (!data.atmosphere_composition.hasValue() ||
            !data.surface_pressure_atm.hasValue()) {
            m_inference->inferAtmosphere(data);
        }
    }

    // Fallback: estimate atmosphere based on physics
    if (!data.surface_pressure_atm.hasValue()) {
        double mass = data.mass_earth.value;
        double temp = data.equilibrium_temp_k.value;
        double gravity = data.surface_gravity_g.value;

        // Rough estimation based on escape velocity and temperature
        double escape_vel = 11.2 * std::sqrt(mass / data.radius_earth.value);  // km/s
        double thermal_vel = 0.157 * std::sqrt(temp);  // km/s for hydrogen

        if (escape_vel < 2 * thermal_vel) {
            // Lost atmosphere
            data.surface_pressure_atm.value = 0.0;
        } else if (mass < 0.5) {
            // Small rocky, minimal atmosphere
            data.surface_pressure_atm.value = 0.001;
        } else if (mass > 10.0) {
            // Gas/ice giant
            data.surface_pressure_atm.value = 1000.0;  // Deep atmosphere
        } else {
            // Earth-like potential
            data.surface_pressure_atm.value = gravity;  // Scale with gravity
        }
        data.surface_pressure_atm.source = DataSource::CALCULATED;
        data.surface_pressure_atm.confidence = 0.4f;
    }

    // Estimate atmosphere composition if missing
    if (!data.atmosphere_composition.hasValue()) {
        double temp = data.equilibrium_temp_k.value;
        double mass = data.mass_earth.value;

        nlohmann::json comp;
        if (mass > 10.0) {
            // Gas giant
            comp = {{"H2", 85}, {"He", 15}};
        } else if (temp > 1500) {
            // Hot, volatiles evaporated
            comp = {{"Na", 30}, {"K", 20}, {"CO2", 50}};
        } else if (temp > 373 && temp < 1000) {
            // Venus-like
            comp = {{"CO2", 95}, {"N2", 3}, {"SO2", 2}};
        } else if (temp >= 200 && temp <= 373) {
            // Potentially habitable
            comp = {{"N2", 78}, {"O2", 21}, {"Ar", 1}};
        } else {
            // Cold
            comp = {{"N2", 95}, {"CH4", 3}, {"CO", 2}};
        }

        data.atmosphere_composition.value = comp.dump();
        data.atmosphere_composition.source = DataSource::CALCULATED;
        data.atmosphere_composition.confidence = 0.3f;
    }

    // Cloud coverage estimation
    if (!data.cloud_coverage_fraction.hasValue()) {
        double temp = data.equilibrium_temp_k.value;
        if (temp > 373) {
            data.cloud_coverage_fraction.value = 0.9;  // Venus-like clouds
        } else if (temp > 273 && temp < 373) {
            data.cloud_coverage_fraction.value = 0.5;  // Earth-like
        } else {
            data.cloud_coverage_fraction.value = 0.3;  // Cold, less clouds
        }
        data.cloud_coverage_fraction.source = DataSource::CALCULATED;
    }

    // Ocean coverage
    if (!data.ocean_coverage_fraction.hasValue()) {
        double temp = data.equilibrium_temp_k.value;
        double hz = data.habitable_zone_distance.value;

        if (temp >= 273 && temp <= 373 && hz > 0.5 && hz < 1.5) {
            data.ocean_coverage_fraction.value = 0.7;  // Potentially habitable
        } else if (temp > 373) {
            data.ocean_coverage_fraction.value = 0.0;  // Too hot
        } else {
            data.ocean_coverage_fraction.value = 0.0;  // Frozen or no water
        }
        data.ocean_coverage_fraction.source = DataSource::CALCULATED;
    }

    // Ice coverage
    if (!data.ice_coverage_fraction.hasValue()) {
        double temp = data.equilibrium_temp_k.value;
        if (temp < 200) {
            data.ice_coverage_fraction.value = 0.9;
        } else if (temp < 273) {
            data.ice_coverage_fraction.value = 0.5;
        } else {
            data.ice_coverage_fraction.value = 0.05;
        }
        data.ice_coverage_fraction.source = DataSource::CALCULATED;
    }
}

void ExoplanetPredictor::ensureAllFieldsValid(ExoplanetData& data) {
    // Final pass to ensure no NaN values in critical fields

    auto ensureValid = [](MeasuredValue<double>& val, double fallback, const char* name) {
        if (!val.hasValue() || std::isnan(val.value) || std::isinf(val.value)) {
            LOG_WARN("Field {} invalid, using fallback {}", name, fallback);
            val.value = fallback;
            val.source = DataSource::CALCULATED;
            val.confidence = 0.1f;
        }
    };

    // Physical
    ensureValid(data.mass_earth, 1.0, "mass_earth");
    ensureValid(data.radius_earth, 1.0, "radius_earth");
    ensureValid(data.density_gcc, 5.5, "density_gcc");
    ensureValid(data.surface_gravity_g, 1.0, "surface_gravity_g");

    // Orbital
    ensureValid(data.orbital_period_days, 365.25, "orbital_period_days");
    ensureValid(data.semi_major_axis_au, 1.0, "semi_major_axis_au");
    ensureValid(data.eccentricity, 0.02, "eccentricity");

    // Temperature
    ensureValid(data.equilibrium_temp_k, 288.0, "equilibrium_temp_k");
    ensureValid(data.albedo, 0.3, "albedo");
    ensureValid(data.insolation_flux, 1.0, "insolation_flux");

    // Habitability
    ensureValid(data.habitable_zone_distance, 1.0, "habitable_zone_distance");
    ensureValid(data.earth_similarity_index, 0.5, "earth_similarity_index");

    // Atmospheric
    ensureValid(data.surface_pressure_atm, 1.0, "surface_pressure_atm");
    ensureValid(data.cloud_coverage_fraction, 0.5, "cloud_coverage_fraction");
    ensureValid(data.ocean_coverage_fraction, 0.0, "ocean_coverage_fraction");
    ensureValid(data.ice_coverage_fraction, 0.0, "ice_coverage_fraction");

    // Stellar
    ensureValid(data.host_star.effective_temp_k, 5778.0, "stellar_temp");
    ensureValid(data.host_star.mass_solar, 1.0, "stellar_mass");
    ensureValid(data.host_star.radius_solar, 1.0, "stellar_radius");
    ensureValid(data.host_star.luminosity_solar, 1.0, "stellar_luminosity");

    // String fields
    if (!data.planet_type.hasValue() || data.planet_type.value.empty()) {
        data.planet_type.value = "Unknown";
        data.planet_type.source = DataSource::CALCULATED;
    }

    if (!data.atmosphere_composition.hasValue() || data.atmosphere_composition.value.empty()) {
        data.atmosphere_composition.value = R"({"N2": 78, "O2": 21, "Ar": 1})";
        data.atmosphere_composition.source = DataSource::CALCULATED;
    }

    if (data.host_star.name.empty()) {
        data.host_star.name = data.name + " (host)";
    }
}

}  // namespace astrocore
