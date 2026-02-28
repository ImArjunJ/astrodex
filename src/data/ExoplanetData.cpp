#include "data/ExoplanetData.hpp"
#include <cmath>

namespace astrocore {

void ExoplanetData::calculateDerivedValues() {
    // Calculate mass in Jupiter if we have Earth mass
    if (mass_earth.hasValue() && !mass_jupiter.hasValue()) {
        mass_jupiter.value = mass_earth.value / 317.8;
        mass_jupiter.source = DataSource::CALCULATED;
    }

    // Calculate mass in Earth if we have Jupiter mass
    if (mass_jupiter.hasValue() && !mass_earth.hasValue()) {
        mass_earth.value = mass_jupiter.value * 317.8;
        mass_earth.source = DataSource::CALCULATED;
    }

    // Calculate radius in Jupiter if we have Earth radius
    if (radius_earth.hasValue() && !radius_jupiter.hasValue()) {
        radius_jupiter.value = radius_earth.value / 11.2;
        radius_jupiter.source = DataSource::CALCULATED;
    }

    // Calculate radius in Earth if we have Jupiter radius
    if (radius_jupiter.hasValue() && !radius_earth.hasValue()) {
        radius_earth.value = radius_jupiter.value * 11.2;
        radius_earth.source = DataSource::CALCULATED;
    }

    // Calculate density from mass and radius
    if (mass_earth.hasValue() && radius_earth.hasValue() && !density_gcc.hasValue()) {
        // Earth density = 5.51 g/cm³
        // density = mass / volume, volume scales as r³
        density_gcc.value = 5.51 * mass_earth.value / std::pow(radius_earth.value, 3.0);
        density_gcc.source = DataSource::CALCULATED;
    }

    // Calculate surface gravity
    if (mass_earth.hasValue() && radius_earth.hasValue() && !surface_gravity_g.hasValue()) {
        // g = GM/r² -> g_planet/g_earth = (M/M_earth) / (R/R_earth)²
        surface_gravity_g.value = mass_earth.value / std::pow(radius_earth.value, 2.0);
        surface_gravity_g.source = DataSource::CALCULATED;
    }

    // Calculate equilibrium temperature if not provided
    if (!equilibrium_temp_k.hasValue() &&
        host_star.effective_temp_k.hasValue() &&
        host_star.radius_solar.hasValue() &&
        semi_major_axis_au.hasValue()) {
        // T_eq = T_star * sqrt(R_star / (2 * a)) * (1 - A)^0.25
        // Assume albedo of 0.3 if not known
        double A = albedo.hasValue() ? albedo.value : 0.3;
        double R_star_au = host_star.radius_solar.value * 0.00465047;  // Solar radii to AU
        equilibrium_temp_k.value = host_star.effective_temp_k.value *
            std::sqrt(R_star_au / (2.0 * semi_major_axis_au.value)) *
            std::pow(1.0 - A, 0.25);
        equilibrium_temp_k.source = DataSource::CALCULATED;
    }

    // Calculate habitable zone distance
    if (host_star.luminosity_solar.hasValue() && semi_major_axis_au.hasValue()) {
        // Conservative HZ inner edge: sqrt(L) * 0.95 AU
        // Conservative HZ outer edge: sqrt(L) * 1.67 AU
        double L = host_star.luminosity_solar.value;
        double hz_inner = std::sqrt(L) * 0.95;
        double hz_outer = std::sqrt(L) * 1.67;
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

nlohmann::json ExoplanetData::toJson() const {
    nlohmann::json j;

    j["name"] = name;
    j["discovery_method"] = discovery_method;
    j["discovery_year"] = discovery_year;

    // Host star
    j["host_star"]["name"] = host_star.name;
    if (host_star.effective_temp_k.hasValue())
        j["host_star"]["effective_temp_k"] = host_star.effective_temp_k.value;
    if (host_star.radius_solar.hasValue())
        j["host_star"]["radius_solar"] = host_star.radius_solar.value;
    if (host_star.mass_solar.hasValue())
        j["host_star"]["mass_solar"] = host_star.mass_solar.value;
    j["host_star"]["spectral_type"] = host_star.spectral_type;

    // Orbital parameters
    if (orbital_period_days.hasValue())
        j["orbital"]["period_days"] = orbital_period_days.value;
    if (semi_major_axis_au.hasValue())
        j["orbital"]["semi_major_axis_au"] = semi_major_axis_au.value;
    if (eccentricity.hasValue())
        j["orbital"]["eccentricity"] = eccentricity.value;

    // Physical parameters
    if (mass_earth.hasValue()) {
        j["physical"]["mass_earth"] = mass_earth.value;
        j["physical"]["mass_earth_source"] = dataSourceToString(mass_earth.source);
    }
    if (radius_earth.hasValue()) {
        j["physical"]["radius_earth"] = radius_earth.value;
        j["physical"]["radius_earth_source"] = dataSourceToString(radius_earth.source);
    }
    if (density_gcc.hasValue())
        j["physical"]["density_gcc"] = density_gcc.value;
    if (equilibrium_temp_k.hasValue())
        j["physical"]["equilibrium_temp_k"] = equilibrium_temp_k.value;

    // Planet type
    if (planet_type.hasValue())
        j["classification"]["planet_type"] = planet_type.value;
    if (habitable_zone_distance.hasValue())
        j["classification"]["hz_distance"] = habitable_zone_distance.value;

    return j;
}

ExoplanetData ExoplanetData::fromJson(const nlohmann::json& j) {
    ExoplanetData data;

    data.name = j.value("name", "Unknown");
    data.discovery_method = j.value("discovery_method", "");
    data.discovery_year = j.value("discovery_year", 0);

    // Host star
    if (j.contains("host_star")) {
        const auto& star = j["host_star"];
        data.host_star.name = star.value("name", "");
        if (star.contains("effective_temp_k")) {
            data.host_star.effective_temp_k.value = star["effective_temp_k"].get<double>();
            data.host_star.effective_temp_k.source = DataSource::NASA_TAP;
        }
        if (star.contains("radius_solar")) {
            data.host_star.radius_solar.value = star["radius_solar"].get<double>();
            data.host_star.radius_solar.source = DataSource::NASA_TAP;
        }
        if (star.contains("mass_solar")) {
            data.host_star.mass_solar.value = star["mass_solar"].get<double>();
            data.host_star.mass_solar.source = DataSource::NASA_TAP;
        }
        data.host_star.spectral_type = star.value("spectral_type", "");
    }

    // Orbital parameters
    if (j.contains("orbital")) {
        const auto& orb = j["orbital"];
        if (orb.contains("period_days")) {
            data.orbital_period_days.value = orb["period_days"].get<double>();
            data.orbital_period_days.source = DataSource::NASA_TAP;
        }
        if (orb.contains("semi_major_axis_au")) {
            data.semi_major_axis_au.value = orb["semi_major_axis_au"].get<double>();
            data.semi_major_axis_au.source = DataSource::NASA_TAP;
        }
        if (orb.contains("eccentricity")) {
            data.eccentricity.value = orb["eccentricity"].get<double>();
            data.eccentricity.source = DataSource::NASA_TAP;
        }
    }

    // Physical parameters
    if (j.contains("physical")) {
        const auto& phys = j["physical"];
        if (phys.contains("mass_earth")) {
            data.mass_earth.value = phys["mass_earth"].get<double>();
            data.mass_earth.source = DataSource::NASA_TAP;
        }
        if (phys.contains("radius_earth")) {
            data.radius_earth.value = phys["radius_earth"].get<double>();
            data.radius_earth.source = DataSource::NASA_TAP;
        }
        if (phys.contains("density_gcc")) {
            data.density_gcc.value = phys["density_gcc"].get<double>();
            data.density_gcc.source = DataSource::NASA_TAP;
        }
        if (phys.contains("equilibrium_temp_k")) {
            data.equilibrium_temp_k.value = phys["equilibrium_temp_k"].get<double>();
            data.equilibrium_temp_k.source = DataSource::NASA_TAP;
        }
    }

    return data;
}

}  // namespace astrocore
