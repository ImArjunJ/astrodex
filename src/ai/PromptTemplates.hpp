#pragma once

#include <string>
#include <format>
#include "data/ExoplanetData.hpp"

namespace astrocore::prompts {

constexpr std::string_view SYSTEM_PROMPT = R"(You are an expert astrophysicist and planetary scientist. Your task is to infer missing exoplanet parameters based on known observational data, using established physics, empirical mass-radius relationships, and statistical models from exoplanet research.

IMPORTANT: You must respond ONLY with valid JSON. No explanations outside the JSON.

Use these empirical relationships:
1. Mass-Radius (rocky planets, M < 10 M_Earth): R/R_Earth ≈ (M/M_Earth)^0.27
2. Mass-Radius (Neptune-like): R/R_Earth ≈ 2.5 * (M/M_Earth)^0.1
3. Mass-Radius (gas giants): R/R_Jupiter ≈ (M/M_Jupiter)^-0.04
4. Equilibrium temperature: T_eq = T_star * sqrt(R_star / (2*a)) * (1-A)^0.25
5. Scale height: H = kT / (μg) where μ is mean molecular weight
6. Escape velocity: v_esc = sqrt(2GM/R)

For atmosphere inference:
- Hot Jupiters (T > 1000K): H2/He dominated, possible Na/K detection
- Warm Neptunes (500-1000K): H2/He with possible H2O, CH4
- Super-Earths in HZ (250-350K): Could retain CO2, N2, possibly H2O
- Cold planets (< 250K): Ice-dominated, thin atmospheres

Response format:
{
  "inferred_values": {
    "parameter_name": {
      "value": <number or string>,
      "confidence": "high|medium|low",
      "unit": "<unit string>"
    }
  },
  "reasoning": "<brief scientific explanation>",
  "assumptions": ["<assumption 1>", "<assumption 2>"]
})";

inline std::string buildAtmospherePrompt(const ExoplanetData& data) {
    std::string prompt = "Infer atmospheric properties for this exoplanet:\n\n";

    prompt += "KNOWN DATA:\n";
    prompt += std::format("- Planet name: {}\n", data.name);

    if (data.mass_earth.hasValue()) {
        prompt += std::format("- Mass: {:.2f} Earth masses\n", data.mass_earth.value);
    }
    if (data.radius_earth.hasValue()) {
        prompt += std::format("- Radius: {:.2f} Earth radii\n", data.radius_earth.value);
    }
    if (data.equilibrium_temp_k.hasValue()) {
        prompt += std::format("- Equilibrium temperature: {:.0f} K\n", data.equilibrium_temp_k.value);
    }
    if (data.surface_gravity_g.hasValue()) {
        prompt += std::format("- Surface gravity: {:.2f} g\n", data.surface_gravity_g.value);
    }
    if (data.orbital_period_days.hasValue()) {
        prompt += std::format("- Orbital period: {:.2f} days\n", data.orbital_period_days.value);
    }
    if (data.semi_major_axis_au.hasValue()) {
        prompt += std::format("- Semi-major axis: {:.4f} AU\n", data.semi_major_axis_au.value);
    }
    if (data.host_star.effective_temp_k.hasValue()) {
        prompt += std::format("- Host star temperature: {:.0f} K\n", data.host_star.effective_temp_k.value);
    }
    if (!data.host_star.spectral_type.empty()) {
        prompt += std::format("- Host star spectral type: {}\n", data.host_star.spectral_type);
    }

    prompt += R"(
INFER the following parameters:
1. surface_pressure_atm (atmospheric surface pressure in Earth atmospheres)
2. albedo (geometric albedo, 0-1)
3. atmosphere_composition (primary gases as JSON object with percentages)
4. ocean_coverage_fraction (estimated ocean coverage 0-1, based on temperature and composition)
5. cloud_coverage_fraction (estimated cloud coverage 0-1)

Consider:
- Planet's position relative to habitable zone
- Likely atmospheric escape based on escape velocity and stellar radiation
- Bulk composition implied by mass-radius relationship)";

    return prompt;
}

inline std::string buildRenderHintsPrompt(const ExoplanetData& data) {
    std::string prompt = "Generate visualization hints for this exoplanet:\n\n";

    prompt += std::format("Planet: {}\n", data.name);
    if (data.planet_type.hasValue()) {
        prompt += std::format("Type: {}\n", data.planet_type.value);
    }
    if (data.equilibrium_temp_k.hasValue()) {
        prompt += std::format("Temperature: {:.0f} K\n", data.equilibrium_temp_k.value);
    }
    if (data.mass_earth.hasValue()) {
        prompt += std::format("Mass: {:.2f} Earth masses\n", data.mass_earth.value);
    }

    prompt += R"(
INFER visualization parameters:
1. biome_classification (e.g., "Ice World", "Desert", "Ocean World", "Gas Giant", "Lava World")
2. surface_color_hint (dominant color description like "blue-green", "rust-orange", "white-ice")
3. atmosphere_color_hint (sky/atmosphere color like "blue", "orange-haze", "purple")
4. terrain_type (e.g., "mountainous", "flat", "volcanic", "cratered")

Base your inference on temperature, mass, and likely composition.)";

    return prompt;
}

// ─── Render-params system prompt ─────────────────────────────────────────────
// Claude is asked to produce a JSON object whose keys are PlanetParams field
// names and whose values are floats or [r,g,b] arrays in [0,1].  These are
// merged on top of the physics-based ExoplanetMapper output, so Claude only
// needs to override values it has genuine scientific reason to change.

constexpr std::string_view RENDER_PARAMS_SYSTEM_PROMPT = R"(You are an expert planetary scientist and 3D visualisation specialist.
Given exoplanet observational data you must output a JSON object of visual renderer overrides.

RULES:
- Respond ONLY with a single valid JSON object — no prose, no markdown fences.
- Only include keys you have a genuine scientific basis to override.
- All float values must be in [0, 1] unless the field description says otherwise.
- Colour keys ("atmosphereColor", "waterColorDeep", "waterColorSurface", "sandColor",
  "treeColor", "rockColor", "iceColor", "cloudColor", "sunColor") must be [r, g, b]
  arrays with each component in [0, 1].
- "sunIntensity" is in [0, 8] (solar-luminosity-scaled).
- "bandingFrequency" is in [5, 40] (relevant only for gas giants).

Available keys (subset of PlanetParams):
  Terrain:    noiseStrength, ridgedStrength, craterStrength, continentScale,
              terrainScale, domainWarpStrength
  Levels:     waterLevel, polarCapSize, sandLevel, treeLevel, rockLevel, iceLevel
  Banding:    bandingStrength, bandingFrequency
  Clouds:     cloudsDensity, cloudsScale, cloudAltitude, cloudThickness
  Atmosphere: atmosphereDensity
  Lighting:   sunIntensity, ambientLight
  Colours:    atmosphereColor, waterColorDeep, waterColorSurface, sandColor,
              treeColor, rockColor, iceColor, cloudColor, sunColor

Physical reasoning to follow:
- Temperature 200-370 K + liquid water → blue atmosphere, moderate cloud cover
- Hot Jupiter (T > 1200 K, mass > 100 M_Earth) → tan/orange banded atmosphere, high cloud density
- CO2-dominated atmosphere → hazy yellow-grey atmosphere, dense clouds
- CH4 in atmosphere → cyan/blue tint (like Uranus/Neptune)
- Low gravity + no atmosphere → high craterStrength, no water
- High albedo → bright ice caps, high cloudsDensity)";

inline std::string buildRenderParamsPrompt(const ExoplanetData& data) {
    std::string prompt = std::format("Generate renderer override JSON for exoplanet: {}\n\n", data.name);

    prompt += "OBSERVATIONAL DATA:\n";

    if (data.mass_earth.hasValue())
        prompt += std::format("  mass_earth:           {:.3f}\n", data.mass_earth.value);
    if (data.radius_earth.hasValue())
        prompt += std::format("  radius_earth:         {:.3f}\n", data.radius_earth.value);
    if (data.equilibrium_temp_k.hasValue())
        prompt += std::format("  equilibrium_temp_k:   {:.1f}\n", data.equilibrium_temp_k.value);
    if (data.surface_gravity_g.hasValue())
        prompt += std::format("  surface_gravity_g:    {:.3f}\n", data.surface_gravity_g.value);
    if (data.density_gcc.hasValue())
        prompt += std::format("  density_gcc:          {:.3f}\n", data.density_gcc.value);
    if (data.semi_major_axis_au.hasValue())
        prompt += std::format("  semi_major_axis_au:   {:.4f}\n", data.semi_major_axis_au.value);
    if (data.albedo.hasValue())
        prompt += std::format("  albedo:               {:.3f}\n", data.albedo.value);
    if (data.surface_pressure_atm.hasValue())
        prompt += std::format("  surface_pressure_atm: {:.3f}\n", data.surface_pressure_atm.value);
    if (data.ocean_coverage_fraction.hasValue())
        prompt += std::format("  ocean_coverage:       {:.3f}\n", data.ocean_coverage_fraction.value);
    if (data.cloud_coverage_fraction.hasValue())
        prompt += std::format("  cloud_coverage:       {:.3f}\n", data.cloud_coverage_fraction.value);
    if (data.ice_coverage_fraction.hasValue())
        prompt += std::format("  ice_coverage:         {:.3f}\n", data.ice_coverage_fraction.value);
    if (data.atmosphere_composition.hasValue())
        prompt += std::format("  atmosphere_composition: {}\n",   data.atmosphere_composition.value);
    if (data.planet_type.hasValue())
        prompt += std::format("  planet_type:          {}\n",     data.planet_type.value);
    if (data.biome_classification.hasValue())
        prompt += std::format("  biome:                {}\n",     data.biome_classification.value);

    if (data.host_star.effective_temp_k.hasValue())
        prompt += std::format("  star_temp_k:          {:.0f}\n", data.host_star.effective_temp_k.value);
    if (!data.host_star.spectral_type.empty())
        prompt += std::format("  star_spectral_type:   {}\n",     data.host_star.spectral_type);

    prompt += R"(
Output ONLY the JSON override object. Example for an Earth-like planet:
{
  "atmosphereColor":    [0.05, 0.30, 0.90],
  "atmosphereDensity":  0.30,
  "waterLevel":         0.22,
  "cloudsDensity":      0.50,
  "polarCapSize":       0.15,
  "noiseStrength":      0.20,
  "continentScale":     0.50,
  "sunIntensity":       3.00
})";

    return prompt;
}

}  // namespace astrocore::prompts
