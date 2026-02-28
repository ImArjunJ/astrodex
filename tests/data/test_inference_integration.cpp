#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "data/ExoplanetData.hpp"

using namespace astrocore;
using Catch::Matchers::WithinAbs;

// ============================================================================
// Test Group 1: ExoplanetData JSON serialization completeness
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

    // These fields must be preserved through serialization
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

    // Atmospheric fields
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

    // These fields are currently missing from serialization
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

    // Set AI-inferred values with reasoning and non-default confidence
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

    // Check ai_reasoning roundtrips
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
    // Verify ALL DataSource enum values survive serialization
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
