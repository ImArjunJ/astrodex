#pragma once

#include "render/Renderer.hpp"
#include "data/ExoplanetData.hpp"
#include <optional>
#include <string>

namespace astrocore {

class InferenceEngine;

// Converts exoplanet data to procedural planet rendering parameters
class ExoplanetConverter {
public:
    // Convert exoplanet data to PlanetParams for rendering
    // If engine is provided and available, uses AI to generate full params
    static PlanetParams toPlanetParams(const ExoplanetData& exo, InferenceEngine* engine = nullptr);

    // Infer planet type from physical properties
    static std::string inferPlanetType(const ExoplanetData& exo);

    // Fallback heuristic conversion (used when AI unavailable)
    static PlanetParams toHeuristicParams(const ExoplanetData& exo);

    // Load pre-cached render params from ~/.cache/astrosplat/planets/
    static std::optional<PlanetParams> loadCachedParams(const std::string& planetName);

    // List all cached planets (name + type only from cache files)
    struct CachedPlanetInfo {
        std::string name;
        std::string type;
    };
    static std::vector<CachedPlanetInfo> listCachedPlanets();

    // Sanitize planet name for cache filename
    static std::string sanitizeFilename(const std::string& name);

private:
    // Helper functions for different planet types
    static PlanetParams createRockyParams(const ExoplanetData& exo);
    static PlanetParams createSuperEarthParams(const ExoplanetData& exo);
    static PlanetParams createMiniNeptuneParams(const ExoplanetData& exo);
    static PlanetParams createGasGiantParams(const ExoplanetData& exo);
    static PlanetParams createIceGiantParams(const ExoplanetData& exo);
    static PlanetParams createLavaWorldParams(const ExoplanetData& exo);
    static PlanetParams createVenusLikeParams(const ExoplanetData& exo);
    static PlanetParams createOceanWorldParams(const ExoplanetData& exo);
    static PlanetParams createDesertWorldParams(const ExoplanetData& exo);
    static PlanetParams createFrozenWorldParams(const ExoplanetData& exo);

    // Validate AI output and auto-correct obvious errors
    static void validateAndCorrectParams(PlanetParams& params, const ExoplanetData& exo, const std::string& planetType);

    // Apply temperature-based color adjustments (planet equilibrium temp)
    static void applyTemperatureColors(PlanetParams& params, double tempK);

    // Apply stellar type color shift (host star temperature affects lighting)
    static void applyStellarTypeColors(PlanetParams& params, double starTempK);

    // Apply AI-inferred parameters (ocean coverage, clouds, atmosphere, etc.)
    static void applyAIInferredParams(PlanetParams& params, const ExoplanetData& exo);

    // Parse color hint string to RGB
    static glm::vec3 parseColorHint(const std::string& hint);
};

}  // namespace astrocore
