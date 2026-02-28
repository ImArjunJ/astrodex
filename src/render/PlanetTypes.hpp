#pragma once

#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <cctype>

namespace astrocore {

enum class PlanetType {
    EarthLike,      // Temperate, water, vegetation
    OceanWorld,     // Mostly water covered
    IceWorld,       // Frozen, thin atmosphere
    DesertWorld,    // Hot, arid, no oceans
    LavaWorld,      // Extreme heat, molten surface
    SuperEarth,     // Larger rocky, thick atmosphere
    MiniNeptune,    // Small gas planet, hazy
    GasGiant,       // Jupiter-like, banded
    HotJupiter,     // Extreme temp gas giant
    Unknown
};

// Convert string to PlanetType
inline PlanetType stringToPlanetType(const std::string& str) {
    static const std::unordered_map<std::string, PlanetType> map = {
        {"earth-like", PlanetType::EarthLike},
        {"earthlike", PlanetType::EarthLike},
        {"terrestrial", PlanetType::EarthLike},
        {"ocean world", PlanetType::OceanWorld},
        {"ocean", PlanetType::OceanWorld},
        {"water world", PlanetType::OceanWorld},
        {"ice world", PlanetType::IceWorld},
        {"ice", PlanetType::IceWorld},
        {"frozen", PlanetType::IceWorld},
        {"desert", PlanetType::DesertWorld},
        {"desert world", PlanetType::DesertWorld},
        {"arid", PlanetType::DesertWorld},
        {"lava world", PlanetType::LavaWorld},
        {"lava", PlanetType::LavaWorld},
        {"molten", PlanetType::LavaWorld},
        {"volcanic", PlanetType::LavaWorld},
        {"super-earth", PlanetType::SuperEarth},
        {"super earth", PlanetType::SuperEarth},
        {"rocky", PlanetType::SuperEarth},
        {"mini-neptune", PlanetType::MiniNeptune},
        {"mini neptune", PlanetType::MiniNeptune},
        {"sub-neptune", PlanetType::MiniNeptune},
        {"gas giant", PlanetType::GasGiant},
        {"jovian", PlanetType::GasGiant},
        {"jupiter-like", PlanetType::GasGiant},
        {"hot jupiter", PlanetType::HotJupiter},
        {"hot jovian", PlanetType::HotJupiter},
    };

    std::string lower = str;
    for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    auto it = map.find(lower);
    return (it != map.end()) ? it->second : PlanetType::Unknown;
}

// Infer planet type from physical parameters
inline PlanetType inferPlanetType(double mass_earth, double radius_earth, double temp_k) {
    // Gas giants: large radius, low density implied
    if (radius_earth > 6.0) {
        return (temp_k > 1000) ? PlanetType::HotJupiter : PlanetType::GasGiant;
    }

    // Mini-Neptunes: intermediate size
    if (radius_earth > 2.5 && radius_earth <= 6.0) {
        return PlanetType::MiniNeptune;
    }

    // Rocky planets: based on temperature
    if (temp_k > 800) {
        return PlanetType::LavaWorld;
    }
    if (temp_k > 380) {
        return PlanetType::DesertWorld;
    }
    if (temp_k < 170) {  // Really frozen
        return PlanetType::IceWorld;
    }

    // Temperate/habitable zone: 170K - 380K
    if (radius_earth > 1.8) {
        return PlanetType::SuperEarth;
    }

    return PlanetType::EarthLike;
}

// Planet profile presets (colors and rendering parameters)
struct PlanetProfile {
    glm::vec3 oceanColor;
    glm::vec3 lowlandColor;
    glm::vec3 highlandColor;
    glm::vec3 mountainColor;
    glm::vec3 snowColor;
    glm::vec3 atmosphereColor;  // Rayleigh coefficients
    float oceanLevel;
    float atmosphereHeight;
    float atmosphereDensity;
    float terrainAmplitude;
};

inline PlanetProfile getProfile(PlanetType type) {
    switch (type) {
        case PlanetType::EarthLike:
            return {
                .oceanColor = {0.01f, 0.08f, 0.35f},       // Deep ocean blue
                .lowlandColor = {0.08f, 0.35f, 0.08f},     // Forest green
                .highlandColor = {0.45f, 0.35f, 0.2f},     // Brown/tan
                .mountainColor = {0.4f, 0.38f, 0.35f},     // Gray rock
                .snowColor = {0.92f, 0.94f, 0.98f},        // White snow
                .atmosphereColor = {5.5e-6f, 13.0e-6f, 22.4e-6f},
                .oceanLevel = 0.45f,                        // 60%+ ocean coverage
                .atmosphereHeight = 0.15f,
                .atmosphereDensity = 1.0f,
                .terrainAmplitude = 0.12f                   // More terrain variation
            };

        case PlanetType::OceanWorld:
            return {
                .oceanColor = {0.01f, 0.12f, 0.35f},
                .lowlandColor = {0.1f, 0.35f, 0.15f},
                .highlandColor = {0.3f, 0.25f, 0.2f},
                .mountainColor = {0.4f, 0.38f, 0.35f},
                .snowColor = {0.9f, 0.95f, 1.0f},
                .atmosphereColor = {5.0e-6f, 12.0e-6f, 20.0e-6f},
                .oceanLevel = 0.7f,
                .atmosphereHeight = 0.18f,
                .atmosphereDensity = 1.5f,
                .terrainAmplitude = 0.05f
            };

        case PlanetType::IceWorld:
            return {
                .oceanColor = {0.4f, 0.5f, 0.6f},
                .lowlandColor = {0.7f, 0.75f, 0.8f},
                .highlandColor = {0.8f, 0.82f, 0.85f},
                .mountainColor = {0.6f, 0.65f, 0.7f},
                .snowColor = {0.95f, 0.97f, 1.0f},
                .atmosphereColor = {4.0e-6f, 8.0e-6f, 12.0e-6f},
                .oceanLevel = 0.2f,
                .atmosphereHeight = 0.08f,
                .atmosphereDensity = 0.4f,
                .terrainAmplitude = 0.06f
            };

        case PlanetType::DesertWorld:
            return {
                .oceanColor = {0.6f, 0.5f, 0.3f},
                .lowlandColor = {0.7f, 0.55f, 0.35f},
                .highlandColor = {0.8f, 0.6f, 0.4f},
                .mountainColor = {0.6f, 0.5f, 0.45f},
                .snowColor = {0.9f, 0.85f, 0.8f},
                .atmosphereColor = {8.0e-6f, 6.0e-6f, 4.0e-6f},
                .oceanLevel = 0.0f,
                .atmosphereHeight = 0.1f,
                .atmosphereDensity = 0.8f,
                .terrainAmplitude = 0.12f
            };

        case PlanetType::LavaWorld:
            return {
                .oceanColor = {0.9f, 0.3f, 0.05f},
                .lowlandColor = {0.15f, 0.1f, 0.08f},
                .highlandColor = {0.25f, 0.2f, 0.18f},
                .mountainColor = {0.3f, 0.25f, 0.2f},
                .snowColor = {0.4f, 0.35f, 0.3f},
                .atmosphereColor = {10.0e-6f, 5.0e-6f, 2.0e-6f},
                .oceanLevel = 0.25f,
                .atmosphereHeight = 0.12f,
                .atmosphereDensity = 1.5f,
                .terrainAmplitude = 0.15f
            };

        case PlanetType::SuperEarth:
            return {
                .oceanColor = {0.03f, 0.18f, 0.45f},
                .lowlandColor = {0.2f, 0.4f, 0.15f},
                .highlandColor = {0.5f, 0.42f, 0.28f},
                .mountainColor = {0.48f, 0.45f, 0.42f},
                .snowColor = {0.92f, 0.94f, 0.98f},
                .atmosphereColor = {6.0e-6f, 14.0e-6f, 24.0e-6f},
                .oceanLevel = 0.4f,
                .atmosphereHeight = 0.2f,
                .atmosphereDensity = 2.0f,
                .terrainAmplitude = 0.06f
            };

        case PlanetType::MiniNeptune:
            return {
                .oceanColor = {0.3f, 0.4f, 0.5f},
                .lowlandColor = {0.35f, 0.45f, 0.55f},
                .highlandColor = {0.4f, 0.5f, 0.6f},
                .mountainColor = {0.45f, 0.55f, 0.65f},
                .snowColor = {0.6f, 0.7f, 0.8f},
                .atmosphereColor = {4.0e-6f, 10.0e-6f, 18.0e-6f},
                .oceanLevel = 0.0f,
                .atmosphereHeight = 0.4f,
                .atmosphereDensity = 3.0f,
                .terrainAmplitude = 0.02f
            };

        case PlanetType::GasGiant:
            return {
                .oceanColor = {0.6f, 0.5f, 0.4f},
                .lowlandColor = {0.7f, 0.6f, 0.5f},
                .highlandColor = {0.8f, 0.75f, 0.65f},
                .mountainColor = {0.55f, 0.5f, 0.45f},
                .snowColor = {0.9f, 0.85f, 0.8f},
                .atmosphereColor = {3.0e-6f, 5.0e-6f, 8.0e-6f},
                .oceanLevel = 0.0f,
                .atmosphereHeight = 0.5f,
                .atmosphereDensity = 4.0f,
                .terrainAmplitude = 0.01f
            };

        case PlanetType::HotJupiter:
            return {
                .oceanColor = {0.4f, 0.25f, 0.15f},
                .lowlandColor = {0.5f, 0.35f, 0.2f},
                .highlandColor = {0.6f, 0.4f, 0.25f},
                .mountainColor = {0.45f, 0.3f, 0.2f},
                .snowColor = {0.7f, 0.5f, 0.35f},
                .atmosphereColor = {12.0e-6f, 8.0e-6f, 4.0e-6f},
                .oceanLevel = 0.0f,
                .atmosphereHeight = 0.6f,
                .atmosphereDensity = 5.0f,
                .terrainAmplitude = 0.01f
            };

        default:
            return {
                .oceanColor = {0.3f, 0.35f, 0.4f},
                .lowlandColor = {0.4f, 0.42f, 0.45f},
                .highlandColor = {0.5f, 0.5f, 0.5f},
                .mountainColor = {0.55f, 0.55f, 0.55f},
                .snowColor = {0.8f, 0.8f, 0.82f},
                .atmosphereColor = {5.0e-6f, 8.0e-6f, 12.0e-6f},
                .oceanLevel = 0.3f,
                .atmosphereHeight = 0.12f,
                .atmosphereDensity = 1.0f,
                .terrainAmplitude = 0.08f
            };
    }
}

}  // namespace astrocore
