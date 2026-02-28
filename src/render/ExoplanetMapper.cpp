#include "render/ExoplanetMapper.hpp"
#include <algorithm>
#include <cmath>
#include <optional>

namespace astrocore {

// ─── Classification ──────────────────────────────────────────────────────────

PlanetCategory ExoplanetMapper::classify(const ExoplanetData& data) {
    const double mass   = data.mass_earth.hasValue()         ? data.mass_earth.value         : 0.0;
    const double radius = data.radius_earth.hasValue()       ? data.radius_earth.value       : 1.0;
    const double temp   = data.equilibrium_temp_k.hasValue() ? data.equilibrium_temp_k.value : 250.0;

    if (data.planet_type.hasValue()) {
        const auto& pt = data.planet_type.value;
        if (pt.find("Gas Giant") != std::string::npos)
            return temp > 1200.0 ? PlanetCategory::HotJupiter : PlanetCategory::GasGiant;
        if (pt.find("Ice Giant") != std::string::npos || pt.find("Mini-Neptune") != std::string::npos)
            return PlanetCategory::IceGiant;
    }

    if (mass > 50.0 || radius > 6.0)
        return temp > 1200.0 ? PlanetCategory::HotJupiter : PlanetCategory::GasGiant;
    if (mass > 10.0 || radius > 3.0)
        return PlanetCategory::IceGiant;
    if (temp > 1800.0) return PlanetCategory::LavaWorld;
    if (temp > 800.0)  return PlanetCategory::HotRocky;
    if (temp < 200.0)  return PlanetCategory::IceWorld;

    if (data.ocean_coverage_fraction.hasValue() && data.ocean_coverage_fraction.value > 0.5)
        return PlanetCategory::OceanWorld;

    if (temp >= 220.0 && temp <= 370.0)
        return PlanetCategory::Terrestrial;

    return PlanetCategory::RockyDesert;
}

// ─── Category base configurations ────────────────────────────────────────────

static PlanetParams baseForCategory(PlanetCategory cat) {
    PlanetParams p;
    switch (cat) {
    case PlanetCategory::LavaWorld:
        p.rockColor        = {0.70f, 0.15f, 0.02f};
        p.sandColor        = {0.90f, 0.40f, 0.10f};
        p.treeColor        = {0.80f, 0.20f, 0.00f};
        p.waterLevel       = 0.0f;
        p.cloudsDensity    = 0.05f;
        p.atmosphereColor  = {0.80f, 0.30f, 0.05f};
        p.atmosphereDensity = 0.5f;
        p.noiseStrength    = 0.35f;
        p.ridgedStrength   = 0.40f;
        p.craterStrength   = 0.20f;
        p.terrainScale     = 1.0f;
        p.sunIntensity     = 4.5f;
        p.sunColor         = {1.0f, 0.80f, 0.50f};
        break;
    case PlanetCategory::HotRocky:
        p.rockColor        = {0.45f, 0.25f, 0.15f};
        p.sandColor        = {0.75f, 0.55f, 0.30f};
        p.treeColor        = {0.60f, 0.40f, 0.20f};
        p.waterLevel       = 0.0f;
        p.cloudsDensity    = 0.10f;
        p.atmosphereColor  = {0.70f, 0.55f, 0.30f};
        p.atmosphereDensity = 0.2f;
        p.noiseStrength    = 0.30f;
        p.craterStrength   = 0.30f;
        p.terrainScale     = 0.9f;
        break;
    case PlanetCategory::HotJupiter:
        p.bandingStrength  = 0.85f;
        p.bandingFrequency = 18.0f;
        p.cloudsDensity    = 0.90f;
        p.cloudsScale      = 2.0f;
        p.cloudColor       = {0.95f, 0.80f, 0.60f};
        p.atmosphereColor  = {0.80f, 0.60f, 0.30f};
        p.atmosphereDensity = 0.9f;
        p.waterColorDeep   = {0.60f, 0.40f, 0.20f};
        p.waterColorSurface = {0.80f, 0.60f, 0.35f};
        p.noiseStrength    = 0.15f;
        p.waterLevel       = 0.0f;
        p.sunIntensity     = 5.0f;
        p.sunColor         = {1.0f, 0.90f, 0.70f};
        break;
    case PlanetCategory::GasGiant:
        p.bandingStrength  = 0.75f;
        p.bandingFrequency = 14.0f;
        p.cloudsDensity    = 0.80f;
        p.cloudsScale      = 2.5f;
        p.cloudColor       = {0.90f, 0.88f, 0.80f};
        p.atmosphereColor  = {0.60f, 0.55f, 0.80f};
        p.atmosphereDensity = 0.8f;
        p.waterColorDeep   = {0.50f, 0.45f, 0.60f};
        p.waterColorSurface = {0.70f, 0.65f, 0.80f};
        p.noiseStrength    = 0.10f;
        p.waterLevel       = 0.0f;
        break;
    case PlanetCategory::IceGiant:
        p.atmosphereColor  = {0.30f, 0.60f, 0.90f};
        p.atmosphereDensity = 0.7f;
        p.cloudsDensity    = 0.60f;
        p.bandingStrength  = 0.30f;
        p.bandingFrequency = 10.0f;
        p.iceColor         = {0.70f, 0.85f, 1.00f};
        p.polarCapSize     = 0.60f;
        p.waterColorDeep   = {0.10f, 0.30f, 0.60f};
        p.waterColorSurface = {0.20f, 0.50f, 0.80f};
        p.noiseStrength    = 0.20f;
        break;
    case PlanetCategory::IceWorld:
        p.iceColor         = {0.88f, 0.93f, 0.98f};
        p.rockColor        = {0.50f, 0.50f, 0.55f};
        p.sandColor        = {0.70f, 0.75f, 0.80f};
        p.treeColor        = {0.55f, 0.60f, 0.65f};
        p.polarCapSize     = 0.85f;
        p.cloudsDensity    = 0.30f;
        p.atmosphereColor  = {0.40f, 0.50f, 0.70f};
        p.atmosphereDensity = 0.15f;
        p.noiseStrength    = 0.25f;
        p.craterStrength   = 0.40f;
        p.waterLevel       = 0.0f;
        break;
    case PlanetCategory::OceanWorld:
        p.waterLevel       = 0.45f;
        p.waterColorDeep   = {0.01f, 0.04f, 0.18f};
        p.waterColorSurface = {0.03f, 0.15f, 0.35f};
        p.cloudsDensity    = 0.75f;
        p.atmosphereColor  = {0.05f, 0.25f, 0.85f};
        p.atmosphereDensity = 0.45f;
        p.polarCapSize     = 0.10f;
        p.continentScale   = 0.30f;
        p.noiseStrength    = 0.15f;
        break;
    case PlanetCategory::Terrestrial:
        p.waterLevel       = 0.20f;
        p.waterColorDeep   = {0.01f, 0.05f, 0.15f};
        p.waterColorSurface = {0.02f, 0.12f, 0.27f};
        p.treeColor        = {0.02f, 0.10f, 0.04f};
        p.sandColor        = {0.85f, 0.75f, 0.50f};
        p.rockColor        = {0.25f, 0.22f, 0.18f};
        p.cloudsDensity    = 0.50f;
        p.atmosphereColor  = {0.05f, 0.30f, 0.90f};
        p.atmosphereDensity = 0.3f;
        p.polarCapSize     = 0.15f;
        p.continentScale   = 0.50f;
        p.noiseStrength    = 0.20f;
        p.craterStrength   = 0.02f;
        break;
    case PlanetCategory::RockyDesert:
    default:
        p.rockColor        = {0.45f, 0.32f, 0.22f};
        p.sandColor        = {0.75f, 0.60f, 0.38f};
        p.treeColor        = {0.55f, 0.40f, 0.25f};
        p.waterLevel       = 0.0f;
        p.cloudsDensity    = 0.05f;
        p.atmosphereColor  = {0.60f, 0.45f, 0.30f};
        p.atmosphereDensity = 0.10f;
        p.craterStrength   = 0.35f;
        p.noiseStrength    = 0.25f;
        break;
    }
    return p;
}

// ─── Physics pass ────────────────────────────────────────────────────────────

PlanetParams ExoplanetMapper::toPlanetParams(const ExoplanetData&  data,
                                              std::set<std::string>* physicsFields,
                                              const PlanetParams*   analogBase) {
    PlanetCategory cat = classify(data);

    // Start from analog base if provided, otherwise generic category defaults
    PlanetParams p = analogBase ? *analogBase : baseForCategory(cat);

    auto markField = [&](const std::string& name) {
        if (physicsFields) physicsFields->insert(name);
    };

    // Visual radius: map Earth radii → renderer units [0.8, 5.0]
    if (data.radius_earth.hasValue()) {
        p.radius = static_cast<float>(std::clamp(data.radius_earth.value * 0.45, 0.8, 5.0));
        markField("radius");
    }

    // Atmosphere colour from measured composition
    if (data.atmosphere_composition.hasValue()) {
        try {
            auto atmo = nlohmann::json::parse(data.atmosphere_composition.value);
            double co2 = atmo.value("CO2",   0.0);
            double n2  = atmo.value("N2",    0.0);
            double h2  = atmo.value("H2",    0.0) + atmo.value("H2/He", 0.0);
            double ch4 = atmo.value("CH4",   0.0);
            double so2 = atmo.value("SO2",   0.0);

            if      (so2 > 0.3)  p.atmosphereColor = {0.70f, 0.40f, 0.20f};
            else if (co2 > 0.5)  p.atmosphereColor = {0.40f, 0.35f, 0.30f};
            else if (h2  > 0.5)  p.atmosphereColor = {0.50f, 0.45f, 0.35f};
            else if (ch4 > 0.1)  p.atmosphereColor = {0.05f, 0.55f, 0.85f};
            else if (n2  > 0.5)  p.atmosphereColor = {0.05f, 0.25f, 0.85f};

            markField("atmosphereColor");
        } catch (...) {}
    }

    // Atmosphere density from measured surface pressure (log-scale)
    if (data.surface_pressure_atm.hasValue()) {
        double pressure = data.surface_pressure_atm.value;
        float density = static_cast<float>(
            std::clamp(0.3 * std::log10(pressure + 1.0) + 0.05, 0.0, 1.0));
        p.atmosphereDensity = density;
        markField("atmosphereDensity");
    }

    // Water level from measured ocean coverage [0,1] → [0, 0.55]
    if (data.ocean_coverage_fraction.hasValue()) {
        p.waterLevel = static_cast<float>(
            std::clamp(data.ocean_coverage_fraction.value * 0.55, 0.0, 0.55));
        markField("waterLevel");
    }

    // Cloud density directly from measured cloud coverage
    if (data.cloud_coverage_fraction.hasValue()) {
        p.cloudsDensity = static_cast<float>(
            std::clamp(data.cloud_coverage_fraction.value, 0.0, 1.0));
        markField("cloudsDensity");
    }

    // Polar caps from measured ice coverage
    if (data.ice_coverage_fraction.hasValue()) {
        p.polarCapSize = static_cast<float>(
            std::clamp(data.ice_coverage_fraction.value * 0.9, 0.0, 0.9));
        markField("polarCapSize");
    }

    // Sun intensity from measured albedo
    if (data.albedo.hasValue()) {
        float albedo = static_cast<float>(std::clamp(data.albedo.value, 0.0, 1.0));
        p.sunIntensity = 2.0f + albedo * 3.0f;
        markField("sunIntensity");
    }

    // Terrain ruggedness from measured surface gravity
    if (data.surface_gravity_g.hasValue()) {
        double g = data.surface_gravity_g.value;
        float ruggedness = static_cast<float>(std::clamp(0.4 / (g + 0.2), 0.05, 0.65));

        const bool solidSurface =
            cat == PlanetCategory::Terrestrial  ||
            cat == PlanetCategory::RockyDesert  ||
            cat == PlanetCategory::HotRocky     ||
            cat == PlanetCategory::IceWorld     ||
            cat == PlanetCategory::LavaWorld;

        if (solidSurface) {
            p.noiseStrength = ruggedness;
            markField("noiseStrength");
        }

        bool thinAtmo = !data.surface_pressure_atm.hasValue() ||
                         data.surface_pressure_atm.value < 0.1;
        if (g < 0.4 && thinAtmo) {
            p.craterStrength = std::min(p.craterStrength + 0.25f, 0.85f);
            markField("craterStrength");
        }
    }

    return p;
}

// ─── AI override pass (fill-empty-slots only) ────────────────────────────────

void ExoplanetMapper::applyAIRenderOverrides(PlanetParams&                params,
                                              const nlohmann::json&        aiJson,
                                              const std::set<std::string>& skipFields) {
    if (aiJson.empty()) return;

    auto skip = [&](const std::string& k) {
        return skipFields.count(k) > 0;
    };

    auto getF = [&](const std::string& key) -> std::optional<float> {
        if (aiJson.contains(key) && aiJson[key].is_number())
            return aiJson[key].get<float>();
        return std::nullopt;
    };

    auto getV3 = [&](const std::string& key) -> std::optional<glm::vec3> {
        if (aiJson.contains(key) && aiJson[key].is_array() && aiJson[key].size() == 3)
            return glm::vec3{aiJson[key][0].get<float>(),
                             aiJson[key][1].get<float>(),
                             aiJson[key][2].get<float>()};
        return std::nullopt;
    };

    // Terrain — only fill if physics pass didn't set from real data
    if (!skip("noiseStrength"))     { if (auto v = getF("noiseStrength"))      params.noiseStrength     = *v; }
    if (!skip("ridgedStrength"))    { if (auto v = getF("ridgedStrength"))      params.ridgedStrength    = *v; }
    if (!skip("craterStrength"))    { if (auto v = getF("craterStrength"))      params.craterStrength    = *v; }
    if (!skip("continentScale"))    { if (auto v = getF("continentScale"))      params.continentScale    = *v; }
    if (!skip("terrainScale"))      { if (auto v = getF("terrainScale"))        params.terrainScale      = *v; }
    if (!skip("domainWarpStrength")){ if (auto v = getF("domainWarpStrength"))  params.domainWarpStrength= *v; }

    // Surface levels
    if (!skip("waterLevel"))        { if (auto v = getF("waterLevel"))          params.waterLevel        = *v; }
    if (!skip("polarCapSize"))      { if (auto v = getF("polarCapSize"))        params.polarCapSize      = *v; }
    if (!skip("sandLevel"))         { if (auto v = getF("sandLevel"))           params.sandLevel         = *v; }
    if (!skip("treeLevel"))         { if (auto v = getF("treeLevel"))           params.treeLevel         = *v; }
    if (!skip("rockLevel"))         { if (auto v = getF("rockLevel"))           params.rockLevel         = *v; }
    if (!skip("iceLevel"))          { if (auto v = getF("iceLevel"))            params.iceLevel          = *v; }

    // Banding (gas giants)
    if (!skip("bandingStrength"))   { if (auto v = getF("bandingStrength"))     params.bandingStrength   = *v; }
    if (!skip("bandingFrequency"))  { if (auto v = getF("bandingFrequency"))    params.bandingFrequency  = *v; }

    // Clouds
    if (!skip("cloudsDensity"))     { if (auto v = getF("cloudsDensity"))       params.cloudsDensity     = *v; }
    if (!skip("cloudsScale"))       { if (auto v = getF("cloudsScale"))         params.cloudsScale       = *v; }
    if (!skip("cloudAltitude"))     { if (auto v = getF("cloudAltitude"))       params.cloudAltitude     = *v; }
    if (!skip("cloudThickness"))    { if (auto v = getF("cloudThickness"))      params.cloudThickness    = *v; }

    // Atmosphere
    if (!skip("atmosphereDensity")) { if (auto v = getF("atmosphereDensity"))   params.atmosphereDensity = *v; }

    // Lighting
    if (!skip("sunIntensity"))      { if (auto v = getF("sunIntensity"))        params.sunIntensity      = *v; }
    if (!skip("ambientLight"))      { if (auto v = getF("ambientLight"))        params.ambientLight      = *v; }

    // Colours
    if (!skip("atmosphereColor"))   { if (auto v = getV3("atmosphereColor"))    params.atmosphereColor   = *v; }
    if (!skip("waterColorDeep"))    { if (auto v = getV3("waterColorDeep"))     params.waterColorDeep    = *v; }
    if (!skip("waterColorSurface")) { if (auto v = getV3("waterColorSurface"))  params.waterColorSurface = *v; }
    if (!skip("sandColor"))         { if (auto v = getV3("sandColor"))          params.sandColor         = *v; }
    if (!skip("treeColor"))         { if (auto v = getV3("treeColor"))          params.treeColor         = *v; }
    if (!skip("rockColor"))         { if (auto v = getV3("rockColor"))          params.rockColor         = *v; }
    if (!skip("iceColor"))          { if (auto v = getV3("iceColor"))           params.iceColor          = *v; }
    if (!skip("cloudColor"))        { if (auto v = getV3("cloudColor"))         params.cloudColor        = *v; }
    if (!skip("sunColor"))          { if (auto v = getV3("sunColor"))           params.sunColor          = *v; }
}

// ─── Full pipeline ────────────────────────────────────────────────────────────

PlanetParams ExoplanetMapper::toRenderParams(const ExoplanetData&  data,
                                              const nlohmann::json& aiJson,
                                              AnalogMatch*          analogMatchOut) {
    // Find closest solar-system analog if we have the physical dimensions
    const PlanetParams* analogBase  = nullptr;
    AnalogMatch         bestAnalog;

    if (data.mass_earth.hasValue() && data.radius_earth.hasValue() &&
        data.equilibrium_temp_k.hasValue()) {

        auto found = SolarSystemDatabase::instance().findClosestAnalog(
            data.mass_earth.value,
            data.radius_earth.value,
            data.equilibrium_temp_k.value,
            0.35f);  // minimum similarity threshold

        if (found) {
            bestAnalog = *found;
            analogBase = &found->entry->visualParams;
            if (analogMatchOut) *analogMatchOut = bestAnalog;
        }
    }

    // Pass 1: physics derivations (tracks which fields came from real data)
    std::set<std::string> physicsFields;
    PlanetParams params = toPlanetParams(data, &physicsFields, analogBase);

    // Pass 2: AI fills only unknown fields
    applyAIRenderOverrides(params, aiJson, physicsFields);

    return params;
}

// ─── String helpers ───────────────────────────────────────────────────────────

std::string ExoplanetMapper::categoryName(PlanetCategory cat) {
    switch (cat) {
    case PlanetCategory::LavaWorld:    return "Lava World";
    case PlanetCategory::HotRocky:    return "Hot Rocky";
    case PlanetCategory::HotJupiter:  return "Hot Jupiter";
    case PlanetCategory::GasGiant:    return "Gas Giant";
    case PlanetCategory::IceGiant:    return "Ice Giant";
    case PlanetCategory::IceWorld:    return "Ice World";
    case PlanetCategory::OceanWorld:  return "Ocean World";
    case PlanetCategory::Terrestrial: return "Terrestrial";
    case PlanetCategory::RockyDesert: return "Rocky Desert";
    default:                          return "Unknown";
    }
}

}  // namespace astrocore
