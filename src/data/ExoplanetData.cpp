#include "data/ExoplanetData.hpp"
#include <cmath>

namespace astrocore {

// Helper to parse DataSource from string
static DataSource parseDataSource(const std::string& str) {
    if (str == "NASA") return DataSource::NASA_TAP;
    if (str == "ExoAtmos") return DataSource::EXOATMOS;
    if (str == "AI") return DataSource::AI_INFERRED;
    if (str == "Calculated") return DataSource::CALCULATED;
    if (str == "Gaia DR3") return DataSource::GAIA;
    if (str == "CDS/VizieR") return DataSource::CDS_VIZIER;
    if (str == "OEC") return DataSource::OEC;
    return DataSource::UNKNOWN;
}

void ExoplanetData::calculateDerivedValues() {
    // Calculate mass in Jupiter if we have Earth mass
    if (mass_earth.hasValue() && !mass_jupiter.hasValue()) {
        mass_jupiter.value = mass_earth.value / constants::JUPITER_TO_EARTH_MASS;
        mass_jupiter.source = DataSource::CALCULATED;
    }

    // Calculate mass in Earth if we have Jupiter mass
    if (mass_jupiter.hasValue() && !mass_earth.hasValue()) {
        mass_earth.value = mass_jupiter.value * constants::JUPITER_TO_EARTH_MASS;
        mass_earth.source = DataSource::CALCULATED;
    }

    // Calculate radius in Jupiter if we have Earth radius
    if (radius_earth.hasValue() && !radius_jupiter.hasValue()) {
        radius_jupiter.value = radius_earth.value / constants::JUPITER_TO_EARTH_RADIUS;
        radius_jupiter.source = DataSource::CALCULATED;
    }

    // Calculate radius in Earth if we have Jupiter radius
    if (radius_jupiter.hasValue() && !radius_earth.hasValue()) {
        radius_earth.value = radius_jupiter.value * constants::JUPITER_TO_EARTH_RADIUS;
        radius_earth.source = DataSource::CALCULATED;
    }

    // Calculate density from mass and radius
    if (mass_earth.hasValue() && radius_earth.hasValue() && !density_gcc.hasValue()) {
        // density = mass / volume, volume scales as r^3
        density_gcc.value = constants::EARTH_DENSITY_GCC * mass_earth.value / std::pow(radius_earth.value, 3.0);
        density_gcc.source = DataSource::CALCULATED;
    }

    // Calculate surface gravity
    if (mass_earth.hasValue() && radius_earth.hasValue() && !surface_gravity_g.hasValue()) {
        // g = GM/r^2 -> g_planet/g_earth = (M/M_earth) / (R/R_earth)^2
        surface_gravity_g.value = mass_earth.value / std::pow(radius_earth.value, 2.0);
        surface_gravity_g.source = DataSource::CALCULATED;
    }

    // Calculate equilibrium temperature if not provided
    if (!equilibrium_temp_k.hasValue() &&
        host_star.effective_temp_k.hasValue() &&
        host_star.radius_solar.hasValue() &&
        semi_major_axis_au.hasValue()) {
        // T_eq = T_star * sqrt(R_star / (2 * a)) * (1 - A)^0.25
        double A = albedo.hasValue() ? albedo.value : constants::DEFAULT_ALBEDO;
        double R_star_au = host_star.radius_solar.value * constants::SOLAR_RADIUS_AU;
        equilibrium_temp_k.value = host_star.effective_temp_k.value *
            std::sqrt(R_star_au / (2.0 * semi_major_axis_au.value)) *
            std::pow(1.0 - A, 0.25);
        equilibrium_temp_k.source = DataSource::CALCULATED;
    }

    // Calculate habitable zone distance
    if (host_star.luminosity_solar.hasValue() && semi_major_axis_au.hasValue()) {
        double L = host_star.luminosity_solar.value;
        double hz_inner = std::sqrt(L) * constants::HZ_INNER_FACTOR;
        double hz_outer = std::sqrt(L) * constants::HZ_OUTER_FACTOR;
        double hz_center = (hz_inner + hz_outer) / 2.0;

        habitable_zone_distance.value = semi_major_axis_au.value / hz_center;
        habitable_zone_distance.source = DataSource::CALCULATED;
    }

    // Classify planet type based on mass and radius
    if (!planet_type.hasValue() && mass_earth.hasValue() && radius_earth.hasValue()) {
        double mass = mass_earth.value;
        double radius = radius_earth.value;
        double density = density_gcc.hasValue() ? density_gcc.value : 0.0;

        if (mass > 50.0 || radius > 6.0) {
            planet_type.value = "Gas Giant";
        } else if (mass > 10.0 || radius > 3.5) {
            if (density > 0 && density < 2.0) {
                planet_type.value = "Ice Giant";
            } else {
                planet_type.value = "Mini-Neptune";
            }
        } else if (mass > 1.5 || radius > 1.25) {
            planet_type.value = "Super-Earth";
        } else {
            planet_type.value = "Rocky";
        }
        planet_type.source = DataSource::CALCULATED;
    }
}

bool ExoplanetData::hasMinimumRenderData() const {
    // Need at least radius (or mass to estimate radius) and temperature
    bool hasSize = radius_earth.hasValue() || radius_jupiter.hasValue() ||
                   mass_earth.hasValue() || mass_jupiter.hasValue();
    bool hasTemp = equilibrium_temp_k.hasValue();

    return hasSize && hasTemp;
}

// ---- Helper lambdas for serializing MeasuredValue with ai_reasoning/confidence ----

// Serialize a double MeasuredValue into a JSON section
static void serializeDouble(nlohmann::json& section, const std::string& key,
                            const MeasuredValue<double>& field) {
    if (!field.hasValue()) return;
    section[key] = field.value;
    section[key + "_source"] = dataSourceToString(field.source);
    if (field.ai_reasoning.has_value()) {
        section[key + "_reasoning"] = *field.ai_reasoning;
    }
    if (field.confidence != 1.0f) {
        section[key + "_confidence"] = field.confidence;
    }
}

// Serialize a string MeasuredValue into a JSON section
static void serializeString(nlohmann::json& section, const std::string& key,
                            const MeasuredValue<std::string>& field) {
    if (field.value.empty()) return;
    section[key] = field.value;
    section[key + "_source"] = dataSourceToString(field.source);
    if (field.ai_reasoning.has_value()) {
        section[key + "_reasoning"] = *field.ai_reasoning;
    }
    if (field.confidence != 1.0f) {
        section[key + "_confidence"] = field.confidence;
    }
}

nlohmann::json ExoplanetData::toJson() const {
    nlohmann::json j;

    j["name"] = name;
    j["discovery_method"] = discovery_method;
    j["discovery_year"] = discovery_year;

    // Host star
    j["host_star"]["name"] = host_star.name;
    j["host_star"]["spectral_type"] = host_star.spectral_type;
    serializeDouble(j["host_star"], "effective_temp_k", host_star.effective_temp_k);
    serializeDouble(j["host_star"], "radius_solar", host_star.radius_solar);
    serializeDouble(j["host_star"], "mass_solar", host_star.mass_solar);
    serializeDouble(j["host_star"], "luminosity_solar", host_star.luminosity_solar);
    serializeDouble(j["host_star"], "metallicity", host_star.metallicity);
    serializeDouble(j["host_star"], "distance_pc", host_star.distance_pc);
    serializeDouble(j["host_star"], "age_gyr", host_star.age_gyr);
    serializeDouble(j["host_star"], "ra_deg", host_star.ra_deg);
    serializeDouble(j["host_star"], "dec_deg", host_star.dec_deg);

    // Orbital parameters
    serializeDouble(j["orbital"], "period_days", orbital_period_days);
    serializeDouble(j["orbital"], "semi_major_axis_au", semi_major_axis_au);
    serializeDouble(j["orbital"], "eccentricity", eccentricity);
    serializeDouble(j["orbital"], "inclination_deg", inclination_deg);
    serializeDouble(j["orbital"], "omega_deg", omega_deg);

    // Physical parameters
    serializeDouble(j["physical"], "mass_earth", mass_earth);
    serializeDouble(j["physical"], "mass_jupiter", mass_jupiter);
    serializeDouble(j["physical"], "radius_earth", radius_earth);
    serializeDouble(j["physical"], "radius_jupiter", radius_jupiter);
    serializeDouble(j["physical"], "density_gcc", density_gcc);
    serializeDouble(j["physical"], "equilibrium_temp_k", equilibrium_temp_k);
    serializeDouble(j["physical"], "surface_gravity_g", surface_gravity_g);

    // Atmosphere
    serializeDouble(j["atmosphere"], "surface_pressure_atm", surface_pressure_atm);
    serializeDouble(j["atmosphere"], "albedo", albedo);
    serializeDouble(j["atmosphere"], "greenhouse_effect", greenhouse_effect);
    serializeString(j["atmosphere"], "atmosphere_composition", atmosphere_composition);
    serializeDouble(j["atmosphere"], "ocean_coverage_fraction", ocean_coverage_fraction);
    serializeDouble(j["atmosphere"], "cloud_coverage_fraction", cloud_coverage_fraction);
    serializeDouble(j["atmosphere"], "ice_coverage_fraction", ice_coverage_fraction);

    // Rendering
    serializeString(j["rendering"], "biome_classification", biome_classification);
    serializeString(j["rendering"], "surface_color_hint", surface_color_hint);

    // Classification
    serializeString(j["classification"], "planet_type", planet_type);
    serializeDouble(j["classification"], "hz_distance", habitable_zone_distance);
    serializeDouble(j["classification"], "earth_similarity_index", earth_similarity_index);

    return j;
}

ExoplanetData ExoplanetData::fromJson(const nlohmann::json& j) {
    ExoplanetData data;

    // Helper to parse a MeasuredValue<double> field from a JSON section,
    // including ai_reasoning and confidence
    auto parseMeasuredField = [](const nlohmann::json& section,
                                 const std::string& key,
                                 MeasuredValue<double>& field,
                                 DataSource defaultSource) {
        if (section.contains(key)) {
            field.value = section[key].get<double>();
            std::string sourceKey = key + "_source";
            field.source = section.contains(sourceKey)
                ? parseDataSource(section[sourceKey].get<std::string>())
                : defaultSource;

            std::string reasonKey = key + "_reasoning";
            if (section.contains(reasonKey)) {
                field.ai_reasoning = section[reasonKey].get<std::string>();
            }
            std::string confKey = key + "_confidence";
            if (section.contains(confKey)) {
                field.confidence = section[confKey].get<float>();
            }
        }
    };

    auto parseMeasuredString = [](const nlohmann::json& section,
                                  const std::string& key,
                                  MeasuredValue<std::string>& field,
                                  DataSource defaultSource) {
        if (section.contains(key)) {
            field.value = section[key].get<std::string>();
            std::string sourceKey = key + "_source";
            field.source = section.contains(sourceKey)
                ? parseDataSource(section[sourceKey].get<std::string>())
                : defaultSource;

            std::string reasonKey = key + "_reasoning";
            if (section.contains(reasonKey)) {
                field.ai_reasoning = section[reasonKey].get<std::string>();
            }
            std::string confKey = key + "_confidence";
            if (section.contains(confKey)) {
                field.confidence = section[confKey].get<float>();
            }
        }
    };

    data.name = j.value("name", "Unknown");
    data.discovery_method = j.value("discovery_method", "");
    data.discovery_year = j.value("discovery_year", 0);

    // Host star
    if (j.contains("host_star")) {
        const auto& star = j["host_star"];
        data.host_star.name = star.value("name", "");
        data.host_star.spectral_type = star.value("spectral_type", "");
        parseMeasuredField(star, "effective_temp_k", data.host_star.effective_temp_k, DataSource::NASA_TAP);
        parseMeasuredField(star, "radius_solar", data.host_star.radius_solar, DataSource::NASA_TAP);
        parseMeasuredField(star, "mass_solar", data.host_star.mass_solar, DataSource::NASA_TAP);
        parseMeasuredField(star, "luminosity_solar", data.host_star.luminosity_solar, DataSource::NASA_TAP);
        parseMeasuredField(star, "metallicity", data.host_star.metallicity, DataSource::CDS_VIZIER);
        parseMeasuredField(star, "distance_pc", data.host_star.distance_pc, DataSource::GAIA);
        parseMeasuredField(star, "age_gyr", data.host_star.age_gyr, DataSource::CDS_VIZIER);
        parseMeasuredField(star, "ra_deg", data.host_star.ra_deg, DataSource::GAIA);
        parseMeasuredField(star, "dec_deg", data.host_star.dec_deg, DataSource::GAIA);
    }

    // Orbital parameters
    if (j.contains("orbital")) {
        const auto& orb = j["orbital"];
        parseMeasuredField(orb, "period_days", data.orbital_period_days, DataSource::NASA_TAP);
        parseMeasuredField(orb, "semi_major_axis_au", data.semi_major_axis_au, DataSource::NASA_TAP);
        parseMeasuredField(orb, "eccentricity", data.eccentricity, DataSource::NASA_TAP);
        parseMeasuredField(orb, "inclination_deg", data.inclination_deg, DataSource::NASA_TAP);
        parseMeasuredField(orb, "omega_deg", data.omega_deg, DataSource::NASA_TAP);
    }

    // Physical parameters
    if (j.contains("physical")) {
        const auto& phys = j["physical"];
        parseMeasuredField(phys, "mass_earth", data.mass_earth, DataSource::NASA_TAP);
        parseMeasuredField(phys, "mass_jupiter", data.mass_jupiter, DataSource::NASA_TAP);
        parseMeasuredField(phys, "radius_earth", data.radius_earth, DataSource::NASA_TAP);
        parseMeasuredField(phys, "radius_jupiter", data.radius_jupiter, DataSource::NASA_TAP);
        parseMeasuredField(phys, "density_gcc", data.density_gcc, DataSource::NASA_TAP);
        parseMeasuredField(phys, "equilibrium_temp_k", data.equilibrium_temp_k, DataSource::NASA_TAP);
        parseMeasuredField(phys, "surface_gravity_g", data.surface_gravity_g, DataSource::CALCULATED);
    }

    // Atmosphere
    if (j.contains("atmosphere")) {
        const auto& atmo = j["atmosphere"];
        parseMeasuredField(atmo, "surface_pressure_atm", data.surface_pressure_atm, DataSource::AI_INFERRED);
        parseMeasuredField(atmo, "albedo", data.albedo, DataSource::AI_INFERRED);
        parseMeasuredField(atmo, "greenhouse_effect", data.greenhouse_effect, DataSource::AI_INFERRED);
        parseMeasuredString(atmo, "atmosphere_composition", data.atmosphere_composition, DataSource::AI_INFERRED);
        parseMeasuredField(atmo, "ocean_coverage_fraction", data.ocean_coverage_fraction, DataSource::AI_INFERRED);
        parseMeasuredField(atmo, "cloud_coverage_fraction", data.cloud_coverage_fraction, DataSource::AI_INFERRED);
        parseMeasuredField(atmo, "ice_coverage_fraction", data.ice_coverage_fraction, DataSource::AI_INFERRED);
    }

    // Rendering
    if (j.contains("rendering")) {
        const auto& rend = j["rendering"];
        parseMeasuredString(rend, "biome_classification", data.biome_classification, DataSource::AI_INFERRED);
        parseMeasuredString(rend, "surface_color_hint", data.surface_color_hint, DataSource::AI_INFERRED);
    }

    // Classification
    if (j.contains("classification")) {
        const auto& cls = j["classification"];
        parseMeasuredString(cls, "planet_type", data.planet_type, DataSource::CALCULATED);
        parseMeasuredField(cls, "hz_distance", data.habitable_zone_distance, DataSource::CALCULATED);
        parseMeasuredField(cls, "earth_similarity_index", data.earth_similarity_index, DataSource::CALCULATED);
    }

    return data;
}

}  // namespace astrocore
