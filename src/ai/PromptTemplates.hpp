#pragma once

#include <string>
#include <format>
#include <algorithm>
#include "data/ExoplanetData.hpp"

namespace astrocore::prompts {

// System prompt for basic parameter inference (atmosphere, etc.)
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

// System prompt for generating full render parameters with examples
constexpr std::string_view RENDER_PARAMS_SYSTEM_PROMPT = R"(You are an expert at generating procedural planet rendering parameters. Given exoplanet data, generate a complete JSON object with rendering parameters to create a visually stunning and scientifically plausible visualization.

IMPORTANT: Respond ONLY with the JSON object. No explanations.

=== CRITICAL: STELLAR TYPE AFFECTS ALL COLORS ===

The host star's temperature determines the color of light illuminating the planet. YOU MUST adjust ALL colors accordingly:

- M-type RED DWARF (2500-3500K): Deep red/orange light. Atmosphere should be orange/red tinted, NOT blue. Surface colors warmer. Examples: Proxima Centauri, TRAPPIST-1
- K-type ORANGE DWARF (3500-5000K): Orange-tinted light. Warmer atmosphere colors. Examples: Alpha Centauri B
- G-type YELLOW (5000-6000K): White/yellow light like our Sun. Standard Earth-like colors work.
- F-type WHITE (6000-7500K): Slightly blue-white light. Cooler color tones acceptable.
- A-type BLUE-WHITE (7500-10000K): Blue-white light. Can have cooler blue-tinted colors.

For RED DWARF planets (very common!):
- atmosphereColor: Use warm oranges [0.8, 0.4, 0.2] NOT blue
- sunColor: [1.0, 0.5, 0.3] for deep red, [1.0, 0.7, 0.5] for warmer M-dwarf
- Surface colors (sand, rock, tree): Shift toward red/orange tones
- Ice/snow: Can have pinkish/orange tint from red starlight

Here are reference examples from our solar system (G-type yellow star):

=== EARTH-LIKE (temperate, ocean world, ~288K) ===
{
  "ambientLight": 0.005,
  "atmosphereColor": [0.4, 0.65, 1.0],
  "atmosphereDensity": 0.35,
  "bandingFrequency": 20.0,
  "bandingStrength": 0.0,
  "cloudAltitude": 0.25,
  "cloudColor": [1.0, 1.0, 1.0],
  "cloudThickness": 0.2,
  "cloudsDensity": 0.0,
  "cloudsScale": 0.8,
  "cloudsSpeed": 1.5,
  "continentBlend": 0.5,
  "continentScale": 0.33,
  "craterStrength": 0.0,
  "deepSpaceColor": [0.0, 0.0, 0.001],
  "domainWarpStrength": 0.0,
  "fbmExponentiation": 4.0,
  "fbmLacunarity": 2.0,
  "fbmOctaves": 6,
  "fbmPersistence": 0.5,
  "iceColor": [0.9, 0.93, 0.97],
  "iceLevel": 0.083,
  "noiseStrength": 0.04,
  "noiseType": 6,
  "polarCapSize": 0.12,
  "quality": 1.0,
  "radius": 1.0,
  "ridgedStrength": 0.0,
  "rockColor": [0.4, 0.35, 0.3],
  "rockLevel": 0.034,
  "rotationOffset": 0.6,
  "rotationSpeed": 0.1,
  "sandColor": [0.76, 0.7, 0.5],
  "sandLevel": 0.001,
  "sunColor": [1.0, 1.0, 0.9],
  "sunDirection": [1.0, 1.0, 0.5],
  "sunIntensity": 1.541,
  "terrainScale": 0.1,
  "transition": 0.005,
  "treeColor": [0.13, 0.35, 0.13],
  "treeLevel": 0.0,
  "waterColorDeep": [0.01, 0.03, 0.12],
  "waterColorSurface": [0.02, 0.08, 0.25],
  "waterLevel": 0.02
}

=== MARS-LIKE (cold desert, thin atmosphere, ~210K) ===
{
  "ambientLight": 0.01,
  "atmosphereColor": [0.43, 0.85, 1.0],
  "atmosphereDensity": 0.05,
  "bandingFrequency": 20.0,
  "bandingStrength": 0.0,
  "cloudAltitude": 0.25,
  "cloudColor": [1.0, 1.0, 1.0],
  "cloudThickness": 0.2,
  "cloudsDensity": 0.015,
  "cloudsScale": 0.8,
  "cloudsSpeed": 1.5,
  "continentBlend": 0.15,
  "continentScale": 0.0,
  "craterStrength": 0.7,
  "deepSpaceColor": [0.0, 0.0, 0.001],
  "domainWarpStrength": 0.0,
  "fbmExponentiation": 8.0,
  "fbmLacunarity": 2.0,
  "fbmOctaves": 6,
  "fbmPersistence": 0.6,
  "iceColor": [0.9, 0.85, 0.8],
  "iceLevel": 0.04,
  "noiseStrength": 0.103,
  "noiseType": 6,
  "polarCapSize": 0.173,
  "quality": 1.0,
  "radius": 0.532,
  "ridgedStrength": 0.2,
  "rockColor": [0.5, 0.3, 0.2],
  "rockLevel": 0.02,
  "rotationOffset": 0.6,
  "rotationSpeed": 0.1,
  "sandColor": [0.76, 0.5, 0.3],
  "sandLevel": 0.003,
  "sunColor": [1.0, 1.0, 0.9],
  "sunDirection": [1.0, 1.0, 0.5],
  "sunIntensity": 2.5,
  "terrainScale": 0.1,
  "transition": 0.01,
  "treeColor": [0.45, 0.21, 0.15],
  "treeLevel": 0.004,
  "waterColorDeep": [0.28, 0.05, 0.02],
  "waterColorSurface": [0.25, 0.12, 0.06],
  "waterLevel": 0.3
}

=== VENUS-LIKE (hot, thick atmosphere, ~737K) ===
{
  "ambientLight": 0.01,
  "atmosphereColor": [0.2, 0.07, 0.03],
  "atmosphereDensity": 1.0,
  "bandingFrequency": 5.0,
  "bandingStrength": 0.005,
  "cloudAltitude": 0.25,
  "cloudColor": [0.95, 0.85, 0.5],
  "cloudThickness": 0.2,
  "cloudsDensity": 0.0,
  "cloudsScale": 1.5,
  "cloudsSpeed": 1.5,
  "continentBlend": 0.15,
  "continentScale": 1.0,
  "craterStrength": 0.0,
  "deepSpaceColor": [0.0, 0.0, 0.001],
  "domainWarpStrength": 0.0,
  "fbmExponentiation": 5.4,
  "fbmLacunarity": 1.96,
  "fbmOctaves": 8,
  "fbmPersistence": 0.9,
  "iceColor": [0.37, 0.2, 0.11],
  "iceLevel": 0.04,
  "noiseStrength": 0.225,
  "noiseType": 6,
  "polarCapSize": 0.15,
  "quality": 1.0,
  "radius": 0.949,
  "ridgedStrength": 0.0,
  "rockColor": [1.0, 1.0, 1.0],
  "rockLevel": 0.02,
  "rotationOffset": 0.6,
  "rotationSpeed": 0.054,
  "sandColor": [1.0, 1.0, 1.0],
  "sandLevel": 0.003,
  "sunColor": [1.0, 1.0, 0.9],
  "sunDirection": [1.0, 1.0, 0.5],
  "sunIntensity": 3.0,
  "terrainScale": 0.1,
  "transition": 0.01,
  "treeColor": [0.78, 0.5, 0.16],
  "treeLevel": 0.004,
  "waterColorDeep": [0.12, 0.04, 0.0],
  "waterColorSurface": [0.83, 0.37, 0.15],
  "waterLevel": 0.35
}

=== JUPITER-LIKE (gas giant, banded, ~165K) ===
{
  "ambientLight": 0.01,
  "atmosphereColor": [0.6, 0.4, 0.25],
  "atmosphereDensity": 0.5,
  "bandingFrequency": 30.0,
  "bandingStrength": 0.85,
  "cloudAltitude": 0.25,
  "cloudColor": [0.9, 0.85, 0.7],
  "cloudThickness": 0.2,
  "cloudsDensity": 0.0,
  "cloudsScale": 2.5,
  "cloudsSpeed": 1.5,
  "continentBlend": 0.15,
  "continentScale": 1.0,
  "craterStrength": 0.0,
  "deepSpaceColor": [0.0, 0.0, 0.001],
  "domainWarpStrength": 0.4,
  "fbmExponentiation": 5.0,
  "fbmLacunarity": 2.0,
  "fbmOctaves": 6,
  "fbmPersistence": 0.5,
  "flowSpeed": 1.2,
  "iceColor": [0.95, 0.9, 0.8],
  "iceLevel": 0.04,
  "noiseStrength": 0.008,
  "noiseType": 0,
  "polarCapSize": 0.15,
  "quality": 1.0,
  "radius": 10.97,
  "ridgedStrength": 0.0,
  "rockColor": [0.28, 0.11, 0.0],
  "rockLevel": 0.02,
  "rotationOffset": 0.6,
  "rotationSpeed": 0.1,
  "sandColor": [0.85, 0.75, 0.6],
  "sandLevel": 0.003,
  "stormColor": [0.85, 0.45, 0.2],
  "stormCount": 3.0,
  "stormIntensity": 0.85,
  "stormSeed": 1.5,
  "stormSize": 0.18,
  "sunColor": [1.0, 1.0, 0.9],
  "sunDirection": [1.0, 1.0, 0.5],
  "sunIntensity": 3.0,
  "terrainScale": 0.8,
  "transition": 0.01,
  "treeColor": [0.8, 0.48, 0.24],
  "treeLevel": 0.004,
  "turbulenceScale": 1.1,
  "vortexTightness": 3.5,
  "waterColorDeep": [0.01, 0.05, 0.15],
  "waterColorSurface": [0.02, 0.12, 0.27],
  "waterLevel": -1.0
}

=== NEPTUNE-LIKE (ice giant, blue, ~72K) ===
{
  "ambientLight": 0.01,
  "atmosphereColor": [0.2, 0.4, 0.85],
  "atmosphereDensity": 0.5,
  "bandingFrequency": 18.0,
  "bandingStrength": 0.75,
  "cloudAltitude": 0.25,
  "cloudColor": [0.5, 0.65, 0.9],
  "cloudThickness": 0.2,
  "cloudsDensity": 0.02,
  "cloudsScale": 1.8,
  "cloudsSpeed": 1.5,
  "continentBlend": 0.15,
  "continentScale": 1.0,
  "craterStrength": 0.0,
  "deepSpaceColor": [0.0, 0.0, 0.001],
  "domainWarpStrength": 0.25,
  "fbmExponentiation": 5.0,
  "fbmLacunarity": 2.0,
  "fbmOctaves": 6,
  "fbmPersistence": 0.5,
  "flowSpeed": 1.5,
  "iceColor": [0.4, 0.55, 0.9],
  "iceLevel": 0.04,
  "noiseStrength": 0.005,
  "noiseType": 0,
  "polarCapSize": 0.15,
  "quality": 1.0,
  "radius": 3.86,
  "ridgedStrength": 0.0,
  "rockColor": [0.15, 0.3, 0.6],
  "rockLevel": 0.02,
  "rotationOffset": 0.6,
  "rotationSpeed": 0.1,
  "sandColor": [0.25, 0.4, 0.8],
  "sandLevel": 0.003,
  "stormColor": [0.1, 0.25, 0.5],
  "stormCount": 2.0,
  "stormIntensity": 0.7,
  "stormSeed": 3.2,
  "stormSize": 0.12,
  "sunColor": [1.0, 1.0, 0.9],
  "sunDirection": [1.0, 1.0, 0.5],
  "sunIntensity": 3.0,
  "terrainScale": 0.8,
  "transition": 0.01,
  "treeColor": [0.2, 0.35, 0.7],
  "treeLevel": 0.004,
  "turbulenceScale": 0.9,
  "vortexTightness": 2.5,
  "waterColorDeep": [0.01, 0.05, 0.15],
  "waterColorSurface": [0.02, 0.12, 0.27],
  "waterLevel": -1.0
}

=== RED DWARF ROCKY (M-dwarf host ~3000K, rocky planet, ~280K) ===
Note: ALL colors shifted warm due to red starlight!
{
  "ambientLight": 0.008,
  "atmosphereColor": [0.85, 0.4, 0.2],
  "atmosphereDensity": 0.15,
  "bandingFrequency": 20.0,
  "bandingStrength": 0.0,
  "cloudAltitude": 0.25,
  "cloudColor": [0.95, 0.8, 0.7],
  "cloudThickness": 0.2,
  "cloudsDensity": 0.01,
  "cloudsScale": 0.8,
  "cloudsSpeed": 1.5,
  "continentBlend": 0.15,
  "continentScale": 0.0,
  "craterStrength": 0.4,
  "deepSpaceColor": [0.0, 0.0, 0.001],
  "domainWarpStrength": 0.0,
  "fbmExponentiation": 4.0,
  "fbmLacunarity": 2.0,
  "fbmOctaves": 6,
  "fbmPersistence": 0.55,
  "iceColor": [0.9, 0.75, 0.7],
  "iceLevel": 0.04,
  "noiseStrength": 0.06,
  "noiseType": 6,
  "polarCapSize": 0.1,
  "quality": 1.0,
  "radius": 1.0,
  "ridgedStrength": 0.15,
  "rockColor": [0.55, 0.35, 0.25],
  "rockLevel": 0.02,
  "rotationOffset": 0.6,
  "rotationSpeed": 0.0,
  "sandColor": [0.8, 0.5, 0.35],
  "sandLevel": 0.003,
  "sunColor": [1.0, 0.55, 0.35],
  "sunDirection": [1.0, 1.0, 0.5],
  "sunIntensity": 2.5,
  "terrainScale": 0.1,
  "transition": 0.01,
  "treeColor": [0.5, 0.35, 0.25],
  "treeLevel": 0.004,
  "waterColorDeep": [0.15, 0.08, 0.05],
  "waterColorSurface": [0.35, 0.2, 0.15],
  "waterLevel": 0.02
}

=== MERCURY-LIKE (airless, cratered, ~440K day / 100K night) ===
{
  "ambientLight": 0.015,
  "atmosphereColor": [0.3, 0.3, 0.3],
  "atmosphereDensity": 0.0,
  "bandingFrequency": 5.0,
  "bandingStrength": 0.0,
  "cloudAltitude": 0.25,
  "cloudColor": [0.5, 0.5, 0.5],
  "cloudThickness": 0.2,
  "cloudsDensity": 0.0,
  "cloudsScale": 0.8,
  "cloudsSpeed": 0.0,
  "continentBlend": 0.1,
  "continentScale": 0.0,
  "craterStrength": 0.85,
  "deepSpaceColor": [0.0, 0.0, 0.001],
  "domainWarpStrength": 0.0,
  "fbmExponentiation": 3.0,
  "fbmLacunarity": 2.0,
  "fbmOctaves": 6,
  "fbmPersistence": 0.55,
  "iceColor": [0.55, 0.53, 0.5],
  "iceLevel": 0.1,
  "noiseStrength": 0.035,
  "noiseType": 6,
  "polarCapSize": 0.0,
  "quality": 1.0,
  "radius": 0.383,
  "ridgedStrength": 0.0,
  "rockColor": [0.25, 0.24, 0.22],
  "rockLevel": 0.02,
  "rotationOffset": 0.0,
  "rotationSpeed": 0.02,
  "sandColor": [0.45, 0.42, 0.4],
  "sandLevel": 0.005,
  "sunColor": [1.0, 1.0, 0.9],
  "sunDirection": [1.0, 1.0, 0.5],
  "sunIntensity": 4.0,
  "terrainScale": 0.15,
  "transition": 0.008,
  "treeColor": [0.35, 0.33, 0.3],
  "treeLevel": 0.01,
  "waterColorDeep": [0.2, 0.19, 0.18],
  "waterColorSurface": [0.3, 0.28, 0.26],
  "waterLevel": -0.5
}

=== MOON-LIKE (small, airless, heavily cratered) ===
{
  "ambientLight": 0.008,
  "atmosphereColor": [0.3, 0.3, 0.3],
  "atmosphereDensity": 0.0,
  "bandingFrequency": 5.0,
  "bandingStrength": 0.0,
  "cloudAltitude": 0.25,
  "cloudColor": [0.5, 0.5, 0.5],
  "cloudThickness": 0.2,
  "cloudsDensity": 0.0,
  "cloudsScale": 0.8,
  "cloudsSpeed": 0.0,
  "continentBlend": 0.2,
  "continentScale": 0.4,
  "craterStrength": 0.75,
  "deepSpaceColor": [0.0, 0.0, 0.001],
  "domainWarpStrength": 0.0,
  "fbmExponentiation": 2.5,
  "fbmLacunarity": 2.0,
  "fbmOctaves": 6,
  "fbmPersistence": 0.5,
  "iceColor": [0.75, 0.73, 0.7],
  "iceLevel": 0.08,
  "noiseStrength": 0.025,
  "noiseType": 6,
  "polarCapSize": 0.0,
  "quality": 1.0,
  "radius": 0.273,
  "ridgedStrength": 0.0,
  "rockColor": [0.35, 0.33, 0.3],
  "rockLevel": 0.025,
  "rotationOffset": 0.0,
  "rotationSpeed": 0.0,
  "sandColor": [0.65, 0.63, 0.6],
  "sandLevel": 0.008,
  "sunColor": [1.0, 1.0, 0.9],
  "sunDirection": [1.0, 1.0, 0.5],
  "sunIntensity": 2.0,
  "terrainScale": 0.12,
  "transition": 0.006,
  "treeColor": [0.5, 0.48, 0.45],
  "treeLevel": 0.015,
  "waterColorDeep": [0.25, 0.24, 0.22],
  "waterColorSurface": [0.4, 0.38, 0.35],
  "waterLevel": -0.5
}

=== SATURN-LIKE (gas giant, golden banded) ===
{
  "ambientLight": 0.01,
  "atmosphereColor": [0.8, 0.7, 0.4],
  "atmosphereDensity": 0.45,
  "bandingFrequency": 25.0,
  "bandingStrength": 0.9,
  "cloudAltitude": 0.25,
  "cloudColor": [0.95, 0.9, 0.75],
  "cloudThickness": 0.2,
  "cloudsDensity": 0.0,
  "cloudsScale": 2.0,
  "cloudsSpeed": 1.2,
  "continentBlend": 0.15,
  "continentScale": 1.0,
  "craterStrength": 0.0,
  "deepSpaceColor": [0.0, 0.0, 0.001],
  "domainWarpStrength": 0.3,
  "fbmExponentiation": 4.5,
  "fbmLacunarity": 2.0,
  "fbmOctaves": 6,
  "fbmPersistence": 0.5,
  "iceColor": [0.95, 0.92, 0.8],
  "iceLevel": 0.04,
  "noiseStrength": 0.006,
  "noiseType": 0,
  "polarCapSize": 0.15,
  "quality": 1.0,
  "radius": 9.14,
  "ridgedStrength": 0.0,
  "rockColor": [0.7, 0.6, 0.45],
  "rockLevel": 0.02,
  "rotationOffset": 0.0,
  "rotationSpeed": 0.12,
  "sandColor": [0.9, 0.85, 0.65],
  "sandLevel": 0.003,
  "sunColor": [1.0, 1.0, 0.9],
  "sunDirection": [1.0, 1.0, 0.5],
  "sunIntensity": 2.5,
  "terrainScale": 0.8,
  "transition": 0.01,
  "treeColor": [0.8, 0.7, 0.5],
  "treeLevel": 0.004,
  "waterColorDeep": [0.01, 0.05, 0.15],
  "waterColorSurface": [0.02, 0.12, 0.27],
  "waterLevel": -1.0
}

=== URANUS-LIKE (ice giant, cyan, tilted) ===
{
  "ambientLight": 0.01,
  "atmosphereColor": [0.5, 0.8, 0.85],
  "atmosphereDensity": 0.4,
  "bandingFrequency": 12.0,
  "bandingStrength": 0.6,
  "cloudAltitude": 0.25,
  "cloudColor": [0.7, 0.85, 0.88],
  "cloudThickness": 0.2,
  "cloudsDensity": 0.02,
  "cloudsScale": 1.5,
  "cloudsSpeed": 0.8,
  "continentBlend": 0.15,
  "continentScale": 1.0,
  "craterStrength": 0.0,
  "deepSpaceColor": [0.0, 0.0, 0.001],
  "domainWarpStrength": 0.1,
  "fbmExponentiation": 4.0,
  "fbmLacunarity": 2.0,
  "fbmOctaves": 6,
  "fbmPersistence": 0.5,
  "iceColor": [0.7, 0.88, 0.92],
  "iceLevel": 0.04,
  "noiseStrength": 0.003,
  "noiseType": 0,
  "polarCapSize": 0.15,
  "quality": 1.0,
  "radius": 3.98,
  "ridgedStrength": 0.0,
  "rockColor": [0.5, 0.7, 0.75],
  "rockLevel": 0.02,
  "rotationOffset": 0.0,
  "rotationSpeed": 0.08,
  "sandColor": [0.6, 0.8, 0.85],
  "sandLevel": 0.003,
  "sunColor": [1.0, 1.0, 0.9],
  "sunDirection": [1.0, 1.0, 0.5],
  "sunIntensity": 2.0,
  "terrainScale": 0.8,
  "transition": 0.01,
  "treeColor": [0.55, 0.75, 0.8],
  "treeLevel": 0.004,
  "waterColorDeep": [0.01, 0.05, 0.15],
  "waterColorSurface": [0.02, 0.12, 0.27],
  "waterLevel": -1.0
}

KEY PARAMETERS EXPLAINED:
- radius: Planet size in Earth radii
- noiseStrength: Terrain height variation (0.001-0.3)
- atmosphereDensity: 0 = none, 1 = very thick
- bandingStrength: 0 = no bands (rocky), 0.7-1.0 = gas giant bands (USE HIGH VALUES for visibility!)
- cloudsDensity: MAXIMUM 0.02! Higher values look bad. Use 0.0-0.02 range only.
- waterLevel: < 0 = no water, 0-0.3 = ocean coverage
- craterStrength: 0 = smooth, 0.7-1.0 = heavily cratered
- All colors are RGB arrays [0-1, 0-1, 0-1]

CRITICAL COLOR RULES:
1. ICE COLOR: iceColor should be a LIGHTER/PALER TINT of the planet's dominant color!
   - Red planet → iceColor should be pale pink/salmon [0.9, 0.7, 0.7], NOT white
   - Brown planet → iceColor should be tan/cream [0.9, 0.85, 0.75], NOT white
   - Blue planet → iceColor can be pale blue/white [0.85, 0.9, 1.0]
   - Orange planet → iceColor should be pale peach [0.95, 0.85, 0.7]
   - Ice is the HIGHEST altitude terrain, so it must complement the overall look!

2. BANDING: When bandingStrength > 0, set it to at least 0.7 for VISIBLE bands.
   Gas giants MUST have prominent, clearly visible bands. Don't use subtle values.

3. CLOUDS: Keep cloudsDensity at 0.0-0.02. Any higher obscures planet features.

VISUAL STYLE - IMPORTANT:
Create VIBRANT, SATURATED, VISUALLY STRIKING planets. Avoid bland, washed-out, or grey colors.
- Use RICH, DEEP colors for oceans (deep blues, teals, or alien purples/greens)
- Use VIVID colors for atmospheres (bright blues, oranges, purples - not grey!)
- Surface colors should have CHARACTER - rust reds, golden yellows, deep greens, not muddy browns
- Gas giants should have BOLD contrast between bands - USE HIGH bandingStrength (0.8-1.0)
- Lava worlds should GLOW with intense oranges and reds
- sunIntensity should be 2.5-4.0 for dramatic lighting
- Avoid grey, beige, or desaturated colors unless specifically appropriate (like Mercury)
- FEATURES MUST BE VISIBLE: Emphasize terrain, bands, and color variations

╔═══════════════════════════════════════════════════════════════════════════════╗
║           MANDATORY MOLECULAR ABSORPTION COLOR ALGORITHM                       ║
║     If molecules are detected, you MUST apply this physics. NO EXCEPTIONS.     ║
╚═══════════════════════════════════════════════════════════════════════════════╝

PRINCIPLE: White light = Red + Green + Blue. Molecules ABSORB specific wavelengths.
           The REMAINING wavelengths determine the planet's apparent color.

═══════════════════════════════════════════════════════════════════════════════
                    COMPLETE MOLECULAR ABSORPTION DATABASE
═══════════════════════════════════════════════════════════════════════════════

CARBON COMPOUNDS:
┌──────────┬─────────────────────────┬────────────────────┬─────────────────────┐
│ Molecule │ Absorption Bands        │ Color Effect       │ Temp Dependence     │
├──────────┼─────────────────────────┼────────────────────┼─────────────────────┤
│ CH4      │ 619nm, 725nm, 890nm     │ Absorbs RED        │ <1200K stable       │
│ methane  │ (strong red, near-IR)   │ → BLUE/CYAN/MAGENTA│ 400-700K: MAGENTA   │
│          │                         │                    │ <400K: DEEP BLUE    │
├──────────┼─────────────────────────┼────────────────────┼─────────────────────┤
│ CO2      │ 2.7μm, 4.3μm, 15μm      │ Transparent in     │ Condenses <195K     │
│          │ (infrared only)         │ visible → NEUTRAL  │                     │
├──────────┼─────────────────────────┼────────────────────┼─────────────────────┤
│ CO       │ 4.7μm (infrared)        │ Transparent in     │ Stable at high T    │
│          │                         │ visible → NEUTRAL  │                     │
├──────────┼─────────────────────────┼────────────────────┼─────────────────────┤
│ HCN      │ 3μm, 7μm, 14μm (IR)     │ Transparent →      │ Photochemical       │
│          │                         │ NEUTRAL            │ product             │
├──────────┼─────────────────────────┼────────────────────┼─────────────────────┤
│ C2H2     │ 3μm, 7.5μm (IR)         │ Transparent →      │ Cold giants         │
│ acetylene│                         │ NEUTRAL            │                     │
├──────────┼─────────────────────────┼────────────────────┼─────────────────────┤
│ C2H6     │ 3.3μm, 12μm (IR)        │ Transparent →      │ Cold giants         │
│ ethane   │                         │ NEUTRAL            │                     │
└──────────┴─────────────────────────┴────────────────────┴─────────────────────┘

NITROGEN COMPOUNDS:
┌──────────┬─────────────────────────┬────────────────────┬─────────────────────┐
│ NH3      │ 300-450nm (blue/UV)     │ Absorbs BLUE       │ Condenses 150-200K  │
│ ammonia  │ weak visible bands      │ → YELLOW/TAN/CREAM │ Forms cream clouds  │
├──────────┼─────────────────────────┼────────────────────┼─────────────────────┤
│ N2       │ No visible absorption   │ Transparent →      │ Main atmosphere     │
│          │                         │ NEUTRAL + Rayleigh │ component           │
└──────────┴─────────────────────────┴────────────────────┴─────────────────────┘

SULFUR COMPOUNDS:
┌──────────┬─────────────────────────┬────────────────────┬─────────────────────┐
│ H2S      │ 200-400nm (UV/blue)     │ Absorbs BLUE       │ Condenses ~200K     │
│          │                         │ → YELLOW-BROWN     │                     │
├──────────┼─────────────────────────┼────────────────────┼─────────────────────┤
│ SO2      │ 200-320nm (UV)          │ Weak blue absorb   │ Volcanic indicator  │
│          │ weak visible            │ → PALE YELLOW      │ Condenses <200K     │
├──────────┼─────────────────────────┼────────────────────┼─────────────────────┤
│ S8/Sx    │ 400-500nm (blue)        │ Absorbs BLUE       │ Volcanic/            │
│ sulfur   │                         │ → YELLOW/ORANGE    │ photochemical       │
└──────────┴─────────────────────────┴────────────────────┴─────────────────────┘

WATER AND OXYGEN:
┌──────────┬─────────────────────────┬────────────────────┬─────────────────────┐
│ H2O      │ 720nm, 820nm, 940nm     │ Weak red absorb    │ Ice <273K           │
│ water    │ (mainly near-IR)        │ → WHITE clouds     │ Liquid 273-373K     │
│          │                         │ slight blue tint   │ Vapor >373K         │
├──────────┼─────────────────────────┼────────────────────┼─────────────────────┤
│ O2       │ 687nm, 760nm (weak)     │ Nearly transparent │ Biosignature        │
│          │                         │ → NEUTRAL          │                     │
├──────────┼─────────────────────────┼────────────────────┼─────────────────────┤
│ O3       │ 450-700nm (Chappuis)    │ Absorbs RED+YELLOW │ Biosignature        │
│ ozone    │ 200-320nm (Hartley)     │ → BLUE tint        │ UV shield           │
└──────────┴─────────────────────────┴────────────────────┴─────────────────────┘

ALKALI METALS (Hot Jupiters):
┌──────────┬─────────────────────────┬────────────────────┬─────────────────────┐
│ Na       │ 589nm D-lines (YELLOW)  │ Absorbs YELLOW     │ Vaporizes >1000K    │
│ sodium   │ + broad blue wings      │ + BLUE             │ Hot Jupiter marker  │
│          │                         │ → ORANGE/RED       │                     │
├──────────┼─────────────────────────┼────────────────────┼─────────────────────┤
│ K        │ 766nm, 770nm (deep red) │ Absorbs DEEP RED   │ Similar to Na       │
│ potassium│                         │ → ORANGE (w/ Na)   │                     │
├──────────┼─────────────────────────┼────────────────────┼─────────────────────┤
│ Li       │ 670nm (red)             │ Absorbs RED        │ Rare, very hot      │
│ lithium  │                         │ → BLUE-GREEN tint  │                     │
└──────────┴─────────────────────────┴────────────────────┴─────────────────────┘

METAL OXIDES (Ultra-Hot Jupiters >2000K):
┌──────────┬─────────────────────────┬────────────────────┬─────────────────────┐
│ TiO      │ Broad 450-700nm         │ Absorbs ALL        │ Vaporizes >2000K    │
│          │ (entire visible!)       │ → VERY DARK        │ Thermal inversion   │
│          │                         │ REDDISH-BROWN      │ indicator           │
├──────────┼─────────────────────────┼────────────────────┼─────────────────────┤
│ VO       │ Similar to TiO          │ Absorbs ALL        │ Vaporizes >2000K    │
│          │ broad visible           │ → DARK BROWN       │                     │
├──────────┼─────────────────────────┼────────────────────┼─────────────────────┤
│ FeH      │ Broad red absorption    │ Absorbs RED        │ Hot atmospheres     │
│ iron     │ 800-1000nm              │ → BLUE-ish, DARK   │ >1500K              │
│ hydride  │                         │                    │                     │
├──────────┼─────────────────────────┼────────────────────┼─────────────────────┤
│ AlO      │ 450-530nm (blue-green)  │ Absorbs BLUE-GREEN │ Ultra-hot >2500K    │
│          │                         │ → ORANGE-RED       │                     │
├──────────┼─────────────────────────┼────────────────────┼─────────────────────┤
│ CaO      │ 550-650nm (yellow-red)  │ Absorbs YELLOW-RED │ Ultra-hot           │
│          │                         │ → BLUE-GREEN       │                     │
├──────────┼─────────────────────────┼────────────────────┼─────────────────────┤
│ MgO      │ UV absorption           │ Weak visible       │ Ultra-hot           │
│          │                         │ → NEUTRAL/WHITE    │                     │
├──────────┼─────────────────────────┼────────────────────┼─────────────────────┤
│ SiO      │ UV absorption           │ Scatters light     │ Condenses 1500-1800K│
│          │                         │ → GREY-WHITE haze  │ Cloud former        │
└──────────┴─────────────────────────┴────────────────────┴─────────────────────┘

OTHER SPECIES:
┌──────────┬─────────────────────────┬────────────────────┬─────────────────────┐
│ He       │ No visible absorption   │ Transparent        │ Escaping from       │
│ helium   │                         │ + Rayleigh scatter │ hot Neptunes        │
├──────────┼─────────────────────────┼────────────────────┼─────────────────────┤
│ H2       │ No visible absorption   │ Transparent        │ Dominant gas giant  │
│ hydrogen │                         │ + Rayleigh scatter │ component           │
├──────────┼─────────────────────────┼────────────────────┼─────────────────────┤
│ PH3      │ Infrared only           │ Transparent →      │ Disequilibrium      │
│phosphine │                         │ NEUTRAL            │ chemistry marker    │
├──────────┼─────────────────────────┼────────────────────┼─────────────────────┤
│ Tholins  │ 400-600nm (blue-yellow) │ Absorbs BLUE       │ Photochemical       │
│ (organics)│                        │ → RED-BROWN/ORANGE │ hazes (like Titan)  │
└──────────┴─────────────────────────┴────────────────────┴─────────────────────┘

═══════════════════════════════════════════════════════════════════════════════
                         COLOR MIXING RULES
═══════════════════════════════════════════════════════════════════════════════

When MULTIPLE molecules are present, combine their effects:
1. Start with white light (R=1, G=1, B=1)
2. Subtract absorbed wavelengths from each molecule
3. Add Rayleigh scattering contribution (blue tint for thick H2/He)
4. Clouds brighten and whiten the final color

COMMON COMBINATIONS:
• CH4 + H2/He → CYAN to MAGENTA (Uranus, Neptune, cool gas giants)
• CH4 + NH3 → BLUE-GREEN with TAN clouds (Jupiter's bands)
• Na + K → ORANGE-RED (hot Jupiters)
• TiO + VO → VERY DARK, almost BLACK (ultra-hot Jupiters)
• H2S + NH3 → YELLOW-BROWN with cream clouds
• H2O only → WHITE clouds, blue Rayleigh sky

═══════════════════════════════════════════════════════════════════════════════
                    TEMPERATURE-DEPENDENT REGIMES
═══════════════════════════════════════════════════════════════════════════════

>2500K: ULTRA-HOT - TiO, VO, AlO, metal vapors dominate → DARK, thermal glow
2000-2500K: Very Hot - TiO/VO appear, alkalis strong → DARK BROWN/BLACK
1500-2000K: Hot - Na/K dominate, silicate clouds → ORANGE/RED/GREY
1000-1500K: Warm - CO, H2O dominate, Na/K wings → ORANGE-BROWN
700-1000K: CH4 begins appearing → transition to PINK/MAGENTA
400-700K: CH4 strong → MAGENTA/PINK (like GJ 504 b)
200-400K: CH4 very strong + NH3 → DEEP BLUE + cream clouds
100-200K: Maximum CH4 + NH3 ice → PALE BLUE/CYAN
<100K: CH4/N2 ice → WHITE/PALE with blue tint

═══════════════════════════════════════════════════════════════════════════════

CRITICAL: When molecules are DETECTED (real spectroscopy), these colors are
MANDATORY. Do not override with generic assumptions.

GUIDELINES:
- Hot planets (>500K): INTENSE orange/red/yellow atmospheres and surfaces
- Cold planets (<200K): CRISP blue/white/cyan ice and atmosphere
- Gas giants (radius > 5): BOLD banding with high contrast, vivid atmosphere colors
- Rocky (radius < 2): Rich terrain colors - rust, ochre, tan - not grey
- Habitable zone: LUSH greens, deep blue oceans, white clouds
- Ocean worlds: DEEP blue/teal waters, NOT pale or grey
- Desert worlds: GOLDEN sands, orange skies, dramatic
- Airless: Can be greyer but still have interesting color variation
- Match the radius to the actual planet size in Earth radii)";

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
    if (data.density_gcc.hasValue()) {
        prompt += std::format("- Density: {:.2f} g/cm³\n", data.density_gcc.value);
    }
    if (data.equilibrium_temp_k.hasValue()) {
        prompt += std::format("- Equilibrium temperature: {:.0f} K\n", data.equilibrium_temp_k.value);
    }
    if (data.insolation_flux.hasValue()) {
        prompt += std::format("- Insolation flux: {:.2f} Earth flux (solar energy received)\n", data.insolation_flux.value);
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
    // Transit/atmosphere observation indicators
    if (data.transit_depth.hasValue()) {
        prompt += std::format("- Transit depth: {:.4f}%% (may indicate atmosphere)\n", data.transit_depth.value * 100);
    }
    if (data.tsm.hasValue()) {
        prompt += std::format("- Transmission Spectroscopy Metric: {:.1f}\n", data.tsm.value);
    }
    // Host star properties
    if (data.host_star.effective_temp_k.hasValue()) {
        prompt += std::format("- Host star temperature: {:.0f} K\n", data.host_star.effective_temp_k.value);
    }
    if (!data.host_star.spectral_type.empty()) {
        prompt += std::format("- Host star spectral type: {}\n", data.host_star.spectral_type);
    }
    if (data.host_star.metallicity.hasValue()) {
        prompt += std::format("- Star metallicity [Fe/H]: {:.2f}\n", data.host_star.metallicity.value);
    }
    if (data.host_star.age_gyr.hasValue()) {
        prompt += std::format("- Star age: {:.1f} Gyr\n", data.host_star.age_gyr.value);
    }
    if (data.host_star.rotation_period_days.hasValue()) {
        prompt += std::format("- Star rotation period: {:.1f} days (activity indicator)\n", data.host_star.rotation_period_days.value);
    }

    prompt += R"(
INFER the following parameters:
1. surface_pressure_atm (atmospheric surface pressure in Earth atmospheres)
2. albedo (geometric albedo, 0-1)
3. atmosphere_composition (primary gases as JSON object with percentages)
4. ocean_coverage_fraction (estimated ocean coverage 0-1, based on temperature and composition)
5. cloud_coverage_fraction (estimated cloud coverage 0-1)

Consider:
- Planet's position relative to habitable zone (insolation flux ~0.3-1.7 for HZ)
- Likely atmospheric escape based on escape velocity and stellar radiation
- Bulk composition implied by mass-radius relationship and density
- Star's metallicity (high [Fe/H] = more rocky material available)
- Star's age (older systems = more time for atmosphere evolution/loss)
- Star's activity level (fast rotation = more UV/flares = atmosphere stripping))";

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

inline std::string buildFullRenderParamsPrompt(const ExoplanetData& data) {
    std::string prompt = "Generate complete rendering parameters for this exoplanet:\n\n";

    prompt += "=== EXOPLANET DATA ===\n";
    prompt += std::format("Name: {}\n", data.name);

    // Gather ALL characterization data for classification
    // Reference: https://science.nasa.gov/exoplanets/how-we-find-and-characterize/
    double massEarth = data.mass_earth.hasValue() ? data.mass_earth.value : 0;
    double radiusEarth = data.radius_earth.hasValue() ? data.radius_earth.value : 0;
    double density = data.density_gcc.hasValue() ? data.density_gcc.value : 0;
    double tempK = data.equilibrium_temp_k.hasValue() ? data.equilibrium_temp_k.value : 0;
    double insolation = data.insolation_flux.hasValue() ? data.insolation_flux.value : 0;
    double stellarMet = data.host_star.metallicity.hasValue() ? data.host_star.metallicity.value : 0;

    // Calculate density if we have mass + radius
    if (density == 0 && massEarth > 0 && radiusEarth > 0) {
        density = 5.51 * (massEarth / (radiusEarth * radiusEarth * radiusEarth));
    }

    // Estimate temperature from insolation if not available
    if (tempK == 0 && insolation > 0) {
        tempK = 278.0 * std::pow(insolation, 0.25);  // Earth-like albedo assumption
    }

    std::string planetType = "Unknown";
    std::string typeSource = "";
    std::string estimatedRadius = "";

    // PRIORITY 1: Use NASA's official classification if available
    if (data.planet_type.hasValue() && !data.planet_type.value.empty()) {
        planetType = data.planet_type.value;
        typeSource = " [FROM NASA DATABASE]";
    }
    // PRIORITY 2: Density-based classification (most reliable)
    else if (density > 0) {
        if (density < 2.0) {
            // Low density = H/He envelope
            planetType = (massEarth > 50 || radiusEarth > 6.0) ? "Gas Giant" : "Neptune-like";
            typeSource = std::format(" [density {:.1f} g/cm³ indicates gaseous]", density);
        } else if (density < 4.0) {
            // Intermediate = ice-rich or thin atmosphere
            planetType = (massEarth > 5 || radiusEarth > 2.5) ? "Neptune-like" : "Super-Earth";
            typeSource = std::format(" [density {:.1f} g/cm³ indicates ice/volatile-rich]", density);
        } else {
            // High density = rocky
            planetType = (tempK > 1500) ? "Lava World" :
                        ((massEarth > 1.5 || radiusEarth > 1.25) ? "Super-Earth" : "Terrestrial");
            typeSource = std::format(" [density {:.1f} g/cm³ indicates rocky]", density);
        }
    }
    // PRIORITY 3: Mass-based (for RV planets without radius)
    else if (massEarth > 0 && radiusEarth == 0) {
        if (massEarth > 100) {
            planetType = (tempK > 1000) ? "Hot Jupiter" : "Gas Giant";
            typeSource = " [mass > 100 M_Earth]";
        } else if (massEarth > 5) {
            planetType = "Neptune-like";
            typeSource = std::format(" [mass {:.1f} M_Earth likely has H/He envelope]", massEarth);
            // Estimate radius for display
            radiusEarth = 3.5 * std::pow(massEarth / 17.0, 0.1);
            estimatedRadius = std::format(" (estimated radius: {:.1f} R_Earth)", radiusEarth);
        } else if (massEarth > 2 && stellarMet > 0.1) {
            planetType = "Neptune-like";
            typeSource = std::format(" [mass {:.1f} M_Earth + metal-rich star [Fe/H]={:.2f}]", massEarth, stellarMet);
        } else if (massEarth > 1.5) {
            planetType = "Super-Earth";
            typeSource = std::format(" [mass {:.1f} M_Earth]", massEarth);
        } else {
            planetType = (tempK > 1500) ? "Lava World" : "Terrestrial";
            typeSource = " [small mass, likely rocky]";
        }
    }
    // PRIORITY 4: Radius-based (for transit planets)
    else if (radiusEarth > 0) {
        if (radiusEarth > 6.0) {
            planetType = "Gas Giant";
        } else if (radiusEarth > 2.0) {
            planetType = "Neptune-like";
        } else if (radiusEarth > 1.6) {
            planetType = (tempK > 1000) ? "Super-Earth" : "Neptune-like";
        } else if (radiusEarth > 1.25) {
            planetType = "Super-Earth";
        } else {
            planetType = "Terrestrial";
        }
        typeSource = std::format(" [radius {:.1f} R_Earth]", radiusEarth);
    }

    // Add planet type as DATA at the TOP - the AI should use this to guide its output
    prompt += std::format("\n*** PLANET TYPE: {}{} ***{}\n\n", planetType, typeSource, estimatedRadius);

    if (data.mass_earth.hasValue()) {
        prompt += std::format("Mass: {:.3f} Earth masses", data.mass_earth.value);
        if (data.mass_earth.value > 318) {
            prompt += std::format(" ({:.2f} Jupiter masses)", data.mass_earth.value / 318.0);
        }
        if (data.mass_earth.uncertainty.has_value() && data.mass_earth.uncertainty.value() > 0) {
            prompt += std::format(" (±{:.3f})", data.mass_earth.uncertainty.value());
        }
        prompt += "\n";
    }

    if (data.radius_earth.hasValue()) {
        prompt += std::format("Radius: {:.3f} Earth radii", data.radius_earth.value);
        if (data.radius_earth.value > 11) {
            prompt += std::format(" ({:.2f} Jupiter radii)", data.radius_earth.value / 11.2);
        }
        if (data.radius_earth.uncertainty.has_value() && data.radius_earth.uncertainty.value() > 0) {
            prompt += std::format(" (±{:.3f})", data.radius_earth.uncertainty.value());
        }
        prompt += "\n";
    } else if (massEarth > 10) {
        // Show estimated radius for massive planets without measured radius
        prompt += std::format("Radius: UNKNOWN (estimated {:.1f} Earth radii from mass-radius relation)\n", radiusEarth);
    }

    if (data.equilibrium_temp_k.hasValue()) {
        prompt += std::format("Equilibrium Temperature: {:.0f} K", data.equilibrium_temp_k.value);
        if (data.equilibrium_temp_k.uncertainty.has_value() && data.equilibrium_temp_k.uncertainty.value() > 0) {
            prompt += std::format(" (±{:.0f})", data.equilibrium_temp_k.uncertainty.value());
        }
        prompt += "\n";
    }

    if (data.surface_gravity_g.hasValue()) {
        prompt += std::format("Surface Gravity: {:.2f} g\n", data.surface_gravity_g.value);
    }

    if (data.density_gcc.hasValue()) {
        prompt += std::format("Density: {:.2f} g/cm³\n", data.density_gcc.value);
    }

    // Insolation - CRITICAL for habitability
    if (data.insolation_flux.hasValue()) {
        prompt += std::format("Insolation Flux: {:.2f} Earth flux (solar energy received)\n", data.insolation_flux.value);
    }

    if (data.orbital_period_days.hasValue()) {
        prompt += std::format("Orbital Period: {:.2f} days\n", data.orbital_period_days.value);
    }

    if (data.semi_major_axis_au.hasValue()) {
        prompt += std::format("Semi-major Axis: {:.4f} AU\n", data.semi_major_axis_au.value);
    }

    if (data.eccentricity.hasValue()) {
        prompt += std::format("Eccentricity: {:.3f}\n", data.eccentricity.value);
    }

    if (data.inclination_deg.hasValue()) {
        prompt += std::format("Orbital Inclination: {:.1f}°\n", data.inclination_deg.value);
    }

    // Transit/atmosphere indicators
    if (data.transit_depth.hasValue() || data.tsm.hasValue() || data.esm.hasValue()) {
        prompt += "\n=== ATMOSPHERE INDICATORS ===\n";
        if (data.transit_depth.hasValue()) {
            prompt += std::format("Transit Depth: {:.4f}% (light blocked during transit)\n", data.transit_depth.value * 100);
        }
        if (data.tsm.hasValue()) {
            prompt += std::format("TSM: {:.1f} (Transmission Spectroscopy Metric - higher = better for atmosphere detection)\n", data.tsm.value);
        }
        if (data.esm.hasValue()) {
            prompt += std::format("ESM: {:.1f} (Emission Spectroscopy Metric)\n", data.esm.value);
        }
    }

    // Host star info
    prompt += "\n=== HOST STAR ===\n";
    prompt += std::format("Star Name: {}\n", data.host_star.name);
    if (!data.host_star.spectral_type.empty()) {
        prompt += std::format("Spectral Type: {}\n", data.host_star.spectral_type);
    }
    if (data.host_star.effective_temp_k.hasValue()) {
        double starTemp = data.host_star.effective_temp_k.value;
        prompt += std::format("Star Temperature: {:.0f} K\n", starTemp);

        // Add explicit color guidance for non-Sun-like stars
        if (starTemp < 4000) {
            prompt += "\n╔══════════════════════════════════════════════════════════════════╗\n";
            prompt += "║  *** RED DWARF HOST - MANDATORY COLOR SHIFT ***                  ║\n";
            prompt += "╚══════════════════════════════════════════════════════════════════╝\n";
            prompt += std::format("This is an M-type RED DWARF ({:.0f}K) - DEEP RED/ORANGE starlight!\n", starTemp);
            prompt += "YOU MUST USE WARM/RED-TINTED COLORS:\n";
            prompt += "  - atmosphereColor: [0.8-0.9, 0.3-0.5, 0.15-0.3] (orange/red, NOT blue!)\n";
            prompt += "  - sunColor: [1.0, 0.5, 0.3] (deep red starlight)\n";
            prompt += "  - All surface colors shifted toward red/orange tones\n";
            prompt += "  - Ice/clouds: pinkish/orange tint, not pure white\n";
            prompt += "  - NO BLUE ATMOSPHERE - blue scattering doesn't work with red light!\n\n";
        } else if (starTemp < 5000) {
            prompt += std::format("\nThis is a K-type ORANGE DWARF ({:.0f}K) - warmer orange-tinted light\n", starTemp);
            prompt += "  - Use slightly warmer atmosphere colors than Earth\n";
            prompt += "  - sunColor: [1.0, 0.8, 0.6]\n\n";
        }
    }
    if (data.host_star.luminosity_solar.hasValue()) {
        prompt += std::format("Star Luminosity: {:.3f} L☉\n", data.host_star.luminosity_solar.value);
    }
    if (data.host_star.radius_solar.hasValue()) {
        prompt += std::format("Star Radius: {:.3f} R☉\n", data.host_star.radius_solar.value);
    }
    if (data.host_star.metallicity.hasValue()) {
        prompt += std::format("Metallicity [Fe/H]: {:.2f} (affects planet composition - high = more rocky/metallic)\n", data.host_star.metallicity.value);
    }
    if (data.host_star.age_gyr.hasValue()) {
        prompt += std::format("Star Age: {:.1f} Gyr (affects planet evolution)\n", data.host_star.age_gyr.value);
    }
    if (data.host_star.rotation_period_days.hasValue()) {
        prompt += std::format("Star Rotation: {:.1f} days (fast = more active/flares)\n", data.host_star.rotation_period_days.value);
    }

    // DETECTED atmospheric composition (from actual spectroscopy - ExoMAST/Exoplanet.eu)
    // This is REAL DATA from telescopes like JWST, HST - very high confidence!
    if (data.atmosphere_composition.hasValue() &&
        data.atmosphere_composition.source == DataSource::EXOATMOS) {
        prompt += "\n╔══════════════════════════════════════════════════════════════════╗\n";
        prompt += "║  *** DETECTED MOLECULES - MANDATORY COLOR OVERRIDE ***           ║\n";
        prompt += "╚══════════════════════════════════════════════════════════════════╝\n";
        prompt += std::format("SPECTROSCOPY DETECTION: {}\n", data.atmosphere_composition.value);
        prompt += "\nAPPLY MOLECULAR ABSORPTION PHYSICS:\n";

        // Parse detected molecules and give explicit guidance
        std::string molecules = data.atmosphere_composition.value;
        std::string moleculesLower = molecules;
        std::transform(moleculesLower.begin(), moleculesLower.end(), moleculesLower.begin(), ::tolower);

        double temp = data.equilibrium_temp_k.hasValue() ? data.equilibrium_temp_k.value : 0;
        bool hasTempData = data.equilibrium_temp_k.hasValue();

        // CARBON COMPOUNDS
        if (moleculesLower.find("ch4") != std::string::npos ||
            moleculesLower.find("methane") != std::string::npos) {
            prompt += "→ CH4 (METHANE): Absorbs RED (619nm, 725nm, 890nm)\n";
            if (hasTempData) {
                if (temp >= 400 && temp <= 700) {
                    prompt += std::format("  At {:.0f}K: MANDATORY atmosphereColor = MAGENTA/PINK [0.8, 0.3, 0.6]\n", temp);
                } else if (temp < 400 && temp >= 200) {
                    prompt += std::format("  At {:.0f}K: MANDATORY atmosphereColor = DEEP BLUE/CYAN [0.2, 0.5, 0.9]\n", temp);
                } else if (temp < 200) {
                    prompt += std::format("  At {:.0f}K: MANDATORY atmosphereColor = PALE BLUE [0.7, 0.85, 0.95]\n", temp);
                } else {
                    prompt += std::format("  At {:.0f}K: CH4 may be dissociating, reduced effect\n", temp);
                }
            }
        }
        if (moleculesLower.find("co2") != std::string::npos) {
            prompt += "→ CO2: Infrared absorber, visible-transparent → no direct color effect\n";
        }
        if (moleculesLower.find("co") != std::string::npos && moleculesLower.find("co2") == std::string::npos) {
            prompt += "→ CO: Infrared absorber, visible-transparent → no direct color effect\n";
        }

        // NITROGEN COMPOUNDS
        if (moleculesLower.find("nh3") != std::string::npos ||
            moleculesLower.find("ammonia") != std::string::npos) {
            prompt += "→ NH3 (AMMONIA): Absorbs BLUE (300-450nm)\n";
            prompt += "  MANDATORY: cloudColor = CREAM/TAN [0.95, 0.9, 0.75], NOT white\n";
        }

        // SULFUR COMPOUNDS
        if (moleculesLower.find("h2s") != std::string::npos) {
            prompt += "→ H2S: Absorbs UV/BLUE → YELLOW-BROWN tint [0.7, 0.55, 0.3]\n";
        }
        if (moleculesLower.find("so2") != std::string::npos) {
            prompt += "→ SO2: Weak blue absorption → PALE YELLOW tint [0.95, 0.92, 0.7]\n";
        }

        // WATER
        if (moleculesLower.find("h2o") != std::string::npos ||
            moleculesLower.find("water") != std::string::npos) {
            prompt += "→ H2O: Mostly visible-transparent → WHITE clouds [1.0, 1.0, 1.0]\n";
            prompt += "  Slight blue tint possible from weak red absorption\n";
        }

        // ALKALI METALS (hot Jupiters)
        if (moleculesLower.find("na") != std::string::npos ||
            moleculesLower.find("sodium") != std::string::npos) {
            prompt += "→ Na (SODIUM): Absorbs YELLOW (589nm D-lines) + blue wings\n";
            prompt += "  MANDATORY: atmosphereColor has ORANGE/RED tint [0.9, 0.5, 0.2]\n";
        }
        if (moleculesLower.find(" k") != std::string::npos ||
            moleculesLower.find("potassium") != std::string::npos ||
            moleculesLower.find(",k") != std::string::npos) {
            prompt += "→ K (POTASSIUM): Absorbs DEEP RED (766-770nm)\n";
            prompt += "  Combined with Na → strong ORANGE atmosphere\n";
        }

        // METAL OXIDES (ultra-hot Jupiters)
        if (moleculesLower.find("tio") != std::string::npos) {
            prompt += "→ TiO: BROAD visible absorption (450-700nm)\n";
            prompt += "  MANDATORY: VERY DARK atmosphere [0.15, 0.1, 0.08], reddish-brown\n";
        }
        if (moleculesLower.find("vo") != std::string::npos) {
            prompt += "→ VO: BROAD visible absorption like TiO\n";
            prompt += "  MANDATORY: VERY DARK atmosphere [0.12, 0.08, 0.06]\n";
        }
        if (moleculesLower.find("feh") != std::string::npos) {
            prompt += "→ FeH: Absorbs RED (800-1000nm) → BLUE-ish dark [0.2, 0.25, 0.35]\n";
        }
        if (moleculesLower.find("alo") != std::string::npos) {
            prompt += "→ AlO: Absorbs BLUE-GREEN → ORANGE-RED [0.85, 0.45, 0.2]\n";
        }

        // HAZES
        if (moleculesLower.find("tholin") != std::string::npos ||
            moleculesLower.find("haze") != std::string::npos) {
            prompt += "→ THOLINS/ORGANICS: Absorbs BLUE → RED-BROWN/ORANGE [0.7, 0.4, 0.2]\n";
        }

        prompt += "\nThese are REAL telescope observations. Apply the physics EXACTLY.\n";
    }

    // AI-inferred data if available (lower confidence than detected)
    if (data.surface_pressure_atm.hasValue() || data.albedo.hasValue() ||
        data.ocean_coverage_fraction.hasValue() ||
        (data.atmosphere_composition.hasValue() && data.atmosphere_composition.source != DataSource::EXOATMOS)) {
        prompt += "\n=== ESTIMATED PROPERTIES ===\n";
        if (data.surface_pressure_atm.hasValue()) {
            prompt += std::format("Surface Pressure: {:.2f} atm\n", data.surface_pressure_atm.value);
        }
        if (data.albedo.hasValue()) {
            prompt += std::format("Albedo: {:.2f}\n", data.albedo.value);
        }
        if (data.ocean_coverage_fraction.hasValue()) {
            prompt += std::format("Ocean Coverage: {:.0f}%\n", data.ocean_coverage_fraction.value * 100);
        }
        if (data.cloud_coverage_fraction.hasValue()) {
            prompt += std::format("Cloud Coverage: {:.0f}%\n", data.cloud_coverage_fraction.value * 100);
        }
        if (data.atmosphere_composition.hasValue() && data.atmosphere_composition.source != DataSource::EXOATMOS) {
            prompt += std::format("Estimated Atmosphere: {}\n", data.atmosphere_composition.value);
        }
    }

    prompt += R"(
=== TASK ===
Generate a UNIQUE, VISUALLY DISTINCTIVE planet based on THIS SPECIFIC exoplanet's data.

CRITICAL: Do NOT just copy an example. Use the actual data values above to create something unique:
1. Look at the EXACT temperature - a 500K planet looks different from 300K or 1200K
2. Look at the EXACT mass/radius - this determines composition and surface features
3. Look at the host star type - affects lighting color and intensity
4. Consider the orbital distance - closer planets are more tidally heated

CLASSIFICATION RULES (use the ACTUAL radius value from the data above):
- radius < 1.2 Earth radii: Rocky/Mercury-like (craters, no atmosphere)
- radius 1.2-1.8: Super-Earth (could have oceans, atmosphere, continents)
- radius 1.8-3.5: Mini-Neptune/Sub-Neptune (thick atmosphere, banding)
- radius 3.5-6: Ice Giant (blue/cyan, moderate banding)
- radius > 6: Gas Giant (strong banding, no surface)

TEMPERATURE EFFECTS (use the ACTUAL temperature from the data above):
- T > 1200K: Lava world - glowing orange/red surface, molten oceans
- T 800-1200K: Hot rocky - dark surface, orange/red atmosphere
- T 400-800K: Hot desert - golden/orange sands, hazy atmosphere
- T 250-400K: Temperate - can have oceans, vegetation, Earth-like
- T 150-250K: Cold - ice caps, pale colors, thin atmosphere
- T < 150K: Frozen - white/blue ice, very thin or no atmosphere

Make the colors BOLD and SATURATED. Avoid grey or washed-out colors.
The planet should look visually interesting and scientifically plausible.

Output ONLY the JSON object. Match the exact format from the examples.)";

    return prompt;
}

}  // namespace astrocore::prompts
