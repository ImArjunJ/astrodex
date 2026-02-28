#pragma once

#include <string>
#include <set>
#include <spdlog/fmt/fmt.h>
#include "data/ExoplanetData.hpp"

namespace astrocore::prompts {

constexpr std::string_view SYSTEM_PROMPT = R"(You are an expert astrophysicist and planetary scientist. Your task is to infer missing exoplanet parameters based on known observational data, using established physics, empirical mass-radius relationships, and statistical models from exoplanet research.

IMPORTANT: You must respond ONLY with valid JSON. No explanations outside the JSON.

Use these empirical relationships:
1. Mass-Radius (rocky planets, M < 10 M_Earth): R/R_Earth = (M/M_Earth)^0.27
2. Mass-Radius (Neptune-like): R/R_Earth = 2.5 * (M/M_Earth)^0.1
3. Mass-Radius (gas giants): R/R_Jupiter = (M/M_Jupiter)^-0.04
4. Equilibrium temperature: T_eq = T_star * sqrt(R_star / (2*a)) * (1-A)^0.25
5. Scale height: H = kT / (ug) where u is mean molecular weight
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
    prompt += fmt::format("- Planet name: {}\n", data.name);

    if (data.mass_earth.hasValue()) {
        prompt += fmt::format("- Mass: {:.2f} Earth masses\n", data.mass_earth.value);
    }
    if (data.radius_earth.hasValue()) {
        prompt += fmt::format("- Radius: {:.2f} Earth radii\n", data.radius_earth.value);
    }
    if (data.equilibrium_temp_k.hasValue()) {
        prompt += fmt::format("- Equilibrium temperature: {:.0f} K\n", data.equilibrium_temp_k.value);
    }
    if (data.surface_gravity_g.hasValue()) {
        prompt += fmt::format("- Surface gravity: {:.2f} g\n", data.surface_gravity_g.value);
    }
    if (data.orbital_period_days.hasValue()) {
        prompt += fmt::format("- Orbital period: {:.2f} days\n", data.orbital_period_days.value);
    }
    if (data.semi_major_axis_au.hasValue()) {
        prompt += fmt::format("- Semi-major axis: {:.4f} AU\n", data.semi_major_axis_au.value);
    }
    if (data.host_star.effective_temp_k.hasValue()) {
        prompt += fmt::format("- Host star temperature: {:.0f} K\n", data.host_star.effective_temp_k.value);
    }
    if (!data.host_star.spectral_type.empty()) {
        prompt += fmt::format("- Host star spectral type: {}\n", data.host_star.spectral_type);
    }

    prompt += R"(
INFER the following parameters:
1. surface_pressure_atm (atmospheric surface pressure in Earth atmospheres)
2. albedo (geometric albedo, 0-1)
3. atmosphere_composition (primary gases as JSON object with percentages)
4. ocean_coverage_fraction (estimated ocean coverage 0-1, based on temperature and composition)
5. cloud_coverage_fraction (estimated cloud coverage 0-1)
6. ice_coverage_fraction (estimated polar/surface ice coverage 0-1, based on temperature)

Consider:
- Planet's position relative to habitable zone
- Likely atmospheric escape based on escape velocity and stellar radiation
- Bulk composition implied by mass-radius relationship
- For ice_coverage_fraction: T < 200K → high (0.5-0.9), T 200-260K → moderate (0.1-0.5), T > 280K → low (0-0.1))";

    return prompt;
}

inline std::string buildRenderHintsPrompt(const ExoplanetData& data) {
    std::string prompt = "Generate visualization hints for this exoplanet:\n\n";

    prompt += fmt::format("Planet: {}\n", data.name);
    if (data.planet_type.hasValue()) {
        prompt += fmt::format("Type: {}\n", data.planet_type.value);
    }
    if (data.equilibrium_temp_k.hasValue()) {
        prompt += fmt::format("Temperature: {:.0f} K\n", data.equilibrium_temp_k.value);
    }
    if (data.mass_earth.hasValue()) {
        prompt += fmt::format("Mass: {:.2f} Earth masses\n", data.mass_earth.value);
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
- You may include an optional "_confidence" object that maps field names to weights
  in [0.0, 1.0]: 1.0 = fully certain (full override), 0.5 = uncertain (50% blend
  with the physics base), 0.0 = no change. Only include "_confidence" when your
  certainty genuinely varies across fields — omit it entirely if all fields are
  high-confidence.

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

// analogContext: human-readable description of the closest solar-system analog,
//   e.g. "Jupiter (similarity 78%) — banded orange-tan gas giant, high cloud density"
//   Pass empty string when no analog is available.
// skipFields: PlanetParams field names already derived from measured data.
//   Claude must NOT suggest overrides for these.
inline std::string buildRenderParamsPrompt(const ExoplanetData&        data,
                                            const std::string&          analogContext = "",
                                            const std::set<std::string>& skipFields   = {}) {
    std::string prompt = fmt::format("Generate renderer override JSON for exoplanet: {}\n\n", data.name);

    prompt += "OBSERVATIONAL DATA:\n";

    if (data.mass_earth.hasValue())
        prompt += fmt::format("  mass_earth:           {:.3f}\n", data.mass_earth.value);
    if (data.radius_earth.hasValue())
        prompt += fmt::format("  radius_earth:         {:.3f}\n", data.radius_earth.value);
    if (data.equilibrium_temp_k.hasValue())
        prompt += fmt::format("  equilibrium_temp_k:   {:.1f}\n", data.equilibrium_temp_k.value);
    if (data.surface_gravity_g.hasValue())
        prompt += fmt::format("  surface_gravity_g:    {:.3f}\n", data.surface_gravity_g.value);
    if (data.density_gcc.hasValue())
        prompt += fmt::format("  density_gcc:          {:.3f}\n", data.density_gcc.value);
    if (data.semi_major_axis_au.hasValue())
        prompt += fmt::format("  semi_major_axis_au:   {:.4f}\n", data.semi_major_axis_au.value);
    if (data.albedo.hasValue())
        prompt += fmt::format("  albedo:               {:.3f}\n", data.albedo.value);
    if (data.surface_pressure_atm.hasValue())
        prompt += fmt::format("  surface_pressure_atm: {:.3f}\n", data.surface_pressure_atm.value);
    if (data.ocean_coverage_fraction.hasValue())
        prompt += fmt::format("  ocean_coverage:       {:.3f}\n", data.ocean_coverage_fraction.value);
    if (data.cloud_coverage_fraction.hasValue())
        prompt += fmt::format("  cloud_coverage:       {:.3f}\n", data.cloud_coverage_fraction.value);
    if (data.ice_coverage_fraction.hasValue())
        prompt += fmt::format("  ice_coverage:         {:.3f}\n", data.ice_coverage_fraction.value);
    if (data.atmosphere_composition.hasValue())
        prompt += fmt::format("  atmosphere_composition: {}\n",   data.atmosphere_composition.value);
    if (data.planet_type.hasValue())
        prompt += fmt::format("  planet_type:          {}\n",     data.planet_type.value);
    if (data.biome_classification.hasValue())
        prompt += fmt::format("  biome:                {}\n",     data.biome_classification.value);

    if (data.host_star.effective_temp_k.hasValue())
        prompt += fmt::format("  star_temp_k:          {:.0f}\n", data.host_star.effective_temp_k.value);
    if (!data.host_star.spectral_type.empty())
        prompt += fmt::format("  star_spectral_type:   {}\n",     data.host_star.spectral_type);

    // Closest solar-system analog gives Claude a concrete visual reference
    if (!analogContext.empty()) {
        prompt += "\nCLOSEST SOLAR SYSTEM ANALOG:\n";
        prompt += "  " + analogContext + "\n";
        prompt += "  Use this as a visual reference. Only deviate where the physical data above\n";
        prompt += "  clearly indicates the planet differs from its analog.\n";
    }

    // Fields already derived from measured data — Claude must not touch these
    if (!skipFields.empty()) {
        prompt += "\nFIELDS ALREADY SET FROM MEASURED DATA (do NOT include these in output):\n  ";
        bool first = true;
        for (const auto& f : skipFields) {
            if (!first) prompt += ", ";
            prompt += f;
            first = false;
        }
        prompt += "\n";
    }

    prompt += R"(
IMPORTANT: Only output fields you genuinely need to set. Omit anything already
covered by the measured data listed above. Output ONLY the JSON object.

Example for an Earth-like planet (assuming waterLevel and atmosphereDensity are
already set from measured data and therefore omitted):
{
  "atmosphereColor":   [0.05, 0.30, 0.90],
  "cloudsDensity":     0.50,
  "polarCapSize":      0.15,
  "noiseStrength":     0.20,
  "continentScale":    0.50,
  "sandColor":         [0.85, 0.75, 0.50],
  "treeColor":         [0.02, 0.10, 0.04],
  "rockColor":         [0.25, 0.22, 0.18]
})";

    return prompt;
}

}  // namespace astrocore::prompts
