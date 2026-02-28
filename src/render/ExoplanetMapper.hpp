#pragma once

#include "render/IRenderer.hpp"
#include "data/ExoplanetData.hpp"
#include <nlohmann/json.hpp>
#include <string>

namespace astrocore {

enum class PlanetCategory {
    LavaWorld,
    HotRocky,
    HotJupiter,
    GasGiant,
    IceGiant,
    IceWorld,
    OceanWorld,
    Terrestrial,
    RockyDesert,
    Unknown
};

// Translates ExoplanetData → PlanetParams using physics-based derivations.
// Operates in two passes:
//   1. toPlanetParams()  — deterministic, from measured/calculated fields
//   2. applyAIRenderOverrides() — merges numeric JSON from InferenceEngine
class ExoplanetMapper {
public:
    // Classify the planet into a broad visual category.
    static PlanetCategory classify(const ExoplanetData& data);

    // Convert physical data to renderer parameters (pure physics pass).
    static PlanetParams toPlanetParams(const ExoplanetData& data);

    // Merge AI-inferred numeric overrides into an existing PlanetParams.
    // aiJson keys match PlanetParams field names, values are floats or [r,g,b] arrays.
    static void applyAIRenderOverrides(PlanetParams& params, const nlohmann::json& aiJson);

    // Convenience: physics pass followed by optional AI overrides.
    static PlanetParams toRenderParams(const ExoplanetData& data,
                                       const nlohmann::json& aiJson = {});

    static std::string categoryName(PlanetCategory cat);
};

}  // namespace astrocore
