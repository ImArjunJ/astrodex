#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "render/PlanetParams.hpp"
#include "data/ExoplanetData.hpp"
#include <cmath>
#include <limits>

using namespace astrocore;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

// Helper to create a default ExoplanetData with NaN fields
static ExoplanetData makeDefaultData(const std::string& name = "TestPlanet") {
    ExoplanetData data;
    data.name = name;
    return data;
}

// Helper to set a MeasuredValue<double>
template<typename T>
static MeasuredValue<T> mv(T val, DataSource src = DataSource::NASA_TAP) {
    return MeasuredValue<T>(val, src);
}

// ============================================================================
// BODY TYPE CLASSIFICATION
// ============================================================================
TEST_CASE("fromObservations(ExoplanetData): body type classification", "[planet_params][body_type]") {

    SECTION("radius < 2.0 Earth -> Terrestrial") {
        auto data = makeDefaultData("SmallRocky");
        data.radius_earth = mv(1.34);
        auto params = CelestialBodyParams::fromObservations(data);
        REQUIRE(params.bodyType == CelestialBodyType::Terrestrial);
    }

    SECTION("radius exactly 2.0 Earth -> IceGiant (boundary)") {
        auto data = makeDefaultData("Boundary2");
        data.radius_earth = mv(2.0);
        auto params = CelestialBodyParams::fromObservations(data);
        REQUIRE(params.bodyType == CelestialBodyType::IceGiant);
    }

    SECTION("radius 2.0-6.0 Earth -> IceGiant") {
        auto data = makeDefaultData("NeptuneLike");
        data.radius_earth = mv(3.88);
        auto params = CelestialBodyParams::fromObservations(data);
        REQUIRE(params.bodyType == CelestialBodyType::IceGiant);
    }

    SECTION("radius exactly 6.0 Earth -> IceGiant (upper boundary)") {
        auto data = makeDefaultData("Boundary6");
        data.radius_earth = mv(6.0);
        auto params = CelestialBodyParams::fromObservations(data);
        REQUIRE(params.bodyType == CelestialBodyType::IceGiant);
    }

    SECTION("radius > 6.0 Earth -> GasGiant") {
        auto data = makeDefaultData("HotJupiter");
        data.radius_earth = mv(20.7);
        auto params = CelestialBodyParams::fromObservations(data);
        REQUIRE(params.bodyType == CelestialBodyType::GasGiant);
    }

    SECTION("mass fallback: mass > 50 Earth, no radius -> GasGiant") {
        auto data = makeDefaultData("MassiveNoRadius");
        data.mass_earth = mv(318.0);
        // radius_earth remains NaN
        auto params = CelestialBodyParams::fromObservations(data);
        REQUIRE(params.bodyType == CelestialBodyType::GasGiant);
    }

    SECTION("mass fallback: mass > 10 Earth, no radius -> IceGiant") {
        auto data = makeDefaultData("MediumNoRadius");
        data.mass_earth = mv(17.0);
        auto params = CelestialBodyParams::fromObservations(data);
        REQUIRE(params.bodyType == CelestialBodyType::IceGiant);
    }

    SECTION("mass fallback: mass <= 10 Earth, no radius -> Terrestrial") {
        auto data = makeDefaultData("SmallNoRadius");
        data.mass_earth = mv(5.0);
        auto params = CelestialBodyParams::fromObservations(data);
        REQUIRE(params.bodyType == CelestialBodyType::Terrestrial);
    }

    SECTION("neither radius nor mass available -> default Terrestrial") {
        auto data = makeDefaultData("NoData");
        auto params = CelestialBodyParams::fromObservations(data);
        REQUIRE(params.bodyType == CelestialBodyType::Terrestrial);
    }
}

// ============================================================================
// RAYLEIGH SCATTERING FROM ATMOSPHERE COMPOSITION
// ============================================================================
TEST_CASE("fromObservations(ExoplanetData): Rayleigh scattering", "[planet_params][rayleigh]") {

    SECTION("N2/O2 dominant -> Earth-like blue") {
        auto data = makeDefaultData("EarthLike");
        data.radius_earth = mv(1.0);
        data.atmosphere_composition = MeasuredValue<std::string>(
            R"({"N2": 78, "O2": 21, "Ar": 1})", DataSource::AI_INFERRED);
        auto params = CelestialBodyParams::fromObservations(data);
        CHECK_THAT(params.atmosphere.rayleighCoeff.x, WithinAbs(5.5e-6f, 1e-7f));
        CHECK_THAT(params.atmosphere.rayleighCoeff.y, WithinAbs(13.0e-6f, 1e-7f));
        CHECK_THAT(params.atmosphere.rayleighCoeff.z, WithinAbs(22.4e-6f, 1e-7f));
    }

    SECTION("CO2 dominant -> Venus orange") {
        auto data = makeDefaultData("VenusLike");
        data.radius_earth = mv(0.95);
        data.atmosphere_composition = MeasuredValue<std::string>(
            R"({"CO2": 96.5, "N2": 3.5})", DataSource::AI_INFERRED);
        auto params = CelestialBodyParams::fromObservations(data);
        CHECK_THAT(params.atmosphere.rayleighCoeff.x, WithinAbs(10.0e-6f, 1e-7f));
        CHECK_THAT(params.atmosphere.rayleighCoeff.y, WithinAbs(6.0e-6f, 1e-7f));
        CHECK_THAT(params.atmosphere.rayleighCoeff.z, WithinAbs(3.0e-6f, 1e-7f));
    }

    SECTION("H2/He dominant -> pale blue") {
        auto data = makeDefaultData("NeptuneLike");
        data.radius_earth = mv(3.88);
        data.atmosphere_composition = MeasuredValue<std::string>(
            R"({"H2": 80, "He": 19, "CH4": 1})", DataSource::AI_INFERRED);
        auto params = CelestialBodyParams::fromObservations(data);
        CHECK_THAT(params.atmosphere.rayleighCoeff.x, WithinAbs(3.0e-6f, 1e-7f));
        CHECK_THAT(params.atmosphere.rayleighCoeff.y, WithinAbs(8.0e-6f, 1e-7f));
        CHECK_THAT(params.atmosphere.rayleighCoeff.z, WithinAbs(15.0e-6f, 1e-7f));
    }

    SECTION("no atmosphere data -> default Earth-like") {
        auto data = makeDefaultData("NoAtmo");
        data.radius_earth = mv(1.0);
        // atmosphere_composition not set (default is empty string with NaN-like behavior)
        auto params = CelestialBodyParams::fromObservations(data);
        CHECK_THAT(params.atmosphere.rayleighCoeff.x, WithinAbs(5.5e-6f, 1e-7f));
        CHECK_THAT(params.atmosphere.rayleighCoeff.y, WithinAbs(13.0e-6f, 1e-7f));
        CHECK_THAT(params.atmosphere.rayleighCoeff.z, WithinAbs(22.4e-6f, 1e-7f));
    }

    SECTION("invalid JSON -> default Earth-like, no crash") {
        auto data = makeDefaultData("BadJSON");
        data.radius_earth = mv(1.0);
        data.atmosphere_composition = MeasuredValue<std::string>(
            "not valid json at all", DataSource::AI_INFERRED);
        auto params = CelestialBodyParams::fromObservations(data);
        CHECK_THAT(params.atmosphere.rayleighCoeff.x, WithinAbs(5.5e-6f, 1e-7f));
        CHECK_THAT(params.atmosphere.rayleighCoeff.y, WithinAbs(13.0e-6f, 1e-7f));
        CHECK_THAT(params.atmosphere.rayleighCoeff.z, WithinAbs(22.4e-6f, 1e-7f));
    }
}

// ============================================================================
// TERRESTRIAL SURFACE MAPPING (MULTI-FACTOR)
// ============================================================================
TEST_CASE("fromObservations(ExoplanetData): terrestrial surface mapping", "[planet_params][terrestrial]") {

    SECTION("Venus/lava world (T > 700K): volcanic, no sea, dense haze") {
        auto data = makeDefaultData("LavaWorld");
        data.radius_earth = mv(0.95);
        data.equilibrium_temp_k = mv(740.0);
        auto params = CelestialBodyParams::fromObservations(data);
        REQUIRE(params.bodyType == CelestialBodyType::Terrestrial);
        CHECK(params.terrain.seaLevel == 0.0f);
        CHECK_THAT(params.terrain.volcanicActivity, WithinAbs(0.6f, 0.01f));
        CHECK(params.biome.vegetationDensity == 0.0f);
        CHECK(params.atmosphere.hazeStrength > 0.5f);
    }

    SECTION("hot desert (350-700K): low seaLevel, low moisture") {
        auto data = makeDefaultData("HotDesert");
        data.radius_earth = mv(1.2);
        data.equilibrium_temp_k = mv(450.0);
        auto params = CelestialBodyParams::fromObservations(data);
        REQUIRE(params.bodyType == CelestialBodyType::Terrestrial);
        CHECK_THAT(params.terrain.seaLevel, WithinAbs(0.1f, 0.01f));
        CHECK_THAT(params.biome.globalMoisture, WithinAbs(0.1f, 0.01f));
        CHECK(params.biome.vegetationDensity < 0.1f);
    }

    SECTION("habitable zone (250-350K): ocean, vegetation") {
        auto data = makeDefaultData("Habitable");
        data.radius_earth = mv(1.0);
        data.equilibrium_temp_k = mv(288.0);
        data.ocean_coverage_fraction = mv(0.7);
        auto params = CelestialBodyParams::fromObservations(data);
        REQUIRE(params.bodyType == CelestialBodyType::Terrestrial);
        // ocean scales with data: 0.7 * 0.8 = 0.56
        CHECK_THAT(params.terrain.seaLevel, WithinAbs(0.56f, 0.05f));
        CHECK(params.biome.vegetationDensity > 0.0f);
    }

    SECTION("habitable zone default ocean when not provided") {
        auto data = makeDefaultData("HabitableNoOcean");
        data.radius_earth = mv(1.0);
        data.equilibrium_temp_k = mv(300.0);
        // ocean_coverage_fraction not set
        auto params = CelestialBodyParams::fromObservations(data);
        // default ocean = 0.5, seaLevel = 0.5 * 0.8 = 0.4
        CHECK_THAT(params.terrain.seaLevel, WithinAbs(0.4f, 0.05f));
    }

    SECTION("cold Mars-like (150-250K): no sea, ice, craters, thin haze") {
        auto data = makeDefaultData("ColdMars");
        data.radius_earth = mv(0.5);
        data.equilibrium_temp_k = mv(210.0);
        auto params = CelestialBodyParams::fromObservations(data);
        REQUIRE(params.bodyType == CelestialBodyType::Terrestrial);
        CHECK(params.terrain.seaLevel == 0.0f);
        CHECK_THAT(params.biome.polarIceExtent, WithinAbs(0.3f, 0.01f));
        CHECK(params.biome.vegetationDensity == 0.0f);
        CHECK_THAT(params.terrain.craterDensity, WithinAbs(0.3f, 0.01f));
        CHECK(params.atmosphere.hazeStrength > 0.0f);
    }

    SECTION("frozen world (T < 150K): full ice, no vegetation") {
        auto data = makeDefaultData("FrozenWorld");
        data.radius_earth = mv(1.0);
        data.equilibrium_temp_k = mv(80.0);
        auto params = CelestialBodyParams::fromObservations(data);
        REQUIRE(params.bodyType == CelestialBodyType::Terrestrial);
        CHECK(params.terrain.seaLevel == 0.0f);
        CHECK_THAT(params.biome.polarIceExtent, WithinAbs(1.0f, 0.01f));
        CHECK(params.biome.vegetationDensity == 0.0f);
    }

    SECTION("CO2 greenhouse: thick CO2 + high pressure -> T*1.8 Venus analog") {
        // Equilibrium T=232K but with 96.5% CO2 at 92 atm -> effectiveT = 232*1.8 = 417.6K
        // This should be in "hot desert" range after greenhouse effect
        auto data = makeDefaultData("VenusAnalog");
        data.radius_earth = mv(0.95);
        data.equilibrium_temp_k = mv(232.0);
        data.surface_pressure_atm = mv(92.0);
        data.atmosphere_composition = MeasuredValue<std::string>(
            R"({"CO2": 96.5, "N2": 3.5})", DataSource::AI_INFERRED);
        auto params = CelestialBodyParams::fromObservations(data);
        // effectiveT = 232 * 1.8 = 417.6K -> hot desert range (350-700K)
        CHECK_THAT(params.terrain.seaLevel, WithinAbs(0.1f, 0.05f));
        CHECK(params.biome.vegetationDensity < 0.1f);
    }

    SECTION("moderate CO2 greenhouse: T scaled by (1 + co2/500)") {
        auto data = makeDefaultData("ModerateCO2");
        data.radius_earth = mv(1.0);
        data.equilibrium_temp_k = mv(250.0);
        data.surface_pressure_atm = mv(2.0);
        data.atmosphere_composition = MeasuredValue<std::string>(
            R"({"CO2": 20, "N2": 70, "O2": 10})", DataSource::AI_INFERRED);
        auto params = CelestialBodyParams::fromObservations(data);
        // effectiveT = 250 * (1 + 20/500) = 250 * 1.04 = 260K -> habitable range
        CHECK(params.terrain.seaLevel > 0.0f); // habitable, should have some ocean
    }
}

// ============================================================================
// GAS GIANT MAPPING
// ============================================================================
TEST_CASE("fromObservations(ExoplanetData): gas giant mapping", "[planet_params][gas_giant]") {

    SECTION("hot Jupiter: muted bands, intense storms, dark colors") {
        auto data = makeDefaultData("HotJupiter");
        data.radius_earth = mv(20.7);
        data.mass_earth = mv(1000.0);
        data.equilibrium_temp_k = mv(2580.0);
        auto params = CelestialBodyParams::fromObservations(data);
        REQUIRE(params.bodyType == CelestialBodyType::GasGiant);
        CHECK_THAT(params.gasGiant.bandContrast, WithinAbs(0.15f, 0.01f));
        CHECK_THAT(params.gasGiant.stormFrequency, WithinAbs(0.3f, 0.01f));
        // Dark colors for hot Jupiter
        CHECK(params.gasGiant.bandColor1.r < 0.5f);
    }

    SECTION("cold gas giant: high contrast, fewer storms, bright bands") {
        auto data = makeDefaultData("ColdGasGiant");
        data.radius_earth = mv(11.2);
        data.mass_earth = mv(317.8);
        data.equilibrium_temp_k = mv(165.0);
        auto params = CelestialBodyParams::fromObservations(data);
        REQUIRE(params.bodyType == CelestialBodyType::GasGiant);
        CHECK_THAT(params.gasGiant.bandContrast, WithinAbs(0.35f, 0.01f));
        CHECK_THAT(params.gasGiant.stormFrequency, WithinAbs(0.15f, 0.01f));
        CHECK(params.gasGiant.bandColor1.r > 0.7f);
    }

    SECTION("band count scales with radius") {
        auto data = makeDefaultData("LargeGasGiant");
        data.radius_earth = mv(22.4); // 2x Jupiter
        data.equilibrium_temp_k = mv(165.0);
        auto params = CelestialBodyParams::fromObservations(data);
        // bandCount = clamp(22.4/11.2 * 12, 4, 20) = clamp(24, 4, 20) = 20
        CHECK_THAT(params.gasGiant.bandCount, WithinAbs(20.0f, 0.01f));
    }

    SECTION("band count minimum clamped to 4") {
        auto data = makeDefaultData("SmallGasGiant");
        data.radius_earth = mv(7.0); // Just above 6.0 threshold
        data.equilibrium_temp_k = mv(165.0);
        auto params = CelestialBodyParams::fromObservations(data);
        // bandCount = clamp(7.0/11.2 * 12, 4, 20) = clamp(7.5, 4, 20) = 7.5
        CHECK(params.gasGiant.bandCount >= 4.0f);
        CHECK(params.gasGiant.bandCount <= 20.0f);
    }

    SECTION("great spot scales with mass: large mass -> spot") {
        auto data = makeDefaultData("MassiveJupiter");
        data.radius_earth = mv(11.2);
        data.mass_earth = mv(317.8); // Jupiter mass -> M/317.8 = 1.0 > 0.5
        data.equilibrium_temp_k = mv(165.0);
        auto params = CelestialBodyParams::fromObservations(data);
        // spotSize = 0.12 * 1.0 = 0.12
        CHECK_THAT(params.gasGiant.greatSpotSize, WithinAbs(0.12f, 0.01f));
    }

    SECTION("great spot: small mass -> no spot") {
        auto data = makeDefaultData("SmallGiant");
        data.radius_earth = mv(7.0);
        data.mass_earth = mv(100.0); // M/317.8 = 0.31 < 0.5
        data.equilibrium_temp_k = mv(165.0);
        auto params = CelestialBodyParams::fromObservations(data);
        CHECK(params.gasGiant.greatSpotSize == 0.0f);
    }
}

// ============================================================================
// ICE GIANT MAPPING
// ============================================================================
TEST_CASE("fromObservations(ExoplanetData): ice giant mapping", "[planet_params][ice_giant]") {

    SECTION("Neptune analog: IceGiant, banded, bluer tint, fewer bands") {
        auto data = makeDefaultData("NeptuneAnalog");
        data.radius_earth = mv(3.88);
        data.mass_earth = mv(17.15);
        data.equilibrium_temp_k = mv(72.0);
        data.atmosphere_composition = MeasuredValue<std::string>(
            R"({"H2": 80, "He": 19, "CH4": 1})", DataSource::AI_INFERRED);
        auto params = CelestialBodyParams::fromObservations(data);
        REQUIRE(params.bodyType == CelestialBodyType::IceGiant);
        // Fewer bands than gas giant (clamped 4-10)
        CHECK(params.gasGiant.bandCount >= 4.0f);
        CHECK(params.gasGiant.bandCount <= 10.0f);
        // Bluer tint
        CHECK(params.gasGiant.bandColor1.b > params.gasGiant.bandColor1.r);
    }
}

// ============================================================================
// PHYSICAL PROPERTIES MAPPING
// ============================================================================
TEST_CASE("fromObservations(ExoplanetData): physical properties", "[planet_params][physical]") {

    SECTION("sets name from data") {
        auto data = makeDefaultData("Kepler-442b");
        data.radius_earth = mv(1.34);
        auto params = CelestialBodyParams::fromObservations(data);
        CHECK(params.name == "Kepler-442b");
    }

    SECTION("radius and mass from data") {
        auto data = makeDefaultData("TestPlanet");
        data.radius_earth = mv(1.5);
        data.mass_earth = mv(3.0);
        auto params = CelestialBodyParams::fromObservations(data);
        CHECK_THAT(params.radius, WithinAbs(1.5f, 0.01f));
        CHECK_THAT(params.mass, WithinAbs(3.0f, 0.01f));
    }

    SECTION("defaults when radius/mass unavailable") {
        auto data = makeDefaultData("NoPhysical");
        auto params = CelestialBodyParams::fromObservations(data);
        CHECK_THAT(params.radius, WithinAbs(1.0f, 0.01f));
        CHECK_THAT(params.mass, WithinAbs(1.0f, 0.01f));
    }

    SECTION("surface gravity from data") {
        auto data = makeDefaultData("GravityPlanet");
        data.radius_earth = mv(1.0);
        data.mass_earth = mv(1.0);
        data.surface_gravity_g = mv(1.5);
        auto params = CelestialBodyParams::fromObservations(data);
        CHECK_THAT(params.surfaceGravity, WithinAbs(1.5f, 0.01f));
    }

    SECTION("surface gravity calculated from mass/radius^2 when not provided") {
        auto data = makeDefaultData("CalcGravity");
        data.radius_earth = mv(2.0);
        data.mass_earth = mv(8.0);
        // gravity = mass / radius^2 = 8 / 4 = 2.0
        auto params = CelestialBodyParams::fromObservations(data);
        CHECK_THAT(params.surfaceGravity, WithinAbs(2.0f, 0.01f));
    }

    SECTION("surface temperature from equilibrium_temp_k") {
        auto data = makeDefaultData("TempPlanet");
        data.radius_earth = mv(1.0);
        data.equilibrium_temp_k = mv(350.0);
        auto params = CelestialBodyParams::fromObservations(data);
        CHECK_THAT(params.surfaceTemp, WithinAbs(350.0f, 1.0f));
    }

    SECTION("seed from hash of planet name (reproducible)") {
        auto data1 = makeDefaultData("Kepler-442b");
        data1.radius_earth = mv(1.34);
        auto params1 = CelestialBodyParams::fromObservations(data1);

        auto data2 = makeDefaultData("Kepler-442b");
        data2.radius_earth = mv(1.34);
        auto params2 = CelestialBodyParams::fromObservations(data2);

        CHECK(params1.seed == params2.seed);
        CHECK(params1.seed != 0); // Should be a non-zero hash
    }

    SECTION("different names produce different seeds") {
        auto data1 = makeDefaultData("PlanetA");
        data1.radius_earth = mv(1.0);
        auto data2 = makeDefaultData("PlanetB");
        data2.radius_earth = mv(1.0);
        auto params1 = CelestialBodyParams::fromObservations(data1);
        auto params2 = CelestialBodyParams::fromObservations(data2);
        CHECK(params1.seed != params2.seed);
    }
}

// ============================================================================
// CLOUD LAYERS
// ============================================================================
TEST_CASE("fromObservations(ExoplanetData): cloud layers", "[planet_params][clouds]") {

    SECTION("habitable terrestrial: 1 cloud layer") {
        auto data = makeDefaultData("CloudyEarth");
        data.radius_earth = mv(1.0);
        data.equilibrium_temp_k = mv(288.0);
        auto params = CelestialBodyParams::fromObservations(data);
        CHECK(params.atmosphere.cloudLayers.size() >= 1);
    }

    SECTION("Venus-like: thick cloud layer, high coverage") {
        auto data = makeDefaultData("CloudyVenus");
        data.radius_earth = mv(0.95);
        data.equilibrium_temp_k = mv(740.0);
        auto params = CelestialBodyParams::fromObservations(data);
        CHECK(params.atmosphere.cloudLayers.size() >= 1);
        if (!params.atmosphere.cloudLayers.empty()) {
            CHECK(params.atmosphere.cloudLayers[0].coverage > 0.8f);
        }
    }

    SECTION("gas giant: 2 cloud layers") {
        auto data = makeDefaultData("CloudyJupiter");
        data.radius_earth = mv(11.2);
        data.equilibrium_temp_k = mv(165.0);
        auto params = CelestialBodyParams::fromObservations(data);
        CHECK(params.atmosphere.cloudLayers.size() >= 2);
    }

    SECTION("frozen world: thin wispy clouds, low coverage") {
        auto data = makeDefaultData("FrozenClouds");
        data.radius_earth = mv(1.0);
        data.equilibrium_temp_k = mv(80.0);
        auto params = CelestialBodyParams::fromObservations(data);
        if (!params.atmosphere.cloudLayers.empty()) {
            CHECK(params.atmosphere.cloudLayers[0].coverage < 0.3f);
        }
    }

    SECTION("cloud coverage from data") {
        auto data = makeDefaultData("SpecificClouds");
        data.radius_earth = mv(1.0);
        data.equilibrium_temp_k = mv(288.0);
        data.cloud_coverage_fraction = mv(0.6);
        auto params = CelestialBodyParams::fromObservations(data);
        if (!params.atmosphere.cloudLayers.empty()) {
            CHECK_THAT(params.atmosphere.cloudLayers[0].coverage, WithinAbs(0.6f, 0.1f));
        }
    }
}

// ============================================================================
// REAL PLANET TEST CASES
// ============================================================================
TEST_CASE("fromObservations(ExoplanetData): Kepler-442b", "[planet_params][real]") {
    auto data = makeDefaultData("Kepler-442b");
    data.radius_earth = mv(1.34);
    data.equilibrium_temp_k = mv(233.0);
    // No atmosphere data -> Earth-like defaults
    auto params = CelestialBodyParams::fromObservations(data);

    CHECK(params.name == "Kepler-442b");
    CHECK(params.bodyType == CelestialBodyType::Terrestrial);
    // T=233K is in the cold range (150-250K): Mars-like
    CHECK(params.terrain.seaLevel == 0.0f);
    CHECK(params.biome.polarIceExtent > 0.0f);
    // Earth-like Rayleigh (no atmosphere data)
    CHECK_THAT(params.atmosphere.rayleighCoeff.x, WithinAbs(5.5e-6f, 1e-7f));
}

TEST_CASE("fromObservations(ExoplanetData): WASP-12b hot Jupiter", "[planet_params][real]") {
    auto data = makeDefaultData("WASP-12b");
    data.radius_earth = mv(20.7);
    data.mass_earth = mv(1410.0);
    data.equilibrium_temp_k = mv(2580.0);
    auto params = CelestialBodyParams::fromObservations(data);

    CHECK(params.name == "WASP-12b");
    CHECK(params.bodyType == CelestialBodyType::GasGiant);
    CHECK_THAT(params.gasGiant.bandContrast, WithinAbs(0.15f, 0.01f));
    CHECK_THAT(params.gasGiant.stormFrequency, WithinAbs(0.3f, 0.01f));
    CHECK(params.gasGiant.bandColor1.r < 0.5f); // dark colors
}

TEST_CASE("fromObservations(ExoplanetData): Venus analog", "[planet_params][real]") {
    auto data = makeDefaultData("VenusAnalog");
    data.radius_earth = mv(0.95);
    data.equilibrium_temp_k = mv(232.0);
    data.surface_pressure_atm = mv(92.0);
    data.atmosphere_composition = MeasuredValue<std::string>(
        R"({"CO2": 96.5, "N2": 3.5})", DataSource::AI_INFERRED);
    auto params = CelestialBodyParams::fromObservations(data);

    CHECK(params.bodyType == CelestialBodyType::Terrestrial);
    // CO2 Rayleigh
    CHECK_THAT(params.atmosphere.rayleighCoeff.x, WithinAbs(10.0e-6f, 1e-7f));
    // effectiveT = 232 * 1.8 = 417.6K -> hot desert range
    // Should have hot desert characteristics
    CHECK(params.biome.vegetationDensity < 0.1f);
}

TEST_CASE("fromObservations(ExoplanetData): Neptune analog", "[planet_params][real]") {
    auto data = makeDefaultData("NeptuneAnalog");
    data.radius_earth = mv(3.88);
    data.mass_earth = mv(17.15);
    data.equilibrium_temp_k = mv(72.0);
    data.atmosphere_composition = MeasuredValue<std::string>(
        R"({"H2": 80, "He": 19, "CH4": 1})", DataSource::AI_INFERRED);
    auto params = CelestialBodyParams::fromObservations(data);

    CHECK(params.bodyType == CelestialBodyType::IceGiant);
    // H2/He Rayleigh -> pale blue
    CHECK_THAT(params.atmosphere.rayleighCoeff.x, WithinAbs(3.0e-6f, 1e-7f));
    CHECK_THAT(params.atmosphere.rayleighCoeff.y, WithinAbs(8.0e-6f, 1e-7f));
    CHECK_THAT(params.atmosphere.rayleighCoeff.z, WithinAbs(15.0e-6f, 1e-7f));
    // Banded
    CHECK(params.gasGiant.bandCount >= 4.0f);
}

TEST_CASE("fromObservations(ExoplanetData): empty data (all NaN)", "[planet_params][real]") {
    auto data = makeDefaultData("EmptyPlanet");
    // All numeric fields remain NaN (default)

    // Must not crash
    auto params = CelestialBodyParams::fromObservations(data);

    CHECK(params.name == "EmptyPlanet");
    CHECK(params.bodyType == CelestialBodyType::Terrestrial); // default
    // Earth-like defaults
    CHECK_THAT(params.radius, WithinAbs(1.0f, 0.01f));
    CHECK_THAT(params.mass, WithinAbs(1.0f, 0.01f));
    CHECK_THAT(params.atmosphere.rayleighCoeff.x, WithinAbs(5.5e-6f, 1e-7f));
}

// ============================================================================
// OLD OVERLOAD COMPATIBILITY
// ============================================================================
TEST_CASE("old fromObservations(5 scalars) still compiles and works", "[planet_params][compat]") {
    auto params = CelestialBodyParams::fromObservations(1.0f, 1.0f, 288.0f, 365.0f, "G2V");
    CHECK_THAT(params.radius, WithinAbs(1.0f, 0.01f));
    CHECK_THAT(params.mass, WithinAbs(1.0f, 0.01f));
    CHECK_THAT(params.surfaceTemp, WithinAbs(288.0f, 1.0f));
}

// ============================================================================
// OCEAN COLORS
// ============================================================================
TEST_CASE("fromObservations(ExoplanetData): ocean colors by temperature", "[planet_params][ocean]") {

    SECTION("warm habitable: green-blue ocean") {
        auto data = makeDefaultData("WarmOcean");
        data.radius_earth = mv(1.0);
        data.equilibrium_temp_k = mv(300.0);
        data.ocean_coverage_fraction = mv(0.7);
        auto params = CelestialBodyParams::fromObservations(data);
        // Warm ocean should have some green tint
        CHECK(params.ocean.shallowColor.g > 0.05f);
    }

    SECTION("cold: dark-blue ocean colors") {
        auto data = makeDefaultData("ColdOcean");
        data.radius_earth = mv(1.0);
        data.equilibrium_temp_k = mv(260.0);
        data.ocean_coverage_fraction = mv(0.5);
        auto params = CelestialBodyParams::fromObservations(data);
        // Cold ocean should be darker blue
        CHECK(params.ocean.deepColor.b > params.ocean.deepColor.r);
    }
}

// ============================================================================
// ATMOSPHERE DENSITY/HEIGHT FROM SURFACE PRESSURE
// ============================================================================
TEST_CASE("fromObservations(ExoplanetData): atmosphere from surface pressure", "[planet_params][atmosphere]") {

    SECTION("high pressure -> dense atmosphere") {
        auto data = makeDefaultData("DenseAtmo");
        data.radius_earth = mv(1.0);
        data.equilibrium_temp_k = mv(288.0);
        data.surface_pressure_atm = mv(90.0);
        auto params = CelestialBodyParams::fromObservations(data);
        CHECK(params.atmosphere.density > 10.0f);
    }

    SECTION("low pressure -> thin atmosphere") {
        auto data = makeDefaultData("ThinAtmo");
        data.radius_earth = mv(1.0);
        data.equilibrium_temp_k = mv(210.0);
        data.surface_pressure_atm = mv(0.006);
        auto params = CelestialBodyParams::fromObservations(data);
        CHECK(params.atmosphere.density < 0.5f);
    }
}
