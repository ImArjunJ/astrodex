#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace astrocore {

// Comprehensive parametric planet generation system
// Every parameter can be AI-inferred from observational data

struct NoiseParams {
    float frequency = 1.0f;
    float amplitude = 1.0f;
    int octaves = 6;
    float persistence = 0.5f;      // Amplitude multiplier per octave
    float lacunarity = 2.0f;       // Frequency multiplier per octave
    glm::vec3 offset = {0, 0, 0};  // Seed offset

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(NoiseParams, frequency, amplitude, octaves, persistence, lacunarity)
};

struct TectonicParams {
    float plateScale = 3.0f;           // Size of tectonic plates
    float ridgeHeight = 0.3f;          // Mountain ridge height at boundaries
    float ridgeWidth = 0.1f;           // Width of mountain ranges
    float continentSize = 0.4f;        // Average continent size (0-1)
    float continentCount = 5.0f;       // Approximate number of continents
    float coastlineComplexity = 0.5f;  // Fractal coastline detail
    float islandDensity = 0.2f;        // Volcanic island chains

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(TectonicParams, plateScale, ridgeHeight, ridgeWidth,
                                    continentSize, continentCount, coastlineComplexity, islandDensity)
};

struct TerrainParams {
    NoiseParams baseNoise;
    NoiseParams detailNoise;
    NoiseParams mountainNoise;
    TectonicParams tectonics;

    float seaLevel = 0.4f;             // Ocean coverage (0-1)
    float heightScale = 0.15f;         // Max terrain height relative to radius
    float erosionStrength = 0.3f;      // Hydraulic erosion simulation
    float craterDensity = 0.0f;        // Impact craters (0 = none, 1 = heavily cratered)
    float craterSize = 0.1f;           // Average crater size
    float volcanicActivity = 0.1f;     // Volcanic features
    float canyonDepth = 0.0f;          // Rift valleys/canyons

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(TerrainParams, seaLevel, heightScale, erosionStrength,
                                    craterDensity, craterSize, volcanicActivity, canyonDepth)
};

struct BiomeParams {
    // Temperature zones
    float equatorTemp = 1.0f;          // Relative temperature at equator
    float polarTemp = 0.0f;            // Relative temperature at poles
    float tempVariation = 0.2f;        // Random temperature variation

    // Moisture
    float globalMoisture = 0.5f;       // Overall moisture level
    float rainShadowStrength = 0.3f;   // Mountains blocking moisture

    // Vegetation
    float vegetationDensity = 0.6f;    // Global vegetation coverage
    float treelineAltitude = 0.6f;     // Where trees stop growing
    float desertThreshold = 0.3f;      // Moisture level below which = desert

    // Ice
    float polarIceExtent = 0.15f;      // How far ice caps extend from poles
    float glacierAltitude = 0.8f;      // Altitude for permanent snow
    float seaIceExtent = 0.1f;         // Frozen ocean extent

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(BiomeParams, equatorTemp, polarTemp, tempVariation,
                                    globalMoisture, rainShadowStrength, vegetationDensity,
                                    treelineAltitude, desertThreshold, polarIceExtent,
                                    glacierAltitude, seaIceExtent)
};

struct OceanParams {
    glm::vec3 deepColor = {0.01f, 0.03f, 0.15f};
    glm::vec3 shallowColor = {0.02f, 0.12f, 0.3f};
    glm::vec3 coastColor = {0.05f, 0.2f, 0.35f};

    float depthColorScale = 0.5f;      // How quickly color changes with depth
    float transparency = 0.3f;         // Water clarity
    float specularStrength = 0.8f;     // Sun reflection intensity
    float specularPower = 64.0f;       // Sharpness of sun reflection
    float waveScale = 0.01f;           // Surface wave amplitude
    float waveFrequency = 20.0f;       // Wave pattern frequency
    float foamThreshold = 0.8f;        // Where foam appears (wave height)

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(OceanParams, depthColorScale, transparency,
                                    specularStrength, specularPower, waveScale, waveFrequency)
};

struct SurfaceColors {
    glm::vec3 beach = {0.76f, 0.70f, 0.50f};
    glm::vec3 lowlandGrass = {0.15f, 0.4f, 0.1f};
    glm::vec3 highland = {0.4f, 0.35f, 0.2f};
    glm::vec3 mountain = {0.45f, 0.43f, 0.4f};
    glm::vec3 snow = {0.95f, 0.97f, 1.0f};
    glm::vec3 desert = {0.8f, 0.65f, 0.4f};
    glm::vec3 tundra = {0.5f, 0.55f, 0.45f};
    glm::vec3 jungle = {0.05f, 0.3f, 0.05f};
    glm::vec3 rock = {0.35f, 0.33f, 0.3f};
    glm::vec3 ice = {0.85f, 0.9f, 0.95f};
    glm::vec3 lava = {1.0f, 0.3f, 0.0f};
    glm::vec3 volcanic = {0.15f, 0.1f, 0.08f};
};

struct CloudLayer {
    float altitude = 0.02f;            // Height above surface
    float thickness = 0.01f;           // Layer thickness
    float coverage = 0.4f;             // 0-1 coverage amount
    float density = 0.7f;              // Opacity when present
    NoiseParams noise;
    glm::vec3 color = {1.0f, 1.0f, 1.0f};
    float shadowStrength = 0.3f;       // Shadow cast on surface

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(CloudLayer, altitude, thickness, coverage, density, shadowStrength)
};

struct AtmosphereParams {
    // Scattering
    glm::vec3 rayleighCoeff = {5.5e-6f, 13.0e-6f, 22.4e-6f};  // Wavelength-dependent
    float rayleighScale = 1.0f;
    float mieCoeff = 2e-5f;
    float mieAnisotropy = 0.76f;       // Mie scattering direction (-1 to 1)

    // Structure
    float height = 0.1f;               // Atmosphere height relative to radius
    float density = 1.0f;              // Overall density
    float scaleHeight = 0.25f;         // Density falloff rate

    // Visual
    glm::vec3 ambientColor = {0.1f, 0.15f, 0.2f};
    float sunsetIntensity = 1.0f;      // Red/orange at terminator
    float auroraStrength = 0.0f;       // Polar lights
    glm::vec3 auroraColor = {0.2f, 1.0f, 0.5f};

    // Clouds (multiple layers)
    std::vector<CloudLayer> cloudLayers;

    // Haze/dust
    float hazeStrength = 0.0f;
    glm::vec3 hazeColor = {0.8f, 0.7f, 0.6f};

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(AtmosphereParams, rayleighScale, mieCoeff, mieAnisotropy,
                                    height, density, scaleHeight, sunsetIntensity,
                                    auroraStrength, hazeStrength)
};

struct GasGiantParams {
    // Band structure
    float bandCount = 12.0f;           // Number of latitude bands
    float bandContrast = 0.4f;         // Color difference between bands
    float bandTurbulence = 0.3f;       // Waviness of band edges
    float jetStreamStrength = 0.5f;    // East-west flow patterns

    // Colors
    glm::vec3 bandColor1 = {0.8f, 0.7f, 0.6f};
    glm::vec3 bandColor2 = {0.6f, 0.5f, 0.4f};
    glm::vec3 stormColor = {0.9f, 0.85f, 0.8f};

    // Storms
    float stormFrequency = 0.1f;       // How common storms are
    float greatSpotSize = 0.0f;        // 0 = no great spot, 0.2 = Jupiter-like
    glm::vec2 greatSpotPosition = {0.0f, 0.3f};  // Longitude, latitude
    glm::vec3 greatSpotColor = {0.9f, 0.5f, 0.4f};

    // Atmosphere effects
    float cloudDepth = 0.5f;           // Visible cloud layer depth
    float lightningFrequency = 0.0f;   // Visible lightning flashes

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(GasGiantParams, bandCount, bandContrast, bandTurbulence,
                                    jetStreamStrength, stormFrequency, greatSpotSize, cloudDepth)
};

struct StarParams {
    float temperature = 5778.0f;       // Kelvin (determines color via blackbody)
    float limbDarkening = 0.6f;        // Edge darkening intensity

    // Surface features
    float granulationScale = 0.01f;    // Convection cell size
    float granulationContrast = 0.1f;  // Brightness variation
    float sunspotCoverage = 0.01f;     // Fraction covered by sunspots
    float sunspotSize = 0.02f;         // Average sunspot size

    // Activity
    float faculaeBrightness = 0.05f;   // Bright regions
    float coronaIntensity = 0.0f;      // Visible corona (for eclipses)
    float flareIntensity = 0.0f;       // Solar flare brightness

    // Emission
    float luminosity = 1.0f;           // Relative to Sun
    float glowRadius = 1.5f;           // Extent of glow effect

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(StarParams, temperature, limbDarkening, granulationScale,
                                    granulationContrast, sunspotCoverage, sunspotSize,
                                    faculaeBrightness, luminosity, glowRadius)
};

struct RingParams {
    bool hasRings = false;
    float innerRadius = 1.2f;          // Relative to planet radius
    float outerRadius = 2.5f;
    float thickness = 0.01f;
    float density = 0.5f;              // Opacity
    float particleSize = 0.001f;       // For scattering calculations
    glm::vec3 color = {0.8f, 0.75f, 0.7f};
    float divisions = 3.0f;            // Gap count (like Saturn's Cassini division)

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(RingParams, hasRings, innerRadius, outerRadius,
                                    thickness, density, particleSize, divisions)
};

struct LightingParams {
    glm::vec3 sunDirection = {0.6f, 0.4f, 0.7f};
    glm::vec3 sunColor = {1.0f, 0.98f, 0.95f};
    float sunIntensity = 1.5f;
    float ambientIntensity = 0.1f;
    float shadowSoftness = 0.1f;

    // For multiple star systems
    bool hasSecondaryStar = false;
    glm::vec3 secondarySunDir = {-0.5f, 0.2f, -0.6f};
    glm::vec3 secondarySunColor = {1.0f, 0.6f, 0.3f};
    float secondarySunIntensity = 0.5f;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(LightingParams, sunIntensity, ambientIntensity, shadowSoftness)
};

// Body type enum
enum class CelestialBodyType {
    Terrestrial,      // Rocky planet with potential atmosphere
    GasGiant,         // Jupiter/Saturn type
    IceGiant,         // Neptune/Uranus type
    Star,             // Main sequence star
    BrownDwarf,       // Failed star
    Moon,             // Typically airless, cratered
    Asteroid,         // Small, irregular body
    Comet             // With tail effects
};

// Master parameter struct containing EVERYTHING
struct CelestialBodyParams {
    // Identification
    std::string name = "Unknown";
    CelestialBodyType bodyType = CelestialBodyType::Terrestrial;

    // Physical properties
    float radius = 1.0f;               // Relative to Earth
    float mass = 1.0f;                 // Relative to Earth
    float rotationPeriod = 24.0f;      // Hours
    float axialTilt = 23.5f;           // Degrees
    float surfaceGravity = 1.0f;       // Relative to Earth

    // Thermal
    float surfaceTemp = 288.0f;        // Kelvin (average)
    float tempVariation = 50.0f;       // Day/night and latitude variation

    // Subsystems (all the detailed params)
    TerrainParams terrain;
    BiomeParams biome;
    OceanParams ocean;
    SurfaceColors colors;
    AtmosphereParams atmosphere;
    GasGiantParams gasGiant;
    StarParams star;
    RingParams rings;
    LightingParams lighting;

    // Rendering
    uint32_t seed = 0;                 // For reproducible procedural generation
    float renderQuality = 1.0f;        // LOD multiplier

    // Serialize to/from JSON for AI inference
    nlohmann::json toJson() const;
    static CelestialBodyParams fromJson(const nlohmann::json& j);

    // Presets for known planets
    static CelestialBodyParams Earth();
    static CelestialBodyParams Mars();
    static CelestialBodyParams Venus();
    static CelestialBodyParams Jupiter();
    static CelestialBodyParams Saturn();
    static CelestialBodyParams Neptune();
    static CelestialBodyParams Moon();
    static CelestialBodyParams Titan();
    static CelestialBodyParams Europa();

    // Generate from basic observational data
    static CelestialBodyParams fromObservations(
        float radiusEarth,
        float massEarth,
        float tempK,
        float orbitalPeriodDays,
        const std::string& starType
    );
};

}  // namespace astrocore
