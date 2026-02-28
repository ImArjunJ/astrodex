#include "render/PlanetParameters.hpp"

namespace astrocore {

PlanetParameters PlanetParameters::Earth() {
    PlanetParameters p;
    p.seed = 42.0f;
    p.tectonicActivity = 0.0f;  // No ridged mountains by default
    p.craterDensity = 0.0f;     // No craters
    p.erosionModifier = 0.0f;   // No domain warping
    p.continentScale = 0.0f;    // Off by default like original
    p.terrainRoughness = 0.5f;
    p.terrainLacunarity = 2.0f;
    p.terrainExponent = 5.0f;   // Sharp peaks like original

    p.colorDeepLiquid = {0.01f, 0.05f, 0.15f};
    p.colorShallowLiquid = {0.02f, 0.12f, 0.27f};
    p.seaLevel = 0.0f;          // Minimal water by default
    p.iceCapCoverage = 0.0f;

    // Colors matching original defaults
    p.colorLowland = {1.0f, 1.0f, 0.85f};   // Sand
    p.colorMidland = {0.02f, 0.1f, 0.06f};  // Trees
    p.colorHighland = {0.15f, 0.12f, 0.12f}; // Rock
    p.colorPeak = {0.8f, 0.9f, 0.9f};       // Ice

    // Biome thresholds - these map to the original shader values
    p.biomeTransition = 0.15f;  // Wider transitions for smoother blending
    p.vegetationLine = 0.28f;   // Maps to sandLevel ~0.028
    p.treeLine = 0.5f;          // Maps to rockLevel ~0.1
    p.snowLine = 0.6f;          // Maps to iceLevel ~0.15

    // Earth Rayleigh coefficients (nitrogen/oxygen atmosphere)
    p.rayleighCoefficients = {5.8e-6f, 13.5e-6f, 33.1e-6f};
    p.atmosphereRadius = 1.025f;
    p.atmosphereDensityFalloff = 3.3f;  // Gives density ~0.3 like original

    p.cloudCoverage = 0.5f;
    p.cloudAltitude = 0.15f;
    p.cloudDensity = 0.8f;
    p.cloudSpeed = 1.5f;
    p.cloudColor = {1.0f, 1.0f, 1.0f};

    p.sunIntensity = 3.0f;
    p.ambientLight = 0.01f;

    return p;
}

PlanetParameters PlanetParameters::Mars() {
    PlanetParameters p;
    p.seed = 123.0f;
    p.tectonicActivity = 0.3f;
    p.craterDensity = 0.6f;
    p.erosionModifier = 0.3f;
    p.continentScale = 0.8f;
    p.terrainRoughness = 0.55f;
    p.terrainExponent = 1.5f;

    // No liquid water
    p.seaLevel = -0.1f;
    p.iceCapCoverage = 0.25f;

    // Rusty reds and oranges
    p.colorLowland = {0.76f, 0.50f, 0.30f};
    p.colorMidland = {0.60f, 0.35f, 0.20f};
    p.colorHighland = {0.50f, 0.30f, 0.20f};
    p.colorPeak = {0.90f, 0.85f, 0.80f};

    // Mars CO2 atmosphere - orange/pink sky
    p.rayleighCoefficients = {19.9e-6f, 13.5e-6f, 5.8e-6f};
    p.atmosphereRadius = 1.01f;
    p.atmosphereDensityFalloff = 11.0f;
    p.mieCoefficients = {40e-6f, 40e-6f, 40e-6f};  // Dust
    p.mieAsymmetry = 0.65f;

    p.cloudCoverage = 0.05f;
    p.sunIntensity = 15.0f;

    return p;
}

PlanetParameters PlanetParameters::Venus() {
    PlanetParameters p;
    p.seed = 456.0f;
    p.tectonicActivity = 0.7f;
    p.craterDensity = 0.15f;
    p.erosionModifier = 0.2f;

    p.seaLevel = -0.1f;
    p.iceCapCoverage = 0.0f;

    // Volcanic yellows and browns
    p.colorLowland = {0.85f, 0.75f, 0.45f};
    p.colorMidland = {0.70f, 0.55f, 0.35f};
    p.colorHighland = {0.55f, 0.45f, 0.30f};
    p.colorPeak = {0.45f, 0.35f, 0.25f};

    // Dense CO2 atmosphere - yellowish
    p.rayleighCoefficients = {10.0e-6f, 8.0e-6f, 3.0e-6f};
    p.atmosphereRadius = 1.08f;
    p.atmosphereDensityFalloff = 4.0f;  // Very thick
    p.mieCoefficients = {100e-6f, 100e-6f, 80e-6f};  // Sulfuric acid haze
    p.mieAsymmetry = 0.8f;

    p.cloudCoverage = 1.0f;
    p.cloudColor = {0.95f, 0.90f, 0.70f};
    p.cloudDensity = 0.95f;

    return p;
}

PlanetParameters PlanetParameters::Jupiter() {
    PlanetParameters p;
    p.isGasGiant = 1.0f;
    p.seed = 789.0f;

    p.coriolisStrength = 0.9f;
    p.bandingFrequency = 25.0f;
    p.bandingContrast = 0.4f;

    // Gas giant colors (bands)
    p.colorLowland = {0.90f, 0.85f, 0.70f};   // Light bands
    p.colorMidland = {0.70f, 0.55f, 0.40f};   // Medium bands
    p.colorHighland = {0.50f, 0.35f, 0.25f};  // Dark bands
    p.colorPeak = {0.85f, 0.60f, 0.45f};      // Storm colors

    // Hydrogen/helium atmosphere
    p.rayleighCoefficients = {3.0e-6f, 7.0e-6f, 15.0e-6f};
    p.atmosphereRadius = 1.02f;
    p.atmosphereDensityFalloff = 20.0f;

    p.cloudCoverage = 1.0f;
    p.cloudDensity = 0.6f;
    p.vortexFrequency = 3.0f;
    p.vortexIntensity = 0.8f;
    p.jetStreamStrength = 0.7f;

    p.planetRadius = 3.0f;

    return p;
}

PlanetParameters PlanetParameters::Titan() {
    PlanetParameters p;
    p.seed = 101.0f;
    p.tectonicActivity = 0.2f;
    p.craterDensity = 0.1f;
    p.erosionModifier = 0.7f;

    // Hydrocarbon lakes
    p.colorDeepLiquid = {0.15f, 0.10f, 0.05f};
    p.colorShallowLiquid = {0.25f, 0.18f, 0.10f};
    p.seaLevel = 0.3f;
    p.liquidViscosity = 0.3f;

    p.iceCapCoverage = 0.0f;

    // Orange/brown surface
    p.colorLowland = {0.55f, 0.40f, 0.25f};
    p.colorMidland = {0.45f, 0.32f, 0.20f};
    p.colorHighland = {0.60f, 0.50f, 0.35f};  // Water ice
    p.colorPeak = {0.70f, 0.65f, 0.55f};

    // Thick nitrogen atmosphere with orange haze
    p.rayleighCoefficients = {8.0e-6f, 6.0e-6f, 3.0e-6f};
    p.atmosphereRadius = 1.06f;
    p.atmosphereDensityFalloff = 6.0f;
    p.mieCoefficients = {80e-6f, 60e-6f, 30e-6f};  // Tholin haze
    p.mieAsymmetry = 0.85f;

    p.cloudCoverage = 0.3f;
    p.cloudColor = {0.8f, 0.6f, 0.4f};

    return p;
}

PlanetParameters PlanetParameters::Lava() {
    PlanetParameters p;
    p.seed = 666.0f;
    p.tectonicActivity = 0.95f;
    p.craterDensity = 0.2f;
    p.erosionModifier = 0.1f;
    p.terrainExponent = 2.0f;

    // Lava "ocean"
    p.colorDeepLiquid = {0.8f, 0.2f, 0.0f};
    p.colorShallowLiquid = {1.0f, 0.5f, 0.1f};
    p.seaLevel = 0.35f;
    p.liquidViscosity = 0.9f;
    p.waveDistortion = 0.05f;

    p.iceCapCoverage = 0.0f;

    // Dark volcanic rock
    p.colorLowland = {0.25f, 0.15f, 0.10f};
    p.colorMidland = {0.20f, 0.10f, 0.05f};
    p.colorHighland = {0.15f, 0.08f, 0.05f};
    p.colorPeak = {0.10f, 0.05f, 0.02f};

    // Volcanic atmosphere
    p.rayleighCoefficients = {15.0e-6f, 8.0e-6f, 3.0e-6f};
    p.atmosphereRadius = 1.02f;
    p.atmosphereDensityFalloff = 5.0f;
    p.mieCoefficients = {50e-6f, 30e-6f, 10e-6f};

    p.cloudCoverage = 0.2f;
    p.cloudColor = {0.4f, 0.2f, 0.1f};

    p.sunIntensity = 25.0f;
    p.ambientLight = 0.08f;

    return p;
}

PlanetParameters PlanetParameters::Ice() {
    PlanetParameters p;
    p.seed = 999.0f;
    p.tectonicActivity = 0.2f;
    p.craterDensity = 0.3f;
    p.erosionModifier = 0.8f;
    p.terrainRoughness = 0.35f;

    // Frozen ocean
    p.colorDeepLiquid = {0.05f, 0.08f, 0.15f};
    p.colorShallowLiquid = {0.10f, 0.15f, 0.25f};
    p.seaLevel = 0.2f;

    p.iceCapCoverage = 0.8f;
    p.iceAlbedo = 0.95f;

    // Icy blues and whites
    p.colorLowland = {0.75f, 0.85f, 0.90f};
    p.colorMidland = {0.60f, 0.70f, 0.80f};
    p.colorHighland = {0.50f, 0.55f, 0.65f};
    p.colorPeak = {0.95f, 0.97f, 1.0f};

    // Cold thin atmosphere
    p.rayleighCoefficients = {4.0e-6f, 10.0e-6f, 25.0e-6f};
    p.atmosphereRadius = 1.015f;
    p.atmosphereDensityFalloff = 10.0f;

    p.cloudCoverage = 0.6f;
    p.cloudColor = {0.9f, 0.95f, 1.0f};

    p.sunIntensity = 12.0f;

    return p;
}

PlanetParameters PlanetParameters::Ocean() {
    PlanetParameters p;
    p.seed = 777.0f;
    p.tectonicActivity = 0.3f;
    p.craterDensity = 0.0f;
    p.erosionModifier = 0.9f;
    p.continentScale = 2.0f;

    // Deep blue ocean
    p.colorDeepLiquid = {0.005f, 0.02f, 0.12f};
    p.colorShallowLiquid = {0.02f, 0.10f, 0.25f};
    p.seaLevel = 0.7f;  // Mostly water
    p.waveDistortion = 0.03f;

    p.iceCapCoverage = 0.1f;

    // Small islands
    p.colorLowland = {0.85f, 0.80f, 0.65f};
    p.colorMidland = {0.20f, 0.45f, 0.20f};
    p.colorHighland = {0.40f, 0.35f, 0.30f};
    p.colorPeak = {0.90f, 0.92f, 0.95f};

    // Earth-like atmosphere
    p.rayleighCoefficients = {5.8e-6f, 13.5e-6f, 33.1e-6f};
    p.atmosphereRadius = 1.03f;
    p.atmosphereDensityFalloff = 8.0f;

    p.cloudCoverage = 0.65f;

    return p;
}

} // namespace astrocore
