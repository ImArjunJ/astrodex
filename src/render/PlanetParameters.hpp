#pragma once

#include <glm/glm.hpp>
#include <cstdint>

namespace astrocore {

// Explicit std140 alignment for OpenGL UBO compatibility
// All parameters are scientifically meaningful and drive the raymarching shader
struct alignas(16) PlanetParameters {
    // ═══════════════════════════════════════════════════════════════════════════
    // 1. GEOMORPHOLOGY & TECTONICS
    // ═══════════════════════════════════════════════════════════════════════════
    float seed = 0.0f;                    // Random seed for terrain generation
    float tectonicActivity = 0.5f;        // 0.0 (smooth) to 1.0 (mountainous ridges)
    float craterDensity = 0.0f;           // 0.0 (eroded) to 1.0 (heavy bombardment)
    float erosionModifier = 0.5f;         // Thermal/wind smoothing factor

    float continentScale = 1.0f;          // Large-scale landmass distribution
    float terrainRoughness = 0.5f;        // FBM persistence (detail level)
    float terrainLacunarity = 2.0f;       // FBM frequency multiplier
    float terrainExponent = 1.0f;         // Height curve exponentiation

    // ═══════════════════════════════════════════════════════════════════════════
    // 2. HYDROSPHERE & CRYOSPHERE
    // ═══════════════════════════════════════════════════════════════════════════
    alignas(16) glm::vec3 colorDeepLiquid = {0.01f, 0.03f, 0.15f};   // Deep ocean RGB
    float seaLevel = 0.4f;                // 0.0 to 1.0 radius threshold

    alignas(16) glm::vec3 colorShallowLiquid = {0.02f, 0.12f, 0.28f}; // Shallow water RGB
    float waveDistortion = 0.02f;         // Liquid normal map high-frequency noise

    float iceCapCoverage = 0.15f;         // Polar latitude threshold (0-1)
    float iceAlbedo = 0.9f;               // Ice reflectivity
    float liquidViscosity = 0.0f;         // 0=water, 1=lava/tar (affects specular)
    float _pad1 = 0.0f;

    // ═══════════════════════════════════════════════════════════════════════════
    // 3. SURFACE MATERIALS & BIOMES
    // ═══════════════════════════════════════════════════════════════════════════
    alignas(16) glm::vec3 colorLowland = {0.76f, 0.70f, 0.50f};      // Beach/desert
    float biomeTransition = 0.02f;        // Blend sharpness between biomes

    alignas(16) glm::vec3 colorMidland = {0.13f, 0.35f, 0.16f};      // Vegetation/forest
    float vegetationLine = 0.5f;          // Altitude where vegetation starts

    alignas(16) glm::vec3 colorHighland = {0.45f, 0.40f, 0.35f};     // Rock/mountain
    float treeLine = 0.7f;                // Altitude where trees stop

    alignas(16) glm::vec3 colorPeak = {0.95f, 0.95f, 0.97f};         // Snow/ice peaks
    float snowLine = 0.85f;               // Altitude where snow starts

    // ═══════════════════════════════════════════════════════════════════════════
    // 4. VOLUMETRIC ATMOSPHERE (Physical Scattering)
    // ═══════════════════════════════════════════════════════════════════════════
    alignas(16) glm::vec3 rayleighCoefficients = {5.8e-6f, 13.5e-6f, 33.1e-6f}; // λ⁻⁴ RGB
    float atmosphereRadius = 1.025f;      // Ratio to planet radius

    alignas(16) glm::vec3 mieCoefficients = {21e-6f, 21e-6f, 21e-6f}; // Aerosol scattering
    float atmosphereDensityFalloff = 8.0f; // Scale height in km (Earth ~8.5)

    float mieAsymmetry = 0.76f;           // Forward scattering phase (-1 to 1)
    float ozoneStrength = 0.0f;           // Ozone absorption (blue tint at horizon)
    float sunIntensity = 20.0f;           // Solar irradiance multiplier
    float ambientLight = 0.03f;           // Minimum light level

    alignas(16) glm::vec3 sunColor = {1.0f, 0.98f, 0.95f};           // Star color temperature
    float sunAngularRadius = 0.00467f;    // Sun disc size (radians)

    alignas(16) glm::vec3 sunDirection = {0.6f, 0.8f, 0.2f};         // Light direction
    float _pad2 = 0.0f;

    // ═══════════════════════════════════════════════════════════════════════════
    // 5. METEOROLOGY & FLUID DYNAMICS
    // ═══════════════════════════════════════════════════════════════════════════
    float isGasGiant = 0.0f;              // Boolean toggle (1.0 or 0.0)
    float coriolisStrength = 0.0f;        // Latitude-based noise warping
    float bandingFrequency = 20.0f;       // Gas giant stripe count
    float bandingContrast = 0.3f;         // Stripe color variation

    float cloudCoverage = 0.4f;           // 0.0 to 1.0
    float cloudAltitude = 0.015f;         // Height above surface
    float cloudDensity = 0.8f;            // Opacity multiplier
    float cloudSpeed = 0.5f;              // Animation speed

    alignas(16) glm::vec3 cloudColor = {1.0f, 1.0f, 1.0f};           // Cloud tint
    float vortexFrequency = 0.0f;         // Hurricane/storm count

    float vortexIntensity = 0.0f;         // Storm strength
    float jetStreamStrength = 0.0f;       // High-altitude wind patterns
    float _pad3[2] = {0.0f, 0.0f};

    // ═══════════════════════════════════════════════════════════════════════════
    // 6. RENDERING & ANIMATION
    // ═══════════════════════════════════════════════════════════════════════════
    float planetRadius = 2.0f;            // Visual size
    float rotationSpeed = 0.02f;          // Spin rate
    float rotationOffset = 0.0f;          // Initial rotation angle
    float renderQuality = 1.0f;           // Raymarching step multiplier

    // ═══════════════════════════════════════════════════════════════════════════
    // Helper methods for common presets
    // ═══════════════════════════════════════════════════════════════════════════
    static PlanetParameters Earth();
    static PlanetParameters Mars();
    static PlanetParameters Venus();
    static PlanetParameters Jupiter();
    static PlanetParameters Titan();
    static PlanetParameters Lava();
    static PlanetParameters Ice();
    static PlanetParameters Ocean();
};

// Verify std140 alignment
static_assert(sizeof(PlanetParameters) % 16 == 0, "PlanetParameters must be 16-byte aligned");

} // namespace astrocore
