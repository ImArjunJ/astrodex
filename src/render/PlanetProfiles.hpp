#pragma once

// This file is deprecated - use PlanetTypes.hpp instead
// Kept for backward compatibility
#include "render/PlanetTypes.hpp"
#include "render/Renderer.hpp"

namespace astrocore {

// Apply profile to render params
inline void applyProfile(PlanetRenderParams& params, const PlanetProfile& profile) {
    params.oceanColor = profile.oceanColor;
    params.lowlandColor = profile.lowlandColor;
    params.highlandColor = profile.highlandColor;
    params.mountainColor = profile.mountainColor;
    params.snowColor = profile.snowColor;
    params.rayleighCoeff = profile.atmosphereColor;
    params.oceanLevel = profile.oceanLevel;
    params.atmosphereHeight = profile.atmosphereHeight;
    params.atmosphereDensity = profile.atmosphereDensity;
    params.terrainAmplitude = profile.terrainAmplitude;
}

}  // namespace astrocore
