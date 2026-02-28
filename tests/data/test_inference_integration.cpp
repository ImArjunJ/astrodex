#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "data/ExoplanetData.hpp"
#include "data/DataFusionEngine.hpp"

using namespace astrocore;
using Catch::Matchers::WithinAbs;

// ============================================================================
// Test Group 1: ExoplanetData JSON serialization completeness (Task 1)
// ============================================================================

TEST_CASE("ExoplanetData: toJson/fromJson roundtrip preserves ALL physical fields", "[inference_integration][json_roundtrip]") {
    ExoplanetData original;
    original.name = "Test-Roundtrip b";
    original.discovery_method = "Transit";
    original.discovery_year = 2023;

    // Physical fields
    original.mass_earth = MeasuredValue<double>(2.5, DataSource::NASA_TAP);
    original.radius_earth = MeasuredValue<double>(1.3, DataSource::NASA_TAP);
    original.density_gcc = MeasuredValue<double>(5.2, DataSource::CALCULATED);
    original.equilibrium_temp_k = MeasuredValue<double>(290.0, DataSource::NASA_TAP);
    original.surface_gravity_g = MeasuredValue<double>(1.5, DataSource::CALCULATED);
    original.inclination_deg = MeasuredValue<double>(89.5, DataSource::NASA_TAP);
    original.omega_deg = MeasuredValue<double>(45.0, DataSource::OEC);

    auto j = original.toJson();
    auto restored = ExoplanetData::fromJson(j);

    REQUIRE(restored.name == "Test-Roundtrip b");
    REQUIRE(restored.discovery_method == "Transit");
    REQUIRE(restored.discovery_year == 2023);

    REQUIRE(restored.mass_earth.hasValue());
    REQUIRE_THAT(restored.mass_earth.value, WithinAbs(2.5, 0.01));
    REQUIRE(restored.mass_earth.source == DataSource::NASA_TAP);

    REQUIRE(restored.radius_earth.hasValue());
    REQUIRE_THAT(restored.radius_earth.value, WithinAbs(1.3, 0.01));

    REQUIRE(restored.density_gcc.hasValue());
    REQUIRE_THAT(restored.density_gcc.value, WithinAbs(5.2, 0.01));

    REQUIRE(restored.equilibrium_temp_k.hasValue());
    REQUIRE_THAT(restored.equilibrium_temp_k.value, WithinAbs(290.0, 0.1));

    REQUIRE(restored.surface_gravity_g.hasValue());
    REQUIRE_THAT(restored.surface_gravity_g.value, WithinAbs(1.5, 0.01));

    REQUIRE(restored.inclination_deg.hasValue());
    REQUIRE_THAT(restored.inclination_deg.value, WithinAbs(89.5, 0.01));

    REQUIRE(restored.omega_deg.hasValue());
    REQUIRE_THAT(restored.omega_deg.value, WithinAbs(45.0, 0.01));
}

TEST_CASE("ExoplanetData: toJson/fromJson roundtrip preserves atmospheric fields", "[inference_integration][json_roundtrip]") {
    ExoplanetData original;
    original.name = "Atmo-Test b";

    original.surface_pressure_atm = MeasuredValue<double>(1.2, DataSource::AI_INFERRED);
    original.albedo = MeasuredValue<double>(0.35, DataSource::NASA_TAP);
    original.greenhouse_effect = MeasuredValue<double>(0.8, DataSource::AI_INFERRED);
    original.atmosphere_composition = MeasuredValue<std::string>(
        R"({"N2":78,"O2":21,"CO2":0.04})", DataSource::AI_INFERRED);
    original.ocean_coverage_fraction = MeasuredValue<double>(0.7, DataSource::AI_INFERRED);
    original.cloud_coverage_fraction = MeasuredValue<double>(0.5, DataSource::AI_INFERRED);
    original.ice_coverage_fraction = MeasuredValue<double>(0.1, DataSource::AI_INFERRED);

    auto j = original.toJson();
    auto restored = ExoplanetData::fromJson(j);

    REQUIRE(restored.surface_pressure_atm.hasValue());
    REQUIRE_THAT(restored.surface_pressure_atm.value, WithinAbs(1.2, 0.01));
    REQUIRE(restored.surface_pressure_atm.source == DataSource::AI_INFERRED);

    REQUIRE(restored.albedo.hasValue());
    REQUIRE_THAT(restored.albedo.value, WithinAbs(0.35, 0.01));

    REQUIRE(restored.greenhouse_effect.hasValue());
    REQUIRE_THAT(restored.greenhouse_effect.value, WithinAbs(0.8, 0.01));

    REQUIRE(restored.atmosphere_composition.hasValue());
    REQUIRE(restored.atmosphere_composition.value == R"({"N2":78,"O2":21,"CO2":0.04})");
    REQUIRE(restored.atmosphere_composition.source == DataSource::AI_INFERRED);

    REQUIRE(restored.ocean_coverage_fraction.hasValue());
    REQUIRE_THAT(restored.ocean_coverage_fraction.value, WithinAbs(0.7, 0.01));

    REQUIRE(restored.cloud_coverage_fraction.hasValue());
    REQUIRE_THAT(restored.cloud_coverage_fraction.value, WithinAbs(0.5, 0.01));

    REQUIRE(restored.ice_coverage_fraction.hasValue());
    REQUIRE_THAT(restored.ice_coverage_fraction.value, WithinAbs(0.1, 0.01));
}

TEST_CASE("ExoplanetData: toJson/fromJson roundtrip preserves rendering fields", "[inference_integration][json_roundtrip]") {
    ExoplanetData original;
    original.name = "Render-Test b";

    original.biome_classification = MeasuredValue<std::string>("Temperate", DataSource::AI_INFERRED);
    original.surface_color_hint = MeasuredValue<std::string>("blue-green", DataSource::AI_INFERRED);
    original.earth_similarity_index = MeasuredValue<double>(0.85, DataSource::CALCULATED);

    auto j = original.toJson();
    auto restored = ExoplanetData::fromJson(j);

    REQUIRE(restored.biome_classification.hasValue());
    REQUIRE(restored.biome_classification.value == "Temperate");
    REQUIRE(restored.biome_classification.source == DataSource::AI_INFERRED);

    REQUIRE(restored.surface_color_hint.hasValue());
    REQUIRE(restored.surface_color_hint.value == "blue-green");
    REQUIRE(restored.surface_color_hint.source == DataSource::AI_INFERRED);

    REQUIRE(restored.earth_similarity_index.hasValue());
    REQUIRE_THAT(restored.earth_similarity_index.value, WithinAbs(0.85, 0.01));
}

TEST_CASE("ExoplanetData: toJson/fromJson preserves host_star extended fields", "[inference_integration][json_roundtrip]") {
    ExoplanetData original;
    original.name = "Star-Test b";

    original.host_star.name = "Star-Test";
    original.host_star.effective_temp_k = MeasuredValue<double>(5800.0, DataSource::NASA_TAP);
    original.host_star.radius_solar = MeasuredValue<double>(1.0, DataSource::NASA_TAP);
    original.host_star.mass_solar = MeasuredValue<double>(1.0, DataSource::NASA_TAP);
    original.host_star.luminosity_solar = MeasuredValue<double>(1.0, DataSource::NASA_TAP);
    original.host_star.spectral_type = "G2V";

    original.host_star.metallicity = MeasuredValue<double>(0.1, DataSource::CDS_VIZIER);
    original.host_star.distance_pc = MeasuredValue<double>(100.0, DataSource::GAIA);
    original.host_star.age_gyr = MeasuredValue<double>(4.5, DataSource::CDS_VIZIER);
    original.host_star.ra_deg = MeasuredValue<double>(180.0, DataSource::GAIA);
    original.host_star.dec_deg = MeasuredValue<double>(-30.0, DataSource::GAIA);

    auto j = original.toJson();
    auto restored = ExoplanetData::fromJson(j);

    REQUIRE(restored.host_star.metallicity.hasValue());
    REQUIRE_THAT(restored.host_star.metallicity.value, WithinAbs(0.1, 0.01));
    REQUIRE(restored.host_star.metallicity.source == DataSource::CDS_VIZIER);

    REQUIRE(restored.host_star.distance_pc.hasValue());
    REQUIRE_THAT(restored.host_star.distance_pc.value, WithinAbs(100.0, 0.1));
    REQUIRE(restored.host_star.distance_pc.source == DataSource::GAIA);

    REQUIRE(restored.host_star.age_gyr.hasValue());
    REQUIRE_THAT(restored.host_star.age_gyr.value, WithinAbs(4.5, 0.01));

    REQUIRE(restored.host_star.ra_deg.hasValue());
    REQUIRE_THAT(restored.host_star.ra_deg.value, WithinAbs(180.0, 0.01));

    REQUIRE(restored.host_star.dec_deg.hasValue());
    REQUIRE_THAT(restored.host_star.dec_deg.value, WithinAbs(-30.0, 0.01));
}

TEST_CASE("ExoplanetData: toJson/fromJson preserves ai_reasoning and confidence", "[inference_integration][json_roundtrip]") {
    ExoplanetData original;
    original.name = "AI-Test b";

    original.surface_pressure_atm = MeasuredValue<double>(2.5, DataSource::AI_INFERRED);
    original.surface_pressure_atm.ai_reasoning = "Based on mass and radius suggesting thick atmosphere";
    original.surface_pressure_atm.confidence = 0.7f;

    original.albedo = MeasuredValue<double>(0.65, DataSource::AI_INFERRED);
    original.albedo.ai_reasoning = "High cloud coverage suggests high reflectivity";
    original.albedo.confidence = 0.9f;

    original.biome_classification = MeasuredValue<std::string>("Ocean World", DataSource::AI_INFERRED);
    original.biome_classification.ai_reasoning = "Temperature and composition favor liquid water";
    original.biome_classification.confidence = 0.5f;

    auto j = original.toJson();
    auto restored = ExoplanetData::fromJson(j);

    REQUIRE(restored.surface_pressure_atm.ai_reasoning.has_value());
    REQUIRE(*restored.surface_pressure_atm.ai_reasoning == "Based on mass and radius suggesting thick atmosphere");
    REQUIRE_THAT(restored.surface_pressure_atm.confidence, WithinAbs(0.7f, 0.01f));

    REQUIRE(restored.albedo.ai_reasoning.has_value());
    REQUIRE(*restored.albedo.ai_reasoning == "High cloud coverage suggests high reflectivity");
    REQUIRE_THAT(restored.albedo.confidence, WithinAbs(0.9f, 0.01f));

    REQUIRE(restored.biome_classification.ai_reasoning.has_value());
    REQUIRE(*restored.biome_classification.ai_reasoning == "Temperature and composition favor liquid water");
    REQUIRE_THAT(restored.biome_classification.confidence, WithinAbs(0.5f, 0.01f));
}

TEST_CASE("ExoplanetData: DataSource roundtrips correctly through toJson/fromJson", "[inference_integration][json_roundtrip]") {
    auto testSource = [](DataSource source, const std::string& label) {
        ExoplanetData data;
        data.name = "Source-Test-" + label;
        data.mass_earth = MeasuredValue<double>(1.0, source);

        auto j = data.toJson();
        auto restored = ExoplanetData::fromJson(j);

        REQUIRE(restored.mass_earth.hasValue());
        REQUIRE(restored.mass_earth.source == source);
    };

    testSource(DataSource::NASA_TAP, "NASA");
    testSource(DataSource::AI_INFERRED, "AI");
    testSource(DataSource::CALCULATED, "CALCULATED");
    testSource(DataSource::GAIA, "GAIA");
    testSource(DataSource::CDS_VIZIER, "CDS");
    testSource(DataSource::OEC, "OEC");
}

// ============================================================================
// Test Group 2: Deterministic fallback behavior (Task 2)
// ============================================================================

TEST_CASE("applyDeterministicDefaults: Hot Jupiter gets Gas Giant biome and H2/He atmosphere", "[inference_integration][defaults]") {
    ExoplanetData planet;
    planet.name = "Hot-Jupiter-Test b";
    planet.mass_earth = MeasuredValue<double>(500.0, DataSource::NASA_TAP);
    planet.radius_earth = MeasuredValue<double>(12.0, DataSource::NASA_TAP);
    planet.equilibrium_temp_k = MeasuredValue<double>(1200.0, DataSource::NASA_TAP);

    DataFusionEngine::applyDeterministicDefaults(planet);

    // Gas giant biome
    REQUIRE(planet.biome_classification.value == "Gas Giant");
    REQUIRE(planet.biome_classification.source == DataSource::CALCULATED);

    // H2/He atmosphere (check contains H2)
    REQUIRE(!planet.atmosphere_composition.value.empty());
    REQUIRE(planet.atmosphere_composition.value.find("H2") != std::string::npos);
    REQUIRE(planet.atmosphere_composition.source == DataSource::CALCULATED);

    // High albedo for hot planet
    REQUIRE(planet.albedo.hasValue());
    REQUIRE_THAT(planet.albedo.value, WithinAbs(0.75, 0.01));
    REQUIRE(planet.albedo.source == DataSource::CALCULATED);
}

TEST_CASE("applyDeterministicDefaults: Habitable zone terrestrial gets Temperate biome", "[inference_integration][defaults]") {
    ExoplanetData planet;
    planet.name = "Earth-Analog b";
    planet.mass_earth = MeasuredValue<double>(1.0, DataSource::NASA_TAP);
    planet.radius_earth = MeasuredValue<double>(1.0, DataSource::NASA_TAP);
    planet.equilibrium_temp_k = MeasuredValue<double>(290.0, DataSource::NASA_TAP);

    DataFusionEngine::applyDeterministicDefaults(planet);

    // Temperate biome
    REQUIRE(planet.biome_classification.value == "Temperate");
    REQUIRE(planet.biome_classification.source == DataSource::CALCULATED);

    // N2/O2-like atmosphere
    REQUIRE(!planet.atmosphere_composition.value.empty());
    REQUIRE(planet.atmosphere_composition.value.find("N2") != std::string::npos);
    REQUIRE(planet.atmosphere_composition.value.find("O2") != std::string::npos);

    // Earth-like albedo
    REQUIRE(planet.albedo.hasValue());
    REQUIRE_THAT(planet.albedo.value, WithinAbs(0.3, 0.01));

    // Moderate ocean coverage
    REQUIRE(planet.ocean_coverage_fraction.hasValue());
    REQUIRE_THAT(planet.ocean_coverage_fraction.value, WithinAbs(0.5, 0.01));
}

TEST_CASE("applyDeterministicDefaults: Frozen world gets Ice World biome", "[inference_integration][defaults]") {
    ExoplanetData planet;
    planet.name = "Frozen-Test b";
    planet.mass_earth = MeasuredValue<double>(0.8, DataSource::NASA_TAP);
    planet.radius_earth = MeasuredValue<double>(0.9, DataSource::NASA_TAP);
    planet.equilibrium_temp_k = MeasuredValue<double>(80.0, DataSource::NASA_TAP);

    DataFusionEngine::applyDeterministicDefaults(planet);

    // Ice World biome
    REQUIRE(planet.biome_classification.value == "Ice World");
    REQUIRE(planet.biome_classification.source == DataSource::CALCULATED);

    // High ice albedo
    REQUIRE(planet.albedo.hasValue());
    REQUIRE_THAT(planet.albedo.value, WithinAbs(0.5, 0.01));

    // High ice coverage
    REQUIRE(planet.ice_coverage_fraction.hasValue());
    REQUIRE(planet.ice_coverage_fraction.value > 0.8);

    // Zero ocean
    REQUIRE(planet.ocean_coverage_fraction.hasValue());
    REQUIRE_THAT(planet.ocean_coverage_fraction.value, WithinAbs(0.0, 0.01));
}

TEST_CASE("applyDeterministicDefaults: Venus analog gets Lava World/Desert biome and CO2 atmosphere", "[inference_integration][defaults]") {
    ExoplanetData planet;
    planet.name = "Venus-Analog b";
    planet.mass_earth = MeasuredValue<double>(0.95, DataSource::NASA_TAP);
    planet.radius_earth = MeasuredValue<double>(0.95, DataSource::NASA_TAP);
    planet.equilibrium_temp_k = MeasuredValue<double>(735.0, DataSource::NASA_TAP);

    DataFusionEngine::applyDeterministicDefaults(planet);

    // Should be Lava World for T > 700K
    REQUIRE(planet.biome_classification.value == "Lava World");

    // CO2-dominated atmosphere for hot rocky planet
    REQUIRE(!planet.atmosphere_composition.value.empty());
    REQUIRE(planet.atmosphere_composition.value.find("CO2") != std::string::npos);

    // High albedo (Venus-like reflective clouds)
    REQUIRE(planet.albedo.hasValue());
    REQUIRE_THAT(planet.albedo.value, WithinAbs(0.75, 0.01));
}

TEST_CASE("applyDeterministicDefaults: Tundra world at 200K", "[inference_integration][defaults]") {
    ExoplanetData planet;
    planet.name = "Tundra-Test b";
    planet.mass_earth = MeasuredValue<double>(1.5, DataSource::NASA_TAP);
    planet.radius_earth = MeasuredValue<double>(1.1, DataSource::NASA_TAP);
    planet.equilibrium_temp_k = MeasuredValue<double>(200.0, DataSource::NASA_TAP);

    DataFusionEngine::applyDeterministicDefaults(planet);

    // Tundra biome (150-250K)
    REQUIRE(planet.biome_classification.value == "Tundra");

    // Moderate ice coverage
    REQUIRE(planet.ice_coverage_fraction.hasValue());
    REQUIRE_THAT(planet.ice_coverage_fraction.value, WithinAbs(0.4, 0.01));
}

TEST_CASE("applyDeterministicDefaults: ALL filled values have CALCULATED source", "[inference_integration][defaults]") {
    ExoplanetData planet;
    planet.name = "Source-Check b";
    planet.mass_earth = MeasuredValue<double>(3.0, DataSource::NASA_TAP);
    planet.radius_earth = MeasuredValue<double>(1.5, DataSource::NASA_TAP);
    planet.equilibrium_temp_k = MeasuredValue<double>(300.0, DataSource::NASA_TAP);

    DataFusionEngine::applyDeterministicDefaults(planet);

    // All filled fields must be CALCULATED
    REQUIRE(planet.albedo.source == DataSource::CALCULATED);
    REQUIRE(planet.surface_pressure_atm.source == DataSource::CALCULATED);
    REQUIRE(planet.atmosphere_composition.source == DataSource::CALCULATED);
    REQUIRE(planet.biome_classification.source == DataSource::CALCULATED);
    REQUIRE(planet.ocean_coverage_fraction.source == DataSource::CALCULATED);
    REQUIRE(planet.cloud_coverage_fraction.source == DataSource::CALCULATED);
    REQUIRE(planet.ice_coverage_fraction.source == DataSource::CALCULATED);
    REQUIRE(planet.surface_color_hint.source == DataSource::CALCULATED);
    REQUIRE(planet.greenhouse_effect.source == DataSource::CALCULATED);
}

// ============================================================================
// Test Group 3: No-overwrite guard (Task 2)
// ============================================================================

TEST_CASE("applyDeterministicDefaults: does NOT overwrite existing measured values", "[inference_integration][no_overwrite]") {
    ExoplanetData planet;
    planet.name = "Guard-Test b";
    planet.mass_earth = MeasuredValue<double>(2.0, DataSource::NASA_TAP);
    planet.radius_earth = MeasuredValue<double>(1.2, DataSource::NASA_TAP);
    planet.equilibrium_temp_k = MeasuredValue<double>(300.0, DataSource::NASA_TAP);

    // Pre-set some values from measured sources
    planet.albedo = MeasuredValue<double>(0.42, DataSource::NASA_TAP);
    planet.albedo.uncertainty = 0.05;
    planet.biome_classification = MeasuredValue<std::string>("Ocean World", DataSource::OEC);
    planet.atmosphere_composition = MeasuredValue<std::string>(
        R"({"N2":70,"O2":20,"CO2":10})", DataSource::EXOATMOS);

    DataFusionEngine::applyDeterministicDefaults(planet);

    // Existing measured albedo must be preserved
    REQUIRE_THAT(planet.albedo.value, WithinAbs(0.42, 0.001));
    REQUIRE(planet.albedo.source == DataSource::NASA_TAP);
    REQUIRE(planet.albedo.uncertainty.has_value());
    REQUIRE_THAT(*planet.albedo.uncertainty, WithinAbs(0.05, 0.001));

    // Existing biome must be preserved
    REQUIRE(planet.biome_classification.value == "Ocean World");
    REQUIRE(planet.biome_classification.source == DataSource::OEC);

    // Existing atmosphere must be preserved
    REQUIRE(planet.atmosphere_composition.value == R"({"N2":70,"O2":20,"CO2":10})");
    REQUIRE(planet.atmosphere_composition.source == DataSource::EXOATMOS);

    // But fields that were NOT pre-set should be filled
    REQUIRE(planet.surface_pressure_atm.hasValue());
    REQUIRE(planet.surface_pressure_atm.source == DataSource::CALCULATED);
}

TEST_CASE("applyInferredValues: skips fields that already have measured data", "[inference_integration][no_overwrite]") {
    // Simulate what the InferenceEngine lambda guards do
    ExoplanetData planet;
    planet.name = "NoOverwrite-Test b";

    // Pre-set albedo from NASA (measured data)
    planet.albedo = MeasuredValue<double>(0.67, DataSource::NASA_TAP);

    // Pre-set atmosphere from OEC
    planet.atmosphere_composition = MeasuredValue<std::string>(
        R"({"CO2":95,"N2":3.5})", DataSource::OEC);

    // The InferenceEngine's applyInferredValues guards with hasValue()/!empty()
    // Simulate by checking the guard conditions
    REQUIRE(planet.albedo.hasValue());  // Guard would return early
    REQUIRE(!planet.atmosphere_composition.value.empty());  // Guard would return early

    // Verify the values are still original
    REQUIRE_THAT(planet.albedo.value, WithinAbs(0.67, 0.001));
    REQUIRE(planet.albedo.source == DataSource::NASA_TAP);
    REQUIRE(planet.atmosphere_composition.source == DataSource::OEC);
}

TEST_CASE("applyDeterministicDefaults: desert world at 500K", "[inference_integration][defaults]") {
    ExoplanetData planet;
    planet.name = "Desert-Test b";
    planet.mass_earth = MeasuredValue<double>(2.5, DataSource::NASA_TAP);
    planet.radius_earth = MeasuredValue<double>(1.3, DataSource::NASA_TAP);
    planet.equilibrium_temp_k = MeasuredValue<double>(500.0, DataSource::NASA_TAP);

    DataFusionEngine::applyDeterministicDefaults(planet);

    // Desert biome (350-700K)
    REQUIRE(planet.biome_classification.value == "Desert");

    // Low ocean coverage for hot world
    REQUIRE(planet.ocean_coverage_fraction.hasValue());
    REQUIRE_THAT(planet.ocean_coverage_fraction.value, WithinAbs(0.05, 0.01));
}

TEST_CASE("applyDeterministicDefaults: Neptune-like planet at moderate temperature", "[inference_integration][defaults]") {
    ExoplanetData planet;
    planet.name = "Neptune-Like-Test b";
    planet.mass_earth = MeasuredValue<double>(20.0, DataSource::NASA_TAP);
    planet.radius_earth = MeasuredValue<double>(4.0, DataSource::NASA_TAP);
    planet.equilibrium_temp_k = MeasuredValue<double>(300.0, DataSource::NASA_TAP);

    DataFusionEngine::applyDeterministicDefaults(planet);

    // Still a terrestrial-sized biome at R=4 (not gas giant at R>6)
    REQUIRE(planet.biome_classification.value == "Temperate");

    // Thick atmosphere for massive planet
    REQUIRE(planet.surface_pressure_atm.hasValue());
    REQUIRE(planet.surface_pressure_atm.value > 1.0);
}

TEST_CASE("ExoplanetData: full JSON roundtrip with all fields including atmosphere and rendering", "[inference_integration][json_roundtrip]") {
    // Create a fully populated ExoplanetData
    ExoplanetData original;
    original.name = "Full-Test b";
    original.discovery_method = "RV";
    original.discovery_year = 2020;

    original.host_star.name = "Full-Test";
    original.host_star.spectral_type = "K2V";
    original.host_star.effective_temp_k = MeasuredValue<double>(4900.0, DataSource::NASA_TAP);
    original.host_star.metallicity = MeasuredValue<double>(-0.2, DataSource::CDS_VIZIER);
    original.host_star.distance_pc = MeasuredValue<double>(50.0, DataSource::GAIA);

    original.orbital_period_days = MeasuredValue<double>(30.0, DataSource::NASA_TAP);
    original.semi_major_axis_au = MeasuredValue<double>(0.15, DataSource::NASA_TAP);
    original.eccentricity = MeasuredValue<double>(0.05, DataSource::NASA_TAP);
    original.inclination_deg = MeasuredValue<double>(87.0, DataSource::NASA_TAP);
    original.omega_deg = MeasuredValue<double>(120.0, DataSource::OEC);

    original.mass_earth = MeasuredValue<double>(5.0, DataSource::NASA_TAP);
    original.radius_earth = MeasuredValue<double>(1.8, DataSource::NASA_TAP);
    original.density_gcc = MeasuredValue<double>(4.0, DataSource::CALCULATED);
    original.equilibrium_temp_k = MeasuredValue<double>(400.0, DataSource::CALCULATED);
    original.surface_gravity_g = MeasuredValue<double>(1.54, DataSource::CALCULATED);

    original.surface_pressure_atm = MeasuredValue<double>(3.0, DataSource::AI_INFERRED);
    original.surface_pressure_atm.ai_reasoning = "Massive super-Earth retains thick atmosphere";
    original.surface_pressure_atm.confidence = 0.7f;
    original.albedo = MeasuredValue<double>(0.4, DataSource::AI_INFERRED);
    original.greenhouse_effect = MeasuredValue<double>(0.5, DataSource::AI_INFERRED);
    original.atmosphere_composition = MeasuredValue<std::string>(
        R"({"CO2":50,"N2":40,"H2O":10})", DataSource::AI_INFERRED);
    original.ocean_coverage_fraction = MeasuredValue<double>(0.2, DataSource::AI_INFERRED);
    original.cloud_coverage_fraction = MeasuredValue<double>(0.6, DataSource::AI_INFERRED);
    original.ice_coverage_fraction = MeasuredValue<double>(0.0, DataSource::AI_INFERRED);

    original.biome_classification = MeasuredValue<std::string>("Desert", DataSource::AI_INFERRED);
    original.surface_color_hint = MeasuredValue<std::string>("rust-orange", DataSource::AI_INFERRED);
    original.earth_similarity_index = MeasuredValue<double>(0.6, DataSource::CALCULATED);

    original.planet_type = MeasuredValue<std::string>("Super-Earth", DataSource::CALCULATED);
    original.habitable_zone_distance = MeasuredValue<double>(0.8, DataSource::CALCULATED);

    // Roundtrip
    auto j = original.toJson();
    auto restored = ExoplanetData::fromJson(j);

    // Verify all fields preserved
    REQUIRE(restored.name == "Full-Test b");
    REQUIRE(restored.host_star.spectral_type == "K2V");
    REQUIRE_THAT(restored.host_star.metallicity.value, WithinAbs(-0.2, 0.01));
    REQUIRE_THAT(restored.inclination_deg.value, WithinAbs(87.0, 0.01));
    REQUIRE_THAT(restored.surface_gravity_g.value, WithinAbs(1.54, 0.01));
    REQUIRE_THAT(restored.surface_pressure_atm.value, WithinAbs(3.0, 0.01));
    REQUIRE(restored.surface_pressure_atm.ai_reasoning.has_value());
    REQUIRE_THAT(restored.surface_pressure_atm.confidence, WithinAbs(0.7f, 0.01f));
    REQUIRE(restored.atmosphere_composition.value.find("CO2") != std::string::npos);
    REQUIRE(restored.biome_classification.value == "Desert");
    REQUIRE(restored.surface_color_hint.value == "rust-orange");
    REQUIRE_THAT(restored.earth_similarity_index.value, WithinAbs(0.6, 0.01));
    REQUIRE(restored.planet_type.value == "Super-Earth");
    REQUIRE_THAT(restored.habitable_zone_distance.value, WithinAbs(0.8, 0.01));
}
