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

}  // namespace astrocore::prompts
