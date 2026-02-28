#pragma once

#include "render/IRenderer.hpp"
#include "data/ExoplanetData.hpp"
#include "data/SolarSystemDatabase.hpp"
#include <nlohmann/json.hpp>
#include <set>
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
//
// Philosophy: fill empty slots, never clobber measured data.
//   Pass 1 (toPlanetParams)  — deterministic derivations from measured fields.
//                              Tracks which PlanetParams fields were set from
//                              real data in physicsFields (out-param).
//   Pass 2 (applyAIRenderOverrides) — merges AI JSON, but skips every field
//                              already covered by pass 1 (skipFields).
class ExoplanetMapper {
public:
    // Classify the planet into a broad visual category.
    static PlanetCategory classify(const ExoplanetData& data);

    // Convert physical data to renderer parameters (pure physics pass).
    //
    // physicsFields (optional out-param): populated with the names of every
    //   PlanetParams field that was set from a *measured* ExoplanetData value.
    //   Pass these to applyAIRenderOverrides as skipFields so AI doesn't
    //   clobber real data.
    //
    // analogBase (optional): when a solar-system analog was found, pass its
    //   visual params here to use as the starting point instead of the generic
    //   category defaults.
    static PlanetParams toPlanetParams(const ExoplanetData& data,
                                       std::set<std::string>* physicsFields = nullptr,
                                       const PlanetParams*   analogBase     = nullptr);

    // Merge AI-inferred numeric overrides, skipping fields already derived
    // from measured data (skipFields).
    static void applyAIRenderOverrides(PlanetParams&                   params,
                                       const nlohmann::json&           aiJson,
                                       const std::set<std::string>&    skipFields = {});

    // Full pipeline convenience method:
    //   1. Finds closest solar-system analog (if mass/radius/temp available)
    //   2. Calls toPlanetParams with that analog as base
    //   3. Calls applyAIRenderOverrides with physics-derived fields protected
    //
    // analogMatch (optional out-param): filled if an analog was used.
    static PlanetParams toRenderParams(const ExoplanetData&  data,
                                       const nlohmann::json& aiJson       = {},
                                       AnalogMatch*          analogMatch  = nullptr);

    static std::string categoryName(PlanetCategory cat);
};

}  // namespace astrocore
