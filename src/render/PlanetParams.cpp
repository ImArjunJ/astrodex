#include "render/PlanetParams.hpp"
#include "data/ExoplanetData.hpp"
#include <functional>
#include <algorithm>

namespace astrocore {

nlohmann::json CelestialBodyParams::toJson() const {
    nlohmann::json j;
    j["name"] = name;
    j["bodyType"] = static_cast<int>(bodyType);
    j["radius"] = radius;
    j["mass"] = mass;
    j["rotationPeriod"] = rotationPeriod;
    j["axialTilt"] = axialTilt;
    j["surfaceGravity"] = surfaceGravity;
    j["surfaceTemp"] = surfaceTemp;
    j["tempVariation"] = tempVariation;
    j["seed"] = seed;

    // Terrain
    j["terrain"]["seaLevel"] = terrain.seaLevel;
    j["terrain"]["heightScale"] = terrain.heightScale;
    j["terrain"]["erosionStrength"] = terrain.erosionStrength;
    j["terrain"]["craterDensity"] = terrain.craterDensity;
    j["terrain"]["volcanicActivity"] = terrain.volcanicActivity;
    j["terrain"]["tectonics"]["plateScale"] = terrain.tectonics.plateScale;
    j["terrain"]["tectonics"]["continentSize"] = terrain.tectonics.continentSize;
    j["terrain"]["tectonics"]["continentCount"] = terrain.tectonics.continentCount;

    // Biome
    j["biome"]["globalMoisture"] = biome.globalMoisture;
    j["biome"]["vegetationDensity"] = biome.vegetationDensity;
    j["biome"]["polarIceExtent"] = biome.polarIceExtent;
    j["biome"]["desertThreshold"] = biome.desertThreshold;

    // Ocean
    j["ocean"]["deepColor"] = {ocean.deepColor.r, ocean.deepColor.g, ocean.deepColor.b};
    j["ocean"]["shallowColor"] = {ocean.shallowColor.r, ocean.shallowColor.g, ocean.shallowColor.b};
    j["ocean"]["specularStrength"] = ocean.specularStrength;

    // Atmosphere
    j["atmosphere"]["height"] = atmosphere.height;
    j["atmosphere"]["density"] = atmosphere.density;
    j["atmosphere"]["rayleighScale"] = atmosphere.rayleighScale;
    j["atmosphere"]["hazeStrength"] = atmosphere.hazeStrength;

    // Gas giant (if applicable)
    if (bodyType == CelestialBodyType::GasGiant || bodyType == CelestialBodyType::IceGiant) {
        j["gasGiant"]["bandCount"] = gasGiant.bandCount;
        j["gasGiant"]["bandContrast"] = gasGiant.bandContrast;
        j["gasGiant"]["stormFrequency"] = gasGiant.stormFrequency;
        j["gasGiant"]["greatSpotSize"] = gasGiant.greatSpotSize;
    }

    // Star (if applicable)
    if (bodyType == CelestialBodyType::Star) {
        j["star"]["temperature"] = star.temperature;
        j["star"]["limbDarkening"] = star.limbDarkening;
        j["star"]["sunspotCoverage"] = star.sunspotCoverage;
    }

    // Rings
    j["rings"]["hasRings"] = rings.hasRings;
    if (rings.hasRings) {
        j["rings"]["innerRadius"] = rings.innerRadius;
        j["rings"]["outerRadius"] = rings.outerRadius;
        j["rings"]["density"] = rings.density;
    }

    return j;
}

CelestialBodyParams CelestialBodyParams::fromJson(const nlohmann::json& j) {
    CelestialBodyParams p;

    if (j.contains("name")) p.name = j["name"].get<std::string>();
    if (j.contains("bodyType")) p.bodyType = static_cast<CelestialBodyType>(j["bodyType"].get<int>());
    if (j.contains("radius")) p.radius = j["radius"].get<float>();
    if (j.contains("mass")) p.mass = j["mass"].get<float>();
    if (j.contains("surfaceTemp")) p.surfaceTemp = j["surfaceTemp"].get<float>();
    if (j.contains("seed")) p.seed = j["seed"].get<uint32_t>();

    // Terrain
    if (j.contains("terrain")) {
        auto& t = j["terrain"];
        if (t.contains("seaLevel")) p.terrain.seaLevel = t["seaLevel"].get<float>();
        if (t.contains("heightScale")) p.terrain.heightScale = t["heightScale"].get<float>();
        if (t.contains("craterDensity")) p.terrain.craterDensity = t["craterDensity"].get<float>();
        if (t.contains("volcanicActivity")) p.terrain.volcanicActivity = t["volcanicActivity"].get<float>();
    }

    // Biome
    if (j.contains("biome")) {
        auto& b = j["biome"];
        if (b.contains("globalMoisture")) p.biome.globalMoisture = b["globalMoisture"].get<float>();
        if (b.contains("vegetationDensity")) p.biome.vegetationDensity = b["vegetationDensity"].get<float>();
        if (b.contains("polarIceExtent")) p.biome.polarIceExtent = b["polarIceExtent"].get<float>();
    }

    // Atmosphere
    if (j.contains("atmosphere")) {
        auto& a = j["atmosphere"];
        if (a.contains("height")) p.atmosphere.height = a["height"].get<float>();
        if (a.contains("density")) p.atmosphere.density = a["density"].get<float>();
    }

    // Gas giant
    if (j.contains("gasGiant")) {
        auto& g = j["gasGiant"];
        if (g.contains("bandCount")) p.gasGiant.bandCount = g["bandCount"].get<float>();
        if (g.contains("bandContrast")) p.gasGiant.bandContrast = g["bandContrast"].get<float>();
        if (g.contains("greatSpotSize")) p.gasGiant.greatSpotSize = g["greatSpotSize"].get<float>();
    }

    return p;
}

// ============================================================================
// EARTH - The reference terrestrial planet
// ============================================================================
CelestialBodyParams CelestialBodyParams::Earth() {
    CelestialBodyParams p;
    p.name = "Earth";
    p.bodyType = CelestialBodyType::Terrestrial;
    p.radius = 1.0f;
    p.mass = 1.0f;
    p.rotationPeriod = 24.0f;
    p.axialTilt = 23.44f;
    p.surfaceGravity = 1.0f;
    p.surfaceTemp = 288.0f;
    p.tempVariation = 50.0f;

    // Terrain - Earth has significant oceans and moderate terrain
    p.terrain.seaLevel = 0.55f;  // ~70% ocean coverage
    p.terrain.heightScale = 0.1f;
    p.terrain.erosionStrength = 0.5f;
    p.terrain.craterDensity = 0.0f;  // Eroded away
    p.terrain.volcanicActivity = 0.1f;
    p.terrain.tectonics.plateScale = 3.0f;
    p.terrain.tectonics.ridgeHeight = 0.4f;
    p.terrain.tectonics.continentSize = 0.35f;
    p.terrain.tectonics.continentCount = 6.0f;
    p.terrain.tectonics.coastlineComplexity = 0.6f;

    // Biome - Diverse with vegetation
    p.biome.equatorTemp = 1.0f;
    p.biome.polarTemp = 0.0f;
    p.biome.globalMoisture = 0.6f;
    p.biome.vegetationDensity = 0.7f;
    p.biome.polarIceExtent = 0.1f;
    p.biome.glacierAltitude = 0.7f;
    p.biome.desertThreshold = 0.25f;

    // Ocean - Deep blue with good reflectivity
    p.ocean.deepColor = {0.01f, 0.03f, 0.12f};
    p.ocean.shallowColor = {0.02f, 0.1f, 0.25f};
    p.ocean.coastColor = {0.05f, 0.15f, 0.3f};
    p.ocean.specularStrength = 0.8f;
    p.ocean.specularPower = 64.0f;

    // Surface colors - Earth-like biomes
    p.colors.beach = {0.76f, 0.70f, 0.50f};
    p.colors.lowlandGrass = {0.1f, 0.35f, 0.08f};
    p.colors.highland = {0.35f, 0.3f, 0.2f};
    p.colors.mountain = {0.4f, 0.38f, 0.35f};
    p.colors.snow = {0.95f, 0.97f, 1.0f};
    p.colors.desert = {0.85f, 0.7f, 0.45f};
    p.colors.jungle = {0.03f, 0.25f, 0.03f};

    // Atmosphere - Blue sky, moderate clouds
    p.atmosphere.rayleighCoeff = {5.5e-6f, 13.0e-6f, 22.4e-6f};
    p.atmosphere.rayleighScale = 1.0f;
    p.atmosphere.mieCoeff = 2e-5f;
    p.atmosphere.height = 0.08f;
    p.atmosphere.density = 1.0f;
    p.atmosphere.scaleHeight = 0.12f;

    // Single cloud layer
    CloudLayer cumulus;
    cumulus.altitude = 0.015f;
    cumulus.coverage = 0.45f;
    cumulus.density = 0.75f;
    cumulus.noise.frequency = 4.0f;
    cumulus.noise.octaves = 5;
    p.atmosphere.cloudLayers.push_back(cumulus);

    p.lighting.sunIntensity = 1.5f;
    p.lighting.ambientIntensity = 0.12f;

    return p;
}

// ============================================================================
// MARS - Cold desert world
// ============================================================================
CelestialBodyParams CelestialBodyParams::Mars() {
    CelestialBodyParams p;
    p.name = "Mars";
    p.bodyType = CelestialBodyType::Terrestrial;
    p.radius = 0.532f;
    p.mass = 0.107f;
    p.rotationPeriod = 24.6f;
    p.axialTilt = 25.19f;
    p.surfaceGravity = 0.38f;
    p.surfaceTemp = 210.0f;
    p.tempVariation = 80.0f;

    // Terrain - No oceans, ancient craters, large volcanoes
    p.terrain.seaLevel = 0.0f;  // No liquid water
    p.terrain.heightScale = 0.2f;  // Olympus Mons!
    p.terrain.erosionStrength = 0.1f;  // Ancient water erosion
    p.terrain.craterDensity = 0.3f;
    p.terrain.volcanicActivity = 0.0f;  // Extinct
    p.terrain.canyonDepth = 0.3f;  // Valles Marineris
    p.terrain.tectonics.plateScale = 1.0f;  // No active tectonics

    // Biome - Frozen desert
    p.biome.globalMoisture = 0.0f;
    p.biome.vegetationDensity = 0.0f;
    p.biome.polarIceExtent = 0.2f;  // CO2 ice caps
    p.biome.glacierAltitude = 0.0f;

    // No ocean, but keeping for potential subsurface water hints
    p.ocean.deepColor = {0.0f, 0.0f, 0.0f};

    // Surface colors - Rusty red/orange
    p.colors.beach = {0.7f, 0.4f, 0.25f};  // Light rust
    p.colors.lowlandGrass = {0.65f, 0.35f, 0.2f};  // More rust
    p.colors.highland = {0.55f, 0.3f, 0.18f};
    p.colors.mountain = {0.45f, 0.28f, 0.18f};
    p.colors.snow = {0.9f, 0.88f, 0.85f};  // Slightly pink CO2 ice
    p.colors.desert = {0.75f, 0.45f, 0.25f};
    p.colors.rock = {0.4f, 0.25f, 0.15f};

    // Atmosphere - Thin, dusty, orange-pink sky
    p.atmosphere.rayleighCoeff = {8e-6f, 4e-6f, 2e-6f};  // Orange-tinted
    p.atmosphere.rayleighScale = 0.01f;  // Very thin
    p.atmosphere.height = 0.05f;
    p.atmosphere.density = 0.006f;  // 0.6% of Earth
    p.atmosphere.hazeStrength = 0.4f;
    p.atmosphere.hazeColor = {0.8f, 0.6f, 0.4f};  // Dust

    // Thin dust clouds
    CloudLayer dust;
    dust.altitude = 0.02f;
    dust.coverage = 0.15f;
    dust.density = 0.3f;
    dust.color = {0.85f, 0.7f, 0.55f};
    p.atmosphere.cloudLayers.push_back(dust);

    return p;
}

// ============================================================================
// VENUS - Hellish greenhouse world
// ============================================================================
CelestialBodyParams CelestialBodyParams::Venus() {
    CelestialBodyParams p;
    p.name = "Venus";
    p.bodyType = CelestialBodyType::Terrestrial;
    p.radius = 0.95f;
    p.mass = 0.815f;
    p.rotationPeriod = 5832.0f;  // 243 Earth days, retrograde
    p.axialTilt = 177.4f;  // Upside down
    p.surfaceGravity = 0.9f;
    p.surfaceTemp = 737.0f;
    p.tempVariation = 5.0f;  // Very uniform due to thick atmosphere

    // Terrain - Volcanic, no oceans
    p.terrain.seaLevel = 0.0f;
    p.terrain.heightScale = 0.08f;
    p.terrain.craterDensity = 0.15f;  // Some, but resurfaced
    p.terrain.volcanicActivity = 0.6f;  // Very active
    p.terrain.tectonics.plateScale = 2.0f;

    // No biomes - too hot
    p.biome.globalMoisture = 0.0f;
    p.biome.vegetationDensity = 0.0f;
    p.biome.polarIceExtent = 0.0f;

    // Surface colors - Dark volcanic
    p.colors.lowlandGrass = {0.3f, 0.25f, 0.2f};
    p.colors.highland = {0.35f, 0.28f, 0.22f};
    p.colors.mountain = {0.4f, 0.32f, 0.25f};
    p.colors.lava = {1.0f, 0.4f, 0.1f};
    p.colors.volcanic = {0.2f, 0.15f, 0.1f};

    // Atmosphere - Extremely thick, yellow/orange
    p.atmosphere.rayleighCoeff = {10e-6f, 6e-6f, 3e-6f};
    p.atmosphere.rayleighScale = 1.0f;
    p.atmosphere.height = 0.15f;
    p.atmosphere.density = 90.0f;  // 90x Earth!
    p.atmosphere.hazeStrength = 0.9f;
    p.atmosphere.hazeColor = {0.9f, 0.8f, 0.5f};

    // Thick cloud deck - surface invisible
    CloudLayer sulfuricClouds;
    sulfuricClouds.altitude = 0.08f;
    sulfuricClouds.thickness = 0.04f;
    sulfuricClouds.coverage = 1.0f;  // Complete coverage
    sulfuricClouds.density = 0.95f;
    sulfuricClouds.color = {0.95f, 0.9f, 0.7f};
    p.atmosphere.cloudLayers.push_back(sulfuricClouds);

    return p;
}

// ============================================================================
// JUPITER - Gas giant reference
// ============================================================================
CelestialBodyParams CelestialBodyParams::Jupiter() {
    CelestialBodyParams p;
    p.name = "Jupiter";
    p.bodyType = CelestialBodyType::GasGiant;
    p.radius = 11.2f;
    p.mass = 317.8f;
    p.rotationPeriod = 9.93f;  // Fast rotation
    p.axialTilt = 3.13f;
    p.surfaceGravity = 2.36f;
    p.surfaceTemp = 165.0f;  // Cloud top temp

    // Gas giant params
    p.gasGiant.bandCount = 12.0f;
    p.gasGiant.bandContrast = 0.35f;
    p.gasGiant.bandTurbulence = 0.4f;
    p.gasGiant.jetStreamStrength = 0.6f;
    p.gasGiant.bandColor1 = {0.85f, 0.75f, 0.65f};  // Light tan
    p.gasGiant.bandColor2 = {0.65f, 0.5f, 0.4f};    // Dark brown
    p.gasGiant.stormColor = {0.95f, 0.9f, 0.85f};

    // Great Red Spot
    p.gasGiant.stormFrequency = 0.15f;
    p.gasGiant.greatSpotSize = 0.12f;
    p.gasGiant.greatSpotPosition = {0.0f, -0.22f};  // Southern hemisphere
    p.gasGiant.greatSpotColor = {0.9f, 0.55f, 0.45f};
    p.gasGiant.cloudDepth = 0.4f;

    // Atmosphere
    p.atmosphere.height = 0.2f;
    p.atmosphere.density = 1.0f;  // Relative to cloud tops
    p.atmosphere.rayleighCoeff = {3e-6f, 5e-6f, 8e-6f};

    // Faint rings
    p.rings.hasRings = true;
    p.rings.innerRadius = 1.3f;
    p.rings.outerRadius = 1.8f;
    p.rings.density = 0.05f;  // Very faint

    return p;
}

// ============================================================================
// SATURN - Ringed gas giant
// ============================================================================
CelestialBodyParams CelestialBodyParams::Saturn() {
    CelestialBodyParams p;
    p.name = "Saturn";
    p.bodyType = CelestialBodyType::GasGiant;
    p.radius = 9.45f;
    p.mass = 95.16f;
    p.rotationPeriod = 10.7f;
    p.axialTilt = 26.73f;
    p.surfaceGravity = 0.92f;
    p.surfaceTemp = 134.0f;

    // Gas giant params - more muted than Jupiter
    p.gasGiant.bandCount = 8.0f;
    p.gasGiant.bandContrast = 0.2f;  // More subtle
    p.gasGiant.bandTurbulence = 0.3f;
    p.gasGiant.jetStreamStrength = 0.7f;  // Fastest winds
    p.gasGiant.bandColor1 = {0.9f, 0.85f, 0.7f};   // Pale yellow
    p.gasGiant.bandColor2 = {0.8f, 0.7f, 0.55f};   // Tan
    p.gasGiant.stormFrequency = 0.08f;
    p.gasGiant.greatSpotSize = 0.0f;  // No persistent spot

    // Spectacular rings
    p.rings.hasRings = true;
    p.rings.innerRadius = 1.1f;
    p.rings.outerRadius = 2.3f;
    p.rings.thickness = 0.001f;
    p.rings.density = 0.7f;
    p.rings.color = {0.85f, 0.8f, 0.7f};
    p.rings.divisions = 5.0f;  // Multiple gaps

    return p;
}

// ============================================================================
// NEPTUNE - Ice giant
// ============================================================================
CelestialBodyParams CelestialBodyParams::Neptune() {
    CelestialBodyParams p;
    p.name = "Neptune";
    p.bodyType = CelestialBodyType::IceGiant;
    p.radius = 3.88f;
    p.mass = 17.15f;
    p.rotationPeriod = 16.1f;
    p.axialTilt = 28.32f;
    p.surfaceGravity = 1.12f;
    p.surfaceTemp = 72.0f;

    // Ice giant - deep blue with subtle bands
    p.gasGiant.bandCount = 5.0f;
    p.gasGiant.bandContrast = 0.15f;
    p.gasGiant.bandTurbulence = 0.2f;
    p.gasGiant.jetStreamStrength = 0.9f;  // Extreme winds
    p.gasGiant.bandColor1 = {0.3f, 0.45f, 0.7f};   // Light blue
    p.gasGiant.bandColor2 = {0.2f, 0.35f, 0.6f};   // Deep blue
    p.gasGiant.stormColor = {0.9f, 0.9f, 0.95f};
    p.gasGiant.stormFrequency = 0.1f;
    p.gasGiant.greatSpotSize = 0.06f;  // Dark spots
    p.gasGiant.greatSpotColor = {0.15f, 0.25f, 0.45f};

    // Faint rings
    p.rings.hasRings = true;
    p.rings.innerRadius = 1.2f;
    p.rings.outerRadius = 1.6f;
    p.rings.density = 0.1f;

    return p;
}

// ============================================================================
// MOON - Airless cratered body
// ============================================================================
CelestialBodyParams CelestialBodyParams::Moon() {
    CelestialBodyParams p;
    p.name = "Moon";
    p.bodyType = CelestialBodyType::Moon;
    p.radius = 0.273f;
    p.mass = 0.0123f;
    p.rotationPeriod = 655.7f;  // Tidally locked
    p.axialTilt = 1.54f;
    p.surfaceGravity = 0.166f;
    p.surfaceTemp = 250.0f;
    p.tempVariation = 150.0f;  // Huge day/night swing

    // Terrain - Heavily cratered
    p.terrain.seaLevel = 0.0f;
    p.terrain.heightScale = 0.05f;
    p.terrain.craterDensity = 0.7f;
    p.terrain.craterSize = 0.15f;
    p.terrain.volcanicActivity = 0.0f;

    // No biomes
    p.biome.globalMoisture = 0.0f;
    p.biome.vegetationDensity = 0.0f;
    p.biome.polarIceExtent = 0.01f;  // Tiny ice in craters

    // Surface - Gray regolith
    p.colors.lowlandGrass = {0.35f, 0.35f, 0.35f};  // Maria (darker)
    p.colors.highland = {0.55f, 0.55f, 0.55f};     // Highlands (lighter)
    p.colors.mountain = {0.6f, 0.6f, 0.6f};
    p.colors.rock = {0.4f, 0.4f, 0.4f};

    // No atmosphere
    p.atmosphere.density = 0.0f;
    p.atmosphere.height = 0.0f;

    return p;
}

// ============================================================================
// Generate params from observational data (basic version)
// ============================================================================
CelestialBodyParams CelestialBodyParams::fromObservations(
    float radiusEarth,
    float massEarth,
    float tempK,
    float orbitalPeriodDays,
    const std::string& starType
) {
    CelestialBodyParams p;

    p.radius = radiusEarth;
    p.mass = massEarth;
    p.surfaceTemp = tempK;

    // Estimate gravity
    if (massEarth > 0 && radiusEarth > 0) {
        p.surfaceGravity = massEarth / (radiusEarth * radiusEarth);
    }

    // Determine body type from size
    if (radiusEarth > 6.0f) {
        p.bodyType = CelestialBodyType::GasGiant;
    } else if (radiusEarth > 2.5f) {
        p.bodyType = CelestialBodyType::IceGiant;
    } else {
        p.bodyType = CelestialBodyType::Terrestrial;
    }

    // Temperature-based adjustments for terrestrial planets
    if (p.bodyType == CelestialBodyType::Terrestrial) {
        if (tempK > 700) {
            // Venus-like hellscape
            p.terrain.seaLevel = 0.0f;
            p.terrain.volcanicActivity = 0.5f;
            p.biome.vegetationDensity = 0.0f;
            p.atmosphere.density = 50.0f;
            p.atmosphere.hazeStrength = 0.8f;
        } else if (tempK > 350) {
            // Hot desert
            p.terrain.seaLevel = 0.1f;
            p.biome.globalMoisture = 0.1f;
            p.biome.vegetationDensity = 0.1f;
            p.colors.lowlandGrass = {0.7f, 0.55f, 0.35f};
        } else if (tempK > 200) {
            // Potentially habitable
            p.terrain.seaLevel = 0.5f;
            p.biome.globalMoisture = 0.5f;
            p.biome.vegetationDensity = 0.5f;
        } else if (tempK > 150) {
            // Cold but possibly habitable
            p.terrain.seaLevel = 0.4f;
            p.biome.polarIceExtent = 0.3f;
            p.biome.vegetationDensity = 0.3f;
        } else {
            // Frozen world
            p.terrain.seaLevel = 0.0f;  // Frozen over
            p.biome.polarIceExtent = 1.0f;  // Entire surface ice
            p.biome.vegetationDensity = 0.0f;
            p.colors.lowlandGrass = {0.8f, 0.85f, 0.9f};
        }
    }

    return p;
}

// ============================================================================
// Helper functions for physics-based ExoplanetData mapping
// ============================================================================

// Map atmosphere composition JSON to Rayleigh scattering coefficients.
// Reference: Earth N2/O2 -> {5.5e-6, 13.0e-6, 22.4e-6}
static glm::vec3 computeRayleighFromComposition(const std::string& atmosphereJson) {
    glm::vec3 earthCoeff = {5.5e-6f, 13.0e-6f, 22.4e-6f};

    try {
        auto comp = nlohmann::json::parse(atmosphereJson);

        float n2_frac = comp.value("N2", 0.0f);
        float o2_frac = comp.value("O2", 0.0f);
        float co2_frac = comp.value("CO2", 0.0f);
        float h2_frac = comp.value("H2", 0.0f);
        float he_frac = comp.value("He", 0.0f);

        // CO2-thick atmosphere: orange-tinted (Venus analog)
        if (co2_frac > 50.0f) {
            return {10.0e-6f, 6.0e-6f, 3.0e-6f};
        }

        // H2/He dominant: pale blue (Neptune/Uranus)
        if (h2_frac + he_frac > 50.0f) {
            return {3.0e-6f, 8.0e-6f, 15.0e-6f};
        }

        // N2/O2 dominant: Earth-like blue
        if (n2_frac + o2_frac > 50.0f) {
            return earthCoeff;
        }

        // Default: Earth-like
        return earthCoeff;
    } catch (...) {
        return earthCoeff;  // Fallback to Earth-like on parse error
    }
}

// Multi-factor terrestrial surface mapping using temperature, atmosphere, pressure
static void mapTerrestrialSurface(CelestialBodyParams& params, const ExoplanetData& data) {
    float T = data.equilibrium_temp_k.hasValue()
        ? static_cast<float>(data.equilibrium_temp_k.value) : 288.0f;
    float pressure = data.surface_pressure_atm.hasValue()
        ? static_cast<float>(data.surface_pressure_atm.value) : 1.0f;

    // Greenhouse effect: thick CO2 atmosphere raises effective temperature
    float effectiveT = T;
    if (data.atmosphere_composition.hasValue()) {
        try {
            auto comp = nlohmann::json::parse(data.atmosphere_composition.value);
            float co2 = comp.value("CO2", 0.0f);
            if (co2 > 90.0f && pressure > 50.0f) {
                effectiveT = T * 1.8f;  // Venus-like greenhouse
            } else if (co2 > 10.0f) {
                effectiveT = T * (1.0f + co2 / 500.0f);  // Moderate greenhouse
            }
        } catch (...) {}
    }

    // Surface classification based on effective temperature
    if (effectiveT > 700.0f) {
        // Venus/lava world
        params.terrain.seaLevel = 0.0f;
        params.terrain.volcanicActivity = 0.6f;
        params.biome.vegetationDensity = 0.0f;
        params.biome.globalMoisture = 0.0f;
        params.atmosphere.density = pressure;
        params.atmosphere.hazeStrength = 0.9f;
        params.atmosphere.hazeColor = {0.9f, 0.8f, 0.5f};
        params.colors.lava = {1.0f, 0.4f, 0.1f};
        params.colors.volcanic = {0.2f, 0.15f, 0.1f};

        // Venus-like cloud layer: thick, high coverage
        CloudLayer sulfuricClouds;
        sulfuricClouds.altitude = 0.05f;
        sulfuricClouds.thickness = 0.04f;
        sulfuricClouds.coverage = 0.9f;
        sulfuricClouds.density = 0.95f;
        sulfuricClouds.color = {0.95f, 0.9f, 0.7f};
        params.atmosphere.cloudLayers.push_back(sulfuricClouds);

        // Hot lava ocean colors (dark, reddish)
        params.ocean.deepColor = {0.1f, 0.02f, 0.01f};
        params.ocean.shallowColor = {0.3f, 0.05f, 0.02f};
        params.ocean.coastColor = {0.5f, 0.1f, 0.03f};

    } else if (effectiveT > 350.0f) {
        // Hot desert
        params.terrain.seaLevel = 0.1f;
        params.biome.globalMoisture = 0.1f;
        params.biome.vegetationDensity = 0.05f;
        params.colors.desert = {0.85f, 0.65f, 0.4f};
        params.colors.lowlandGrass = {0.7f, 0.55f, 0.35f};

        // Sparse clouds
        CloudLayer thinClouds;
        thinClouds.altitude = 0.02f;
        thinClouds.coverage = 0.2f;
        thinClouds.density = 0.4f;
        params.atmosphere.cloudLayers.push_back(thinClouds);

        // Warm desert ocean colors
        params.ocean.deepColor = {0.02f, 0.04f, 0.12f};
        params.ocean.shallowColor = {0.04f, 0.10f, 0.22f};
        params.ocean.coastColor = {0.08f, 0.18f, 0.30f};

    } else if (effectiveT > 250.0f) {
        // Habitable zone -- consider ocean coverage from AI
        float ocean = data.ocean_coverage_fraction.hasValue()
            ? static_cast<float>(data.ocean_coverage_fraction.value) : 0.5f;
        params.terrain.seaLevel = ocean * 0.8f;
        params.biome.globalMoisture = ocean * 0.8f;
        params.biome.vegetationDensity = std::max(0.0f, (effectiveT - 250.0f) / 100.0f * 0.7f);

        // Habitable cloud layer
        float cloudCov = data.cloud_coverage_fraction.hasValue()
            ? static_cast<float>(data.cloud_coverage_fraction.value) : 0.4f;
        CloudLayer cumulus;
        cumulus.altitude = 0.02f;
        cumulus.coverage = cloudCov;
        cumulus.density = 0.7f;
        cumulus.noise.frequency = 4.0f;
        cumulus.noise.octaves = 5;
        params.atmosphere.cloudLayers.push_back(cumulus);

        // Warm habitable: green-blue ocean
        params.ocean.deepColor = {0.01f, 0.04f, 0.14f};
        params.ocean.shallowColor = {0.03f, 0.12f, 0.28f};
        params.ocean.coastColor = {0.06f, 0.20f, 0.34f};

    } else if (effectiveT > 150.0f) {
        // Cold but possibly habitable (Mars-like)
        params.terrain.seaLevel = 0.0f;
        params.biome.polarIceExtent = 0.3f;
        params.biome.vegetationDensity = 0.0f;
        params.terrain.craterDensity = 0.3f;
        params.atmosphere.hazeStrength = 0.4f;
        params.atmosphere.hazeColor = {0.8f, 0.6f, 0.4f};

        // Thin dust clouds
        CloudLayer dust;
        dust.altitude = 0.02f;
        dust.coverage = 0.15f;
        dust.density = 0.3f;
        dust.color = {0.85f, 0.7f, 0.55f};
        params.atmosphere.cloudLayers.push_back(dust);

        // Cold ocean colors (darker blue)
        params.ocean.deepColor = {0.01f, 0.02f, 0.10f};
        params.ocean.shallowColor = {0.02f, 0.06f, 0.18f};
        params.ocean.coastColor = {0.04f, 0.10f, 0.22f};

    } else {
        // Frozen world
        params.terrain.seaLevel = 0.0f;
        params.biome.polarIceExtent = 1.0f;
        params.biome.vegetationDensity = 0.0f;
        params.colors.lowlandGrass = {0.8f, 0.85f, 0.9f};
        params.colors.ice = {0.85f, 0.9f, 0.95f};

        // Thin wispy clouds
        CloudLayer wisps;
        wisps.altitude = 0.03f;
        wisps.coverage = 0.1f;
        wisps.density = 0.2f;
        wisps.color = {0.9f, 0.92f, 0.95f};
        params.atmosphere.cloudLayers.push_back(wisps);

        // Frozen: gray-blue ocean colors
        params.ocean.deepColor = {0.03f, 0.04f, 0.08f};
        params.ocean.shallowColor = {0.05f, 0.08f, 0.14f};
        params.ocean.coastColor = {0.08f, 0.12f, 0.18f};
    }
}

// Gas giant parameter mapping based on temperature, radius, mass
static void mapGasGiantParams(CelestialBodyParams& params, const ExoplanetData& data) {
    float T = data.equilibrium_temp_k.hasValue()
        ? static_cast<float>(data.equilibrium_temp_k.value) : 165.0f;
    float R = data.radius_earth.hasValue()
        ? static_cast<float>(data.radius_earth.value) : 11.2f;
    float M = data.mass_earth.hasValue()
        ? static_cast<float>(data.mass_earth.value) : 317.8f;

    // Band count scales with radius (larger planets = more bands)
    params.gasGiant.bandCount = std::clamp(R / 11.2f * 12.0f, 4.0f, 20.0f);

    // Hot Jupiters: muted bands, intense storms
    if (T > 1000.0f) {
        params.gasGiant.bandContrast = 0.15f;
        params.gasGiant.stormFrequency = 0.3f;
        params.gasGiant.bandColor1 = {0.4f, 0.35f, 0.3f};    // Dark
        params.gasGiant.bandColor2 = {0.3f, 0.25f, 0.2f};
        params.gasGiant.stormColor = {0.6f, 0.5f, 0.45f};
    } else {
        // Cold gas giants: Jupiter-like high contrast
        params.gasGiant.bandContrast = 0.35f;
        params.gasGiant.stormFrequency = 0.15f;
        params.gasGiant.bandColor1 = {0.85f, 0.75f, 0.65f};  // Light tan
        params.gasGiant.bandColor2 = {0.65f, 0.5f, 0.4f};    // Dark brown
        params.gasGiant.stormColor = {0.95f, 0.9f, 0.85f};
    }

    // Great spot probability scales with mass
    float massRatio = M / 317.8f;
    params.gasGiant.greatSpotSize = (massRatio > 0.5f) ? 0.12f * massRatio : 0.0f;

    // Cloud layers for gas giant: 2 layers at different altitudes
    CloudLayer upperClouds;
    upperClouds.altitude = 0.05f;
    upperClouds.coverage = 0.7f;
    upperClouds.density = 0.6f;
    upperClouds.noise.frequency = 8.0f;
    params.atmosphere.cloudLayers.push_back(upperClouds);

    CloudLayer lowerClouds;
    lowerClouds.altitude = 0.02f;
    lowerClouds.coverage = 0.9f;
    lowerClouds.density = 0.8f;
    lowerClouds.noise.frequency = 4.0f;
    params.atmosphere.cloudLayers.push_back(lowerClouds);
}

// Ice giant mapping: similar to gas giant but with bluer tint, fewer bands
static void mapIceGiantParams(CelestialBodyParams& params, const ExoplanetData& data) {
    float R = data.radius_earth.hasValue()
        ? static_cast<float>(data.radius_earth.value) : 3.88f;
    float M = data.mass_earth.hasValue()
        ? static_cast<float>(data.mass_earth.value) : 17.15f;

    // Fewer bands than gas giant (clamped 4-10)
    params.gasGiant.bandCount = std::clamp(R / 3.88f * 5.0f, 4.0f, 10.0f);

    // Ice giants: bluer tint, moderate contrast
    params.gasGiant.bandContrast = 0.2f;
    params.gasGiant.stormFrequency = 0.1f;
    params.gasGiant.bandColor1 = {0.3f, 0.45f, 0.7f};    // Light blue
    params.gasGiant.bandColor2 = {0.2f, 0.35f, 0.6f};    // Deep blue
    params.gasGiant.stormColor = {0.9f, 0.9f, 0.95f};
    params.gasGiant.jetStreamStrength = 0.9f;  // Extreme winds

    // Great spot: smaller for ice giants
    float massRatio = M / 317.8f;
    params.gasGiant.greatSpotSize = (massRatio > 0.5f) ? 0.06f * massRatio : 0.0f;

    // Cloud layers for ice giant
    CloudLayer upperClouds;
    upperClouds.altitude = 0.04f;
    upperClouds.coverage = 0.5f;
    upperClouds.density = 0.5f;
    upperClouds.noise.frequency = 6.0f;
    params.atmosphere.cloudLayers.push_back(upperClouds);

    CloudLayer lowerClouds;
    lowerClouds.altitude = 0.02f;
    lowerClouds.coverage = 0.7f;
    lowerClouds.density = 0.7f;
    lowerClouds.noise.frequency = 3.0f;
    params.atmosphere.cloudLayers.push_back(lowerClouds);
}

// ============================================================================
// Generate params from enriched ExoplanetData (physics-based)
// ============================================================================
CelestialBodyParams CelestialBodyParams::fromObservations(const ExoplanetData& data) {
    CelestialBodyParams p;

    // 1. Set name
    p.name = data.name;

    // 2. Physical properties (with defaults)
    float R = data.radius_earth.hasValue()
        ? static_cast<float>(data.radius_earth.value) : 1.0f;
    float M = data.mass_earth.hasValue()
        ? static_cast<float>(data.mass_earth.value) : 1.0f;

    p.radius = R;
    p.mass = M;

    // Surface temperature from equilibrium temperature
    p.surfaceTemp = data.equilibrium_temp_k.hasValue()
        ? static_cast<float>(data.equilibrium_temp_k.value) : 288.0f;

    // Surface gravity: from data, or calculated from mass/radius^2
    if (data.surface_gravity_g.hasValue()) {
        p.surfaceGravity = static_cast<float>(data.surface_gravity_g.value);
    } else if (data.mass_earth.hasValue() && data.radius_earth.hasValue() && R > 0.0f) {
        p.surfaceGravity = M / (R * R);
    }

    // 3. Body type classification (user's locked thresholds)
    if (data.radius_earth.hasValue()) {
        float radius = static_cast<float>(data.radius_earth.value);
        if (radius < 2.0f) {
            p.bodyType = CelestialBodyType::Terrestrial;
        } else if (radius <= 6.0f) {
            p.bodyType = CelestialBodyType::IceGiant;
        } else {
            p.bodyType = CelestialBodyType::GasGiant;
        }
    } else if (data.mass_earth.hasValue()) {
        // Mass fallback when radius unavailable
        float mass = static_cast<float>(data.mass_earth.value);
        if (mass > 50.0f) {
            p.bodyType = CelestialBodyType::GasGiant;
        } else if (mass > 10.0f) {
            p.bodyType = CelestialBodyType::IceGiant;
        } else {
            p.bodyType = CelestialBodyType::Terrestrial;
        }
    } else {
        // Neither available: default to Terrestrial
        p.bodyType = CelestialBodyType::Terrestrial;
    }

    // 4. Rayleigh scattering from atmosphere composition
    if (data.atmosphere_composition.hasValue() && !data.atmosphere_composition.value.empty()) {
        p.atmosphere.rayleighCoeff = computeRayleighFromComposition(
            data.atmosphere_composition.value);
    }
    // else: keep default Earth-like rayleighCoeff from struct defaults

    // 5. Atmosphere density and height from surface pressure
    if (data.surface_pressure_atm.hasValue()) {
        float pressure = static_cast<float>(data.surface_pressure_atm.value);
        p.atmosphere.density = pressure;
        // Height scales with pressure (thicker atmosphere = taller)
        p.atmosphere.height = std::clamp(0.08f * std::sqrt(pressure), 0.01f, 0.3f);
    }

    // 6. Branch on body type for surface/band mapping
    switch (p.bodyType) {
        case CelestialBodyType::Terrestrial:
            mapTerrestrialSurface(p, data);
            break;
        case CelestialBodyType::GasGiant:
            mapGasGiantParams(p, data);
            break;
        case CelestialBodyType::IceGiant:
            mapIceGiantParams(p, data);
            break;
        default:
            break;
    }

    // 7. Seed from name hash (reproducible)
    p.seed = static_cast<uint32_t>(std::hash<std::string>{}(data.name));

    return p;
}

}  // namespace astrocore
