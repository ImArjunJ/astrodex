#include "render/ExoplanetConverter.hpp"
#include "ai/InferenceEngine.hpp"
#include "core/Logger.hpp"
#include <cmath>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <functional>
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <cctype>

namespace astrocore {

std::string ExoplanetConverter::inferPlanetType(const ExoplanetData& exo) {
    // Gather ALL available characterization data (not just mass/radius)
    // Reference: https://science.nasa.gov/exoplanets/how-we-find-and-characterize/

    double massEarth = exo.mass_earth.hasValue() ? exo.mass_earth.value : 0.0;
    double radiusEarth = exo.radius_earth.hasValue() ? exo.radius_earth.value : 0.0;
    double density = exo.density_gcc.hasValue() ? exo.density_gcc.value : 0.0;
    double tempK = exo.equilibrium_temp_k.hasValue() ? exo.equilibrium_temp_k.value : 0.0;
    double insolation = exo.insolation_flux.hasValue() ? exo.insolation_flux.value : 0.0;
    double stellarMet = exo.host_star.metallicity.hasValue() ? exo.host_star.metallicity.value : 0.0;
    // stellarTemp could be used for more advanced classification in the future
    (void)exo.host_star.effective_temp_k;  // Acknowledge we have this data

    // Log what data we have for classification
    LOG_DEBUG("Classifying {} with: mass={:.1f} M_E, radius={:.1f} R_E, density={:.1f}, temp={:.0f}K, insol={:.1f}, [Fe/H]={:.2f}",
              exo.name, massEarth, radiusEarth, density, tempK, insolation, stellarMet);

    // If we have both mass AND radius, calculate density for classification
    if (density == 0.0 && massEarth > 0 && radiusEarth > 0) {
        // Earth: 5.51 g/cm³, mass=1, radius=1
        density = 5.51 * (massEarth / (radiusEarth * radiusEarth * radiusEarth));
        LOG_DEBUG("Calculated density: {:.2f} g/cm³", density);
    }

    // Estimate temperature from insolation if not directly available
    if (tempK == 0.0 && insolation > 0) {
        // T_eq ≈ 278 * (insolation)^0.25 for Earth-like albedo
        tempK = 278.0 * std::pow(insolation, 0.25);
        LOG_DEBUG("Estimated temp from insolation: {:.0f}K", tempK);
    }

    // === CLASSIFICATION LOGIC using ALL available data ===
    // NASA categories: Gas Giant, Neptune-like, Super-Earth, Terrestrial
    // Plus sub-types: Hot Jupiter, Lava World, Ocean World, etc.

    // 1. DEFINITE GAS GIANTS - very high mass or very large radius
    if (massEarth > 100 || radiusEarth > 8.0) {
        if (tempK > 1000) return "Hot Jupiter";
        return "Gas Giant";
    }

    // 2. DENSITY-BASED classification (most reliable when available)
    // Rocky planets: density > 4 g/cm³ (Earth=5.5, Mars=3.9)
    // Gaseous planets: density < 2 g/cm³ (Neptune=1.64, Saturn=0.69)
    // Ice/water worlds: density 2-4 g/cm³
    if (density > 0) {
        if (density < 2.0) {
            // Low density = significant H/He envelope
            if (massEarth > 50 || radiusEarth > 6.0) return "Gas Giant";
            return "Neptune-like";
        }
        if (density >= 2.0 && density < 4.0) {
            // Intermediate density - could be ice-rich or have thin atmosphere
            if (massEarth > 5 || radiusEarth > 2.5) return "Neptune-like";
            if (tempK > 0 && tempK < 350) return "Ocean World";
            return "Super-Earth";
        }
        // density >= 4.0 = rocky composition
        if (tempK > 1500) return "Lava World";
        // Venus-like: hot rocky planets (400-1500K) with likely volcanic activity
        if (tempK > 400 && tempK <= 1500) return "Venus-like";
        // Frozen world: very cold rocky planets
        if (tempK > 0 && tempK < 150) return "Frozen World";
        if (massEarth > 1.5 || radiusEarth > 1.25) return "Super-Earth";
        return "Terrestrial";
    }

    // 3. MASS-BASED classification (for RV-discovered planets without radius)
    if (massEarth > 0 && radiusEarth == 0) {
        // Without radius, use mass + statistical likelihood
        // Planets > 5 M_Earth are statistically likely to have retained H/He envelopes
        if (massEarth > 50) return "Gas Giant";
        if (massEarth > 5) return "Neptune-like";

        // 2-5 M_Earth is ambiguous - use stellar metallicity as hint
        // High metallicity stars more likely to form planets with gas envelopes
        if (massEarth > 2) {
            if (stellarMet > 0.1) return "Neptune-like";  // Metal-rich star
            return "Super-Earth";  // Could be either
        }

        // < 2 M_Earth likely rocky
        if (tempK > 1500) return "Lava World";
        if (tempK > 400) return "Venus-like";
        return "Terrestrial";
    }

    // 4. RADIUS-BASED classification (for transit-discovered planets)
    if (radiusEarth > 0) {
        // The "Fulton gap" at 1.5-2.0 R_Earth separates rocky from gaseous
        if (radiusEarth > 6.0) return "Gas Giant";
        if (radiusEarth > 3.5) return "Neptune-like";
        if (radiusEarth > 2.0) return "Neptune-like";  // Sub-Neptune
        if (radiusEarth > 1.6) {
            // In the transition zone - temperature matters
            if (tempK > 1000) return "Super-Earth";  // Lost atmosphere
            return "Neptune-like";  // Likely has atmosphere
        }
        // Smaller rocky planets - use temperature for sub-classification
        if (tempK > 1500) return "Lava World";
        if (tempK > 400) return "Venus-like";
        if (tempK > 0 && tempK < 150) return "Frozen World";
        if (radiusEarth > 1.25) return "Super-Earth";
        return "Terrestrial";
    }

    // 5. TEMPERATURE-BASED fallback
    if (tempK > 1500) return "Lava World";
    if (tempK > 400) return "Venus-like";  // Hot rocky = volcanic
    if (tempK > 0 && tempK < 350 && insolation > 0.3 && insolation < 1.5) {
        return "Terrestrial";  // Potentially habitable zone
    }

    // 6. Final fallback
    return "Unknown";
}

PlanetParams ExoplanetConverter::toPlanetParams(const ExoplanetData& exo, InferenceEngine* engine) {
    // Log the planet type from DATA (NASA classification or inferred)
    std::string planetType = exo.planet_type.hasValue() ? exo.planet_type.value : inferPlanetType(exo);
    std::string typeSource = exo.planet_type.hasValue() ? "NASA" : "inferred";
    LOG_INFO("Planet {} type: {} (source: {})", exo.name, planetType, typeSource);

    // PRIORITY 1: Try to load pre-cached params (highest quality, Claude Sonnet generated)
    auto cachedParams = loadCachedParams(exo.name);
    if (cachedParams) {
        LOG_INFO("Using cached render params for {}", exo.name);
        // Still validate/correct in case of stellar type issues
        validateAndCorrectParams(*cachedParams, exo, planetType);
        return *cachedParams;
    }

    // PRIORITY 2: Try AI-based generation
    if (engine && engine->isAvailable()) {
        LOG_INFO("Generating AI render params for {}", exo.name);
        auto aiParams = engine->generateRenderParams(exo);
        if (aiParams) {
            // Validate and auto-correct AI output
            validateAndCorrectParams(*aiParams, exo, planetType);
            LOG_INFO("Successfully generated AI render params for {}", exo.name);
            return *aiParams;
        }
        LOG_WARN("AI render params generation failed, falling back to heuristics for {}", exo.name);
    }

    // PRIORITY 3: Fall back to heuristic-based conversion
    return toHeuristicParams(exo);
}

PlanetParams ExoplanetConverter::toHeuristicParams(const ExoplanetData& exo) {
    std::string type;

    // PRIORITY 1: Use NASA's planet_type if available (this is authoritative DATA)
    if (exo.planet_type.hasValue() && !exo.planet_type.value.empty()) {
        type = exo.planet_type.value;
        LOG_INFO("Converting {} using NASA planet type: {}", exo.name, type);
    }
    // PRIORITY 2: Use AI-inferred biome if available
    else if (exo.biome_classification.hasValue() && exo.biome_classification.isAIInferred()) {
        type = exo.biome_classification.value;
        LOG_INFO("Converting {} using AI-inferred type: {}", exo.name, type);
    }
    // PRIORITY 3: Fall back to computed inference
    else {
        type = inferPlanetType(exo);
        LOG_INFO("Converting {} using computed type: {}", exo.name, type);
    }

    // Normalize type string for matching (lowercase, check for keywords)
    std::string typeLower = type;
    std::transform(typeLower.begin(), typeLower.end(), typeLower.begin(), ::tolower);

    PlanetParams params;

    // Match by keywords in the type string (handles NASA's various naming conventions)
    if (typeLower.find("lava") != std::string::npos || typeLower.find("molten") != std::string::npos) {
        params = createLavaWorldParams(exo);
    } else if (typeLower.find("venus") != std::string::npos || typeLower.find("volcanic") != std::string::npos ||
               typeLower.find("sulfur") != std::string::npos || typeLower.find("inferno") != std::string::npos) {
        params = createVenusLikeParams(exo);
    } else if (typeLower.find("jupiter") != std::string::npos || typeLower.find("jovian") != std::string::npos ||
               (typeLower.find("gas") != std::string::npos && typeLower.find("giant") != std::string::npos)) {
        params = createGasGiantParams(exo);
    } else if (typeLower.find("ice giant") != std::string::npos ||
               (typeLower.find("ice") != std::string::npos && typeLower.find("giant") != std::string::npos)) {
        params = createIceGiantParams(exo);
    } else if (typeLower.find("neptune") != std::string::npos || typeLower.find("neptunian") != std::string::npos ||
               typeLower.find("sub-neptune") != std::string::npos || typeLower.find("mini-neptune") != std::string::npos) {
        // Neptune-like, Neptune-size, Sub-Neptune, Mini-Neptune all map to Mini-Neptune params
        params = createMiniNeptuneParams(exo);
    } else if (typeLower.find("super-earth") != std::string::npos || typeLower.find("super earth") != std::string::npos ||
               typeLower.find("terrestrial") != std::string::npos) {
        params = createSuperEarthParams(exo);
    } else if (typeLower.find("ocean") != std::string::npos || typeLower.find("water") != std::string::npos) {
        params = createOceanWorldParams(exo);
    } else if (typeLower.find("desert") != std::string::npos || typeLower.find("arid") != std::string::npos) {
        params = createDesertWorldParams(exo);
    } else if (typeLower.find("frozen") != std::string::npos || typeLower.find("ice world") != std::string::npos ||
               typeLower.find("icy") != std::string::npos || typeLower.find("snowball") != std::string::npos) {
        params = createFrozenWorldParams(exo);
    } else if (typeLower.find("rocky") != std::string::npos || typeLower.find("earth") != std::string::npos) {
        params = createRockyParams(exo);
    } else {
        // Unknown type - use mass/radius to pick best match
        LOG_WARN("Unknown planet type '{}' for {}, using mass-based heuristic", type, exo.name);
        params = createRockyParams(exo);  // Safe default
    }

    // Apply AI-inferred parameters if available (atmosphere composition, ocean coverage, etc.)
    applyAIInferredParams(params, exo);

    // Apply temperature-based color modifications (planet equilibrium temp)
    if (exo.equilibrium_temp_k.hasValue()) {
        applyTemperatureColors(params, exo.equilibrium_temp_k.value);
    }

    // Apply stellar type color shift (host star temperature affects lighting)
    if (exo.host_star.effective_temp_k.hasValue()) {
        applyStellarTypeColors(params, exo.host_star.effective_temp_k.value);
    }

    // Scale radius based on planet size (Earth = 2.0 render units)
    if (exo.radius_earth.hasValue()) {
        params.radius = 2.0f * static_cast<float>(exo.radius_earth.value);
        params.radius = std::clamp(params.radius, 1.0f, 10.0f);
    }

    return params;
}

PlanetParams ExoplanetConverter::createRockyParams(const ExoplanetData& exo) {
    PlanetParams p;
    p.noiseStrength = 0.035f;
    p.terrainScale = 0.8f;
    p.craterStrength = 0.5f;
    p.ridgedStrength = 0.25f;

    // Rich rust/ochre rocky surface - NOT grey!
    p.sandColor = {0.75f, 0.55f, 0.35f};   // Golden tan
    p.treeColor = {0.55f, 0.4f, 0.25f};    // Rust brown
    p.rockColor = {0.4f, 0.28f, 0.18f};    // Deep rust
    p.iceColor = {0.9f, 0.88f, 0.82f};     // Warm white

    p.atmosphereDensity = 0.08f;
    p.atmosphereColor = {0.6f, 0.4f, 0.3f}; // Dusty atmosphere
    p.cloudsDensity = 0.0f;
    p.waterLevel = -0.2f;
    p.sunIntensity = 3.0f;

    return p;
}

PlanetParams ExoplanetConverter::createSuperEarthParams(const ExoplanetData& exo) {
    PlanetParams p;
    p.noiseStrength = 0.025f;
    p.terrainScale = 0.9f;
    p.continentScale = 1.2f;
    p.continentBlend = 0.2f;

    // VIBRANT Earth-like colors
    p.waterColorDeep = {0.01f, 0.06f, 0.25f};     // Deep rich blue
    p.waterColorSurface = {0.02f, 0.15f, 0.45f};   // Bright blue
    p.sandColor = {0.95f, 0.85f, 0.55f};           // Golden sand
    p.treeColor = {0.05f, 0.35f, 0.12f};           // Lush green
    p.rockColor = {0.45f, 0.35f, 0.25f};           // Warm brown
    p.iceColor = {0.95f, 0.98f, 1.0f};             // Crisp white

    p.atmosphereColor = {0.2f, 0.5f, 0.95f};       // Bright blue sky
    p.atmosphereDensity = 0.4f;
    p.cloudsDensity = 0.5f;
    p.cloudColor = {1.0f, 1.0f, 1.0f};
    p.waterLevel = 0.05f;
    p.sunIntensity = 2.8f;

    return p;
}

PlanetParams ExoplanetConverter::createMiniNeptuneParams(const ExoplanetData& exo) {
    PlanetParams p;
    p.noiseStrength = 0.025f;
    p.terrainScale = 0.5f;
    p.bandingStrength = 0.5f;
    p.bandingFrequency = 18.0f;
    p.domainWarpStrength = 0.2f;

    // VIBRANT teal/cyan atmosphere
    p.atmosphereColor = {0.15f, 0.6f, 0.85f};      // Bright teal
    p.atmosphereDensity = 0.65f;
    p.cloudsDensity = 0.6f;
    p.cloudsScale = 1.8f;
    p.cloudColor = {0.7f, 0.9f, 0.95f};            // Cyan-tinted clouds

    p.sandColor = {0.25f, 0.55f, 0.75f};           // Bright blue
    p.treeColor = {0.15f, 0.45f, 0.65f};           // Deep teal
    p.rockColor = {0.1f, 0.35f, 0.55f};            // Rich blue
    p.iceColor = {0.7f, 0.9f, 1.0f};               // Ice blue

    p.waterLevel = -0.5f;
    p.sunIntensity = 2.5f;

    return p;
}

PlanetParams ExoplanetConverter::createGasGiantParams(const ExoplanetData& exo) {
    PlanetParams p;
    p.noiseStrength = 0.008f;
    p.terrainScale = 0.6f;
    p.bandingStrength = 0.9f;
    p.bandingFrequency = 28.0f;
    p.domainWarpStrength = 0.45f;

    // BOLD Jupiter-like bands with high contrast
    p.sandColor = {0.95f, 0.8f, 0.5f};             // Bright cream/gold
    p.treeColor = {0.75f, 0.5f, 0.25f};            // Rich orange-brown
    p.rockColor = {0.4f, 0.25f, 0.12f};            // Deep brown bands
    p.iceColor = {1.0f, 0.95f, 0.85f};             // Bright cream

    p.atmosphereColor = {0.65f, 0.45f, 0.3f};      // Warm orange glow
    p.atmosphereDensity = 0.7f;
    p.cloudsDensity = 0.85f;
    p.cloudsScale = 2.5f;
    p.cloudsSpeed = 2.5f;
    p.cloudColor = {1.0f, 0.95f, 0.8f};

    p.waterLevel = -1.0f;
    p.sunIntensity = 2.8f;

    // STORM SYSTEM - Dramatic fluid dynamics
    p.stormCount = 5.0f;                           // Multiple storms for variety
    p.stormSize = 0.22f;                           // Larger, more visible storms
    p.stormIntensity = 1.0f;                       // Full intensity vortices
    p.stormSeed = static_cast<float>(std::hash<std::string>{}(exo.name) % 1000) / 100.0f;
    p.stormColor = {0.9f, 0.5f, 0.25f};            // Vivid red-orange storm color
    p.flowSpeed = 1.5f;                            // Fast active flow
    p.turbulenceScale = 1.4f;                      // Strong turbulence for eddies
    p.vortexTightness = 4.0f;                      // Tight dramatic spirals

    return p;
}

PlanetParams ExoplanetConverter::createIceGiantParams(const ExoplanetData& exo) {
    PlanetParams p;
    p.noiseStrength = 0.006f;
    p.terrainScale = 0.5f;
    p.bandingStrength = 0.45f;
    p.bandingFrequency = 15.0f;
    p.domainWarpStrength = 0.2f;

    // VIBRANT Neptune/Uranus blue-cyan
    p.atmosphereColor = {0.2f, 0.55f, 0.95f};      // Brilliant blue
    p.atmosphereDensity = 0.65f;
    p.cloudsDensity = 0.5f;
    p.cloudColor = {0.6f, 0.85f, 1.0f};            // Cyan clouds

    p.sandColor = {0.35f, 0.65f, 0.9f};            // Bright azure
    p.treeColor = {0.25f, 0.55f, 0.85f};           // Deep azure
    p.rockColor = {0.15f, 0.4f, 0.7f};             // Rich blue
    p.iceColor = {0.75f, 0.92f, 1.0f};             // Bright ice blue

    p.waterLevel = -1.0f;
    p.sunIntensity = 2.5f;

    // STORM SYSTEM - Neptune-style with dramatic dark spots
    p.stormCount = 4.0f;                           // More storms for visual interest
    p.stormSize = 0.18f;                           // Larger storms
    p.stormIntensity = 0.9f;                       // Strong intensity
    p.stormSeed = static_cast<float>(std::hash<std::string>{}(exo.name) % 1000) / 100.0f;
    p.stormColor = {0.08f, 0.2f, 0.45f};           // Deep blue storm color
    p.flowSpeed = 2.0f;                            // Very fast winds (Neptune has fastest)
    p.turbulenceScale = 1.3f;                      // Strong turbulence
    p.vortexTightness = 3.0f;                      // Medium tight spirals

    return p;
}

PlanetParams ExoplanetConverter::createLavaWorldParams(const ExoplanetData& exo) {
    PlanetParams p;
    p.noiseStrength = 0.15f;
    p.terrainScale = 1.0f;
    p.domainWarpStrength = 0.7f;
    p.fbmExponentiation = 2.5f;
    p.ridgedStrength = 0.6f;
    p.craterStrength = 0.3f;

    // INTENSE GLOWING lava colors
    p.waterColorDeep = {0.7f, 0.15f, 0.0f};        // Deep orange lava
    p.waterColorSurface = {1.0f, 0.5f, 0.0f};      // Bright orange-yellow
    p.sandColor = {0.12f, 0.06f, 0.03f};           // Dark basalt
    p.treeColor = {0.08f, 0.04f, 0.02f};           // Near black
    p.rockColor = {0.05f, 0.02f, 0.01f};           // Charred black
    p.iceColor = {1.0f, 0.7f, 0.2f};               // Glowing highlights

    p.atmosphereColor = {0.9f, 0.35f, 0.1f};       // Fiery orange atmosphere
    p.atmosphereDensity = 0.5f;
    p.cloudsDensity = 0.15f;
    p.cloudColor = {0.6f, 0.25f, 0.1f};            // Smoke/ash clouds

    p.sunIntensity = 4.5f;
    p.ambientLight = 0.02f;  // Extra ambient from lava glow
    p.waterLevel = 0.1f;     // Lava "oceans"

    return p;
}

PlanetParams ExoplanetConverter::createVenusLikeParams(const ExoplanetData& exo) {
    PlanetParams p;
    p.noiseStrength = 0.08f;
    p.terrainScale = 1.2f;
    p.domainWarpStrength = 0.5f;
    p.fbmExponentiation = 3.0f;
    p.ridgedStrength = 0.4f;
    p.craterStrength = 0.35f;  // Shield volcanoes / calderas

    // VENUS-LIKE VOLCANIC surface - ochre/orange/brown volcanic terrain
    p.sandColor = {0.85f, 0.6f, 0.25f};            // Golden ochre volcanic plains
    p.treeColor = {0.65f, 0.35f, 0.12f};           // Deep rust volcanic highlands
    p.rockColor = {0.35f, 0.18f, 0.08f};           // Dark basalt flows
    p.iceColor = {0.95f, 0.7f, 0.3f};              // Hot glow highlights (not actual ice)

    // Lava "lakes" - not full oceans but visible magma
    p.waterColorDeep = {0.6f, 0.15f, 0.0f};        // Deep red-orange lava
    p.waterColorSurface = {0.9f, 0.4f, 0.05f};     // Bright orange surface glow
    p.waterLevel = -0.05f;                          // Low lava coverage (volcanic vents)

    // THICK SULFUROUS ATMOSPHERE - Venus's signature look
    p.atmosphereColor = {0.95f, 0.75f, 0.3f};      // Dense yellow-orange haze
    p.atmosphereDensity = 0.85f;                    // Very thick atmosphere
    p.cloudsDensity = 0.7f;                         // Dense sulfuric acid clouds
    p.cloudsScale = 2.0f;
    p.cloudsSpeed = 0.8f;                           // Slow super-rotation
    p.cloudColor = {0.95f, 0.85f, 0.5f};           // Yellow-white sulfur clouds
    p.cloudAltitude = 0.35f;
    p.cloudThickness = 0.3f;

    p.sunIntensity = 3.5f;
    p.ambientLight = 0.015f;                        // Some glow from hot surface

    return p;
}

PlanetParams ExoplanetConverter::createOceanWorldParams(const ExoplanetData& exo) {
    PlanetParams p;
    p.noiseStrength = 0.012f;
    p.terrainScale = 0.7f;
    p.fbmExponentiation = 6.0f;
    p.continentScale = 1.5f;

    // RICH, DEEP ocean colors
    p.waterColorDeep = {0.005f, 0.04f, 0.25f};     // Very deep blue
    p.waterColorSurface = {0.02f, 0.12f, 0.45f};   // Vivid blue surface
    p.sandColor = {0.98f, 0.95f, 0.8f};            // Bright beach sand
    p.treeColor = {0.02f, 0.18f, 0.08f};           // Tropical green
    p.rockColor = {0.2f, 0.15f, 0.1f};             // Wet rock
    p.iceColor = {0.95f, 0.98f, 1.0f};             // Bright ice

    p.atmosphereColor = {0.15f, 0.5f, 0.95f};      // Brilliant blue atmosphere
    p.atmosphereDensity = 0.45f;
    p.cloudsDensity = 0.6f;
    p.cloudColor = {1.0f, 1.0f, 1.0f};             // Bright white clouds

    p.waterLevel = 0.15f;  // High water level
    p.sunIntensity = 2.8f;

    return p;
}

PlanetParams ExoplanetConverter::createDesertWorldParams(const ExoplanetData& exo) {
    PlanetParams p;
    p.noiseStrength = 0.025f;
    p.terrainScale = 0.9f;
    p.fbmLacunarity = 1.8f;
    p.fbmExponentiation = 4.5f;
    p.ridgedStrength = 0.15f;

    // GOLDEN, DRAMATIC desert colors
    p.sandColor = {0.98f, 0.8f, 0.45f};            // Bright golden sand
    p.treeColor = {0.75f, 0.5f, 0.2f};             // Deep orange dunes
    p.rockColor = {0.55f, 0.35f, 0.15f};           // Rich rust rock
    p.iceColor = {1.0f, 0.95f, 0.85f};             // Warm white

    p.atmosphereColor = {0.85f, 0.55f, 0.2f};      // Dusty orange sky
    p.atmosphereDensity = 0.2f;
    p.cloudsDensity = 0.08f;
    p.cloudColor = {0.95f, 0.85f, 0.7f};           // Dusty clouds

    p.sunIntensity = 4.0f;
    p.waterLevel = -0.2f;

    return p;
}

PlanetParams ExoplanetConverter::createFrozenWorldParams(const ExoplanetData& exo) {
    PlanetParams p;
    p.noiseStrength = 0.02f;
    p.terrainScale = 0.7f;
    p.fbmExponentiation = 3.5f;
    p.ridgedStrength = 0.25f;      // Ice ridges
    p.craterStrength = 0.2f;

    // ICY surface - Europa/Enceladus style
    p.sandColor = {0.85f, 0.9f, 0.95f};            // Light ice
    p.treeColor = {0.7f, 0.78f, 0.85f};            // Blue-grey ice
    p.rockColor = {0.5f, 0.55f, 0.65f};            // Dark ice/rock
    p.iceColor = {0.95f, 0.98f, 1.0f};             // Pure white ice

    // Frozen methane/nitrogen "lakes" (like Titan)
    p.waterColorDeep = {0.15f, 0.25f, 0.4f};       // Deep dark frozen liquid
    p.waterColorSurface = {0.25f, 0.35f, 0.5f};    // Frozen surface
    p.waterLevel = -0.1f;

    // Thin, cold atmosphere
    p.atmosphereColor = {0.6f, 0.75f, 0.95f};      // Pale blue
    p.atmosphereDensity = 0.15f;
    p.cloudsDensity = 0.1f;
    p.cloudColor = {0.9f, 0.95f, 1.0f};

    p.polarCapSize = 0.8f;                          // Large ice caps
    p.sunIntensity = 1.8f;                          // Dim distant star
    p.ambientLight = 0.005f;                        // Very dark

    return p;
}

void ExoplanetConverter::validateAndCorrectParams(PlanetParams& params, const ExoplanetData& exo, const std::string& planetType) {
    // Auto-correct AI output if it doesn't make physical sense
    std::string typeLower = planetType;
    std::transform(typeLower.begin(), typeLower.end(), typeLower.begin(), ::tolower);

    // 1. RED DWARF CORRECTION: If star is M-type (<3500K), atmosphere CAN'T be blue
    if (exo.host_star.effective_temp_k.hasValue() && exo.host_star.effective_temp_k.value < 3500) {
        float starTemp = static_cast<float>(exo.host_star.effective_temp_k.value);

        // Check if atmosphere is too blue (blue > red by significant margin)
        if (params.atmosphereColor.b > params.atmosphereColor.r + 0.2f) {
            LOG_WARN("Correcting blue atmosphere for red dwarf star ({:.0f}K)", starTemp);
            // Swap to warm colors
            float avgBrightness = (params.atmosphereColor.r + params.atmosphereColor.g + params.atmosphereColor.b) / 3.0f;
            params.atmosphereColor = glm::vec3(
                std::min(1.0f, avgBrightness + 0.3f),  // Boost red
                avgBrightness * 0.6f,                   // Reduce green
                avgBrightness * 0.3f                    // Strongly reduce blue
            );
        }

        // Force warm sun color for red dwarf
        params.sunColor = glm::vec3(1.0f, 0.5f, 0.3f);
    }

    // 2. GAS/ICE GIANT CORRECTION: Must have banding
    bool isGiant = typeLower.find("giant") != std::string::npos ||
                   typeLower.find("jupiter") != std::string::npos ||
                   typeLower.find("neptune") != std::string::npos;
    if (isGiant && params.bandingStrength < 0.3f) {
        LOG_WARN("Correcting low banding ({:.2f}) for {} type", params.bandingStrength, planetType);
        params.bandingStrength = 0.6f;
        params.bandingFrequency = std::max(params.bandingFrequency, 15.0f);
    }

    // 3. ROCKY/TERRESTRIAL CORRECTION: Should NOT have banding
    bool isRocky = typeLower.find("terrestrial") != std::string::npos ||
                   typeLower.find("rocky") != std::string::npos ||
                   typeLower.find("earth") != std::string::npos ||
                   typeLower.find("super-earth") != std::string::npos;
    if (isRocky && params.bandingStrength > 0.1f) {
        LOG_WARN("Correcting banding ({:.2f}) for rocky planet", params.bandingStrength);
        params.bandingStrength = 0.0f;
    }

    // 4. CLOUD DENSITY CAP (too high looks bad)
    if (params.cloudsDensity > 0.02f) {
        LOG_DEBUG("Capping cloud density from {:.3f} to 0.02", params.cloudsDensity);
        params.cloudsDensity = 0.02f;
    }

    // 5. Apply stellar color shift after validation
    if (exo.host_star.effective_temp_k.hasValue()) {
        applyStellarTypeColors(params, exo.host_star.effective_temp_k.value);
    }
}

void ExoplanetConverter::applyTemperatureColors(PlanetParams& params, double tempK) {
    // Very hot - shift toward reds/oranges
    if (tempK > 800) {
        float heat = static_cast<float>(std::min(1.0, (tempK - 800) / 1000.0));
        params.atmosphereColor.r = std::min(1.0f, params.atmosphereColor.r + heat * 0.5f);
        params.atmosphereColor.g = params.atmosphereColor.g * (1.0f - heat * 0.3f);
        params.atmosphereColor.b = params.atmosphereColor.b * (1.0f - heat * 0.5f);
    }
    // Very cold - shift toward blues
    else if (tempK < 200) {
        float cold = static_cast<float>(std::min(1.0, (200 - tempK) / 150.0));
        params.atmosphereColor.b = std::min(1.0f, params.atmosphereColor.b + cold * 0.3f);
        params.iceColor = {0.85f + cold * 0.1f, 0.9f + cold * 0.05f, 1.0f};
        params.polarCapSize = std::min(1.0f, params.polarCapSize + cold * 0.4f);
    }
}

void ExoplanetConverter::applyStellarTypeColors(PlanetParams& params, double starTempK) {
    // Shift ALL colors based on host star type
    // Red dwarfs (M-type): 2500-3500K -> deep red/orange light
    // Orange dwarfs (K-type): 3500-5000K -> orange-tinted light
    // Yellow (G-type): 5000-6000K -> our Sun, white/yellow (no shift needed)
    // White (F-type): 6000-7500K -> slightly blue-white
    // Blue-white (A-type): 7500-10000K -> blue-tinted

    if (starTempK < 3500) {
        // RED DWARF - everything gets warm red/orange tint
        float redShift = static_cast<float>((3500.0 - starTempK) / 1000.0);  // 0-1 scale
        redShift = std::clamp(redShift, 0.0f, 1.0f);

        LOG_DEBUG("Applying red dwarf color shift: {:.2f} for star temp {:.0f}K", redShift, starTempK);

        // Shift atmosphere from blue toward orange/red
        params.atmosphereColor.r = std::min(1.0f, params.atmosphereColor.r + redShift * 0.6f);
        params.atmosphereColor.g = params.atmosphereColor.g * (1.0f - redShift * 0.3f);
        params.atmosphereColor.b = params.atmosphereColor.b * (1.0f - redShift * 0.7f);

        // Shift sun color to deep red
        params.sunColor = glm::vec3(1.0f, 0.5f - redShift * 0.2f, 0.3f - redShift * 0.15f);

        // Shift surface colors warmer
        params.sandColor.r = std::min(1.0f, params.sandColor.r + redShift * 0.2f);
        params.sandColor.b = params.sandColor.b * (1.0f - redShift * 0.4f);

        params.rockColor.r = std::min(1.0f, params.rockColor.r + redShift * 0.15f);
        params.rockColor.b = params.rockColor.b * (1.0f - redShift * 0.3f);

        // Ice/clouds get pinkish tint
        params.iceColor.r = std::min(1.0f, params.iceColor.r + redShift * 0.1f);
        params.iceColor.b = params.iceColor.b * (1.0f - redShift * 0.2f);

        params.cloudColor.r = std::min(1.0f, params.cloudColor.r + redShift * 0.1f);
        params.cloudColor.b = params.cloudColor.b * (1.0f - redShift * 0.15f);

    } else if (starTempK < 5000) {
        // ORANGE DWARF (K-type) - subtle warm shift
        float warmShift = static_cast<float>((5000.0 - starTempK) / 1500.0);
        warmShift = std::clamp(warmShift, 0.0f, 1.0f);

        params.atmosphereColor.r = std::min(1.0f, params.atmosphereColor.r + warmShift * 0.15f);
        params.atmosphereColor.b = params.atmosphereColor.b * (1.0f - warmShift * 0.2f);
        params.sunColor = glm::vec3(1.0f, 0.9f - warmShift * 0.1f, 0.7f - warmShift * 0.15f);

    } else if (starTempK > 7500) {
        // BLUE-WHITE STAR (A/F-type) - cool blue shift
        float blueShift = static_cast<float>((starTempK - 7500.0) / 2500.0);
        blueShift = std::clamp(blueShift, 0.0f, 1.0f);

        params.atmosphereColor.b = std::min(1.0f, params.atmosphereColor.b + blueShift * 0.2f);
        params.sunColor = glm::vec3(0.95f - blueShift * 0.1f, 0.95f, 1.0f);
    }
    // G-type (5000-6000K) = Sun-like, no shift needed
}

void ExoplanetConverter::applyAIInferredParams(PlanetParams& params, const ExoplanetData& exo) {
    // Apply ocean coverage from AI
    if (exo.ocean_coverage_fraction.hasValue()) {
        float oceanFrac = static_cast<float>(exo.ocean_coverage_fraction.value);
        // Higher ocean coverage = higher water level
        params.waterLevel = -0.1f + oceanFrac * 0.25f;
        LOG_DEBUG("Applied AI ocean coverage: {:.1f}% -> water level {:.2f}",
                  oceanFrac * 100, params.waterLevel);
    }

    // Apply cloud coverage from AI
    if (exo.cloud_coverage_fraction.hasValue()) {
        params.cloudsDensity = static_cast<float>(exo.cloud_coverage_fraction.value);
        LOG_DEBUG("Applied AI cloud coverage: {:.1f}%", params.cloudsDensity * 100);
    }

    // Apply albedo to atmosphere density
    if (exo.albedo.hasValue()) {
        float albedo = static_cast<float>(exo.albedo.value);
        // High albedo suggests thick clouds/ice
        if (albedo > 0.5f) {
            params.cloudsDensity = std::max(params.cloudsDensity, albedo * 0.8f);
        }
    }

    // Apply ice coverage
    if (exo.ice_coverage_fraction.hasValue()) {
        float iceFrac = static_cast<float>(exo.ice_coverage_fraction.value);
        params.polarCapSize = iceFrac;
    }

    // Apply surface color hint from AI
    if (exo.surface_color_hint.hasValue() && !exo.surface_color_hint.value.empty()) {
        glm::vec3 hintColor = parseColorHint(exo.surface_color_hint.value);
        if (hintColor != glm::vec3(0.0f)) {
            // Blend hint color with existing colors
            params.sandColor = glm::mix(params.sandColor, hintColor, 0.5f);
            params.rockColor = glm::mix(params.rockColor, hintColor * 0.6f, 0.4f);
            LOG_DEBUG("Applied AI surface color hint: {}", exo.surface_color_hint.value);
        }
    }

    // Apply atmosphere composition hints
    if (exo.atmosphere_composition.hasValue()) {
        const std::string& atmo = exo.atmosphere_composition.value;
        // Adjust atmosphere color based on composition
        if (atmo.find("methane") != std::string::npos ||
            atmo.find("CH4") != std::string::npos) {
            // Methane gives blue/cyan tint
            params.atmosphereColor = glm::mix(params.atmosphereColor,
                                               glm::vec3(0.2f, 0.5f, 0.8f), 0.3f);
        }
        if (atmo.find("sulfur") != std::string::npos ||
            atmo.find("SO2") != std::string::npos) {
            // Sulfurous atmosphere - yellow/orange tint
            params.atmosphereColor = glm::mix(params.atmosphereColor,
                                               glm::vec3(0.8f, 0.6f, 0.2f), 0.3f);
        }
        if (atmo.find("nitrogen") != std::string::npos &&
            atmo.find("oxygen") != std::string::npos) {
            // Earth-like atmosphere
            params.atmosphereColor = glm::mix(params.atmosphereColor,
                                               glm::vec3(0.1f, 0.4f, 0.9f), 0.3f);
        }
    }
}

glm::vec3 ExoplanetConverter::parseColorHint(const std::string& hint) {
    // Parse common color descriptions to RGB
    std::string lower = hint;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower.find("red") != std::string::npos) return {0.8f, 0.3f, 0.2f};
    if (lower.find("orange") != std::string::npos) return {0.9f, 0.5f, 0.2f};
    if (lower.find("yellow") != std::string::npos) return {0.9f, 0.85f, 0.4f};
    if (lower.find("green") != std::string::npos) return {0.3f, 0.7f, 0.3f};
    if (lower.find("blue") != std::string::npos) return {0.3f, 0.5f, 0.9f};
    if (lower.find("purple") != std::string::npos) return {0.6f, 0.3f, 0.8f};
    if (lower.find("brown") != std::string::npos) return {0.6f, 0.4f, 0.2f};
    if (lower.find("tan") != std::string::npos) return {0.8f, 0.7f, 0.5f};
    if (lower.find("white") != std::string::npos) return {0.95f, 0.95f, 0.95f};
    if (lower.find("gray") != std::string::npos ||
        lower.find("grey") != std::string::npos) return {0.5f, 0.5f, 0.5f};
    if (lower.find("black") != std::string::npos) return {0.1f, 0.1f, 0.1f};

    return {0.0f, 0.0f, 0.0f};  // Unknown color
}

std::string ExoplanetConverter::sanitizeFilename(const std::string& name) {
    std::string result;
    result.reserve(name.size());
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-') {
            result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        } else {
            result += '_';
        }
    }
    return result;
}

std::optional<PlanetParams> ExoplanetConverter::loadCachedParams(const std::string& planetName) {
    // Build cache path: ~/.cache/astrosplat/planets/{sanitized_name}.json
    const char* homeDir = std::getenv("HOME");
    if (!homeDir) {
        return std::nullopt;
    }

    std::filesystem::path cachePath = std::filesystem::path(homeDir) /
                                       ".cache" / "astrosplat" / "planets" /
                                       (sanitizeFilename(planetName) + ".json");

    if (!std::filesystem::exists(cachePath)) {
        return std::nullopt;
    }

    try {
        std::ifstream file(cachePath);
        if (!file.is_open()) {
            return std::nullopt;
        }

        nlohmann::json j = nlohmann::json::parse(file);

        PlanetParams params;

        // Parse all fields from cached JSON
        auto getVec3 = [&j](const std::string& key) -> glm::vec3 {
            if (j.contains(key) && j[key].is_array() && j[key].size() >= 3) {
                return glm::vec3(j[key][0].get<float>(), j[key][1].get<float>(), j[key][2].get<float>());
            }
            return glm::vec3(0.0f);
        };

        auto getFloat = [&j](const std::string& key, float def = 0.0f) -> float {
            return j.value(key, def);
        };

        auto getInt = [&j](const std::string& key, int def = 0) -> int {
            return j.value(key, def);
        };

        params.ambientLight = getFloat("ambientLight", 0.01f);
        params.atmosphereColor = getVec3("atmosphereColor");
        params.atmosphereDensity = getFloat("atmosphereDensity", 0.3f);
        params.bandingFrequency = getFloat("bandingFrequency", 20.0f);
        params.bandingStrength = getFloat("bandingStrength", 0.0f);
        params.cloudAltitude = getFloat("cloudAltitude", 0.25f);
        params.cloudColor = getVec3("cloudColor");
        params.cloudThickness = getFloat("cloudThickness", 0.2f);
        params.cloudsDensity = getFloat("cloudsDensity", 0.01f);
        params.cloudsScale = getFloat("cloudsScale", 0.8f);
        params.cloudsSpeed = getFloat("cloudsSpeed", 1.5f);
        params.continentBlend = getFloat("continentBlend", 0.15f);
        params.continentScale = getFloat("continentScale", 0.0f);
        params.craterStrength = getFloat("craterStrength", 0.0f);
        params.deepSpaceColor = getVec3("deepSpaceColor");
        params.domainWarpStrength = getFloat("domainWarpStrength", 0.0f);
        params.fbmExponentiation = getFloat("fbmExponentiation", 5.0f);
        params.fbmLacunarity = getFloat("fbmLacunarity", 2.0f);
        params.fbmOctaves = getInt("fbmOctaves", 6);
        params.fbmPersistence = getFloat("fbmPersistence", 0.5f);
        params.iceColor = getVec3("iceColor");
        params.iceLevel = getFloat("iceLevel", 0.04f);
        params.noiseStrength = getFloat("noiseStrength", 0.02f);
        params.noiseType = static_cast<NoiseType>(getInt("noiseType", 0));
        params.polarCapSize = getFloat("polarCapSize", 0.0f);
        params.quality = getFloat("quality", 1.0f);
        params.radius = getFloat("radius", 1.0f);
        params.ridgedStrength = getFloat("ridgedStrength", 0.0f);
        params.rockColor = getVec3("rockColor");
        params.rockLevel = getFloat("rockLevel", 0.02f);
        params.rotationOffset = getFloat("rotationOffset", 0.6f);
        params.rotationSpeed = getFloat("rotationSpeed", 0.1f);
        params.sandColor = getVec3("sandColor");
        params.sandLevel = getFloat("sandLevel", 0.003f);
        params.sunColor = getVec3("sunColor");
        params.sunDirection = getVec3("sunDirection");
        params.sunIntensity = getFloat("sunIntensity", 3.0f);
        params.terrainScale = getFloat("terrainScale", 0.8f);
        params.transition = getFloat("transition", 0.01f);
        params.treeColor = getVec3("treeColor");
        params.treeLevel = getFloat("treeLevel", 0.004f);
        params.waterColorDeep = getVec3("waterColorDeep");
        params.waterColorSurface = getVec3("waterColorSurface");
        params.waterLevel = getFloat("waterLevel", 0.0f);

        LOG_INFO("Loaded cached render params for {} from {}", planetName, cachePath.string());
        return params;

    } catch (const std::exception& e) {
        LOG_WARN("Failed to load cached params for {}: {}", planetName, e.what());
        return std::nullopt;
    }
}

std::vector<ExoplanetConverter::CachedPlanetInfo> ExoplanetConverter::listCachedPlanets() {
    std::vector<CachedPlanetInfo> results;

    const char* homeDir = std::getenv("HOME");
    if (!homeDir) {
        return results;
    }

    std::filesystem::path cacheDir = std::filesystem::path(homeDir) /
                                      ".cache" / "astrosplat" / "planets";

    if (!std::filesystem::exists(cacheDir)) {
        return results;
    }

    try {
        for (const auto& entry : std::filesystem::directory_iterator(cacheDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                try {
                    std::ifstream file(entry.path());
                    if (file.is_open()) {
                        nlohmann::json j = nlohmann::json::parse(file);
                        CachedPlanetInfo info;
                        info.name = j.value("_planet_name", entry.path().stem().string());
                        info.type = j.value("_planet_type", "Unknown");
                        results.push_back(info);
                    }
                } catch (...) {
                    // Skip invalid JSON files
                }
            }
        }
    } catch (const std::exception& e) {
        LOG_WARN("Failed to list cached planets: {}", e.what());
    }

    // Sort by name
    std::sort(results.begin(), results.end(),
              [](const CachedPlanetInfo& a, const CachedPlanetInfo& b) {
                  return a.name < b.name;
              });

    LOG_INFO("Found {} cached planets", static_cast<int>(results.size()));
    return results;
}

}  // namespace astrocore
