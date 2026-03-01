#pragma once

#include "render/Renderer.hpp"
#include "data/ExoplanetData.hpp"

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

private:
    // Helper functions for different planet types
    static PlanetParams createRockyParams(const ExoplanetData& exo);
    static PlanetParams createSuperEarthParams(const ExoplanetData& exo);
    static PlanetParams createMiniNeptuneParams(const ExoplanetData& exo);
    static PlanetParams createGasGiantParams(const ExoplanetData& exo);
    static PlanetParams createIceGiantParams(const ExoplanetData& exo);
    static PlanetParams createLavaWorldParams(const ExoplanetData& exo);
    static PlanetParams createOceanWorldParams(const ExoplanetData& exo);
    static PlanetParams createDesertWorldParams(const ExoplanetData& exo);

    // Apply temperature-based color adjustments
    static void applyTemperatureColors(PlanetParams& params, double tempK);

    // Apply AI-inferred parameters (ocean coverage, clouds, atmosphere, etc.)
    static void applyAIInferredParams(PlanetParams& params, const ExoplanetData& exo);

    // Parse color hint string to RGB
    static glm::vec3 parseColorHint(const std::string& hint);
};

}  // namespace astrocore
