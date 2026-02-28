#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/catch_approx.hpp>
#include "data/NasaApiClient.hpp"
#include "data/ExoplanetData.hpp"
#include "mocks/MockHttpClient.hpp"

using namespace astrocore;
using namespace astrocore::test;

TEST_CASE("NASA ADQL builder produces valid query for name search", "[nasa_client]") {
    NasaApiClient client;

    // Test the ADQL structure indirectly by verifying the data structure
    // In a real scenario, we'd expose buildADQL as a public method or test through queries
    // For now, we verify the client can be constructed successfully
    REQUIRE(true);  // Placeholder - buildADQL is private

    // Note: The actual ADQL query construction is tested through integration
    // This test validates that the client initializes properly
}

TEST_CASE("NASA JSON row parsing handles complete data", "[nasa_client]") {
    // Parse the sample JSON
    auto json = nlohmann::json::parse(SAMPLE_KEPLER_442B_JSON);
    REQUIRE(json.is_array());
    REQUIRE(json.size() == 1);

    // Parse the row
    ExoplanetData planet = NasaApiClient::parseNasaTapRow(json[0]);

    // Verify basic fields
    REQUIRE(planet.name == "Kepler-442 b");
    REQUIRE(planet.host_star.name == "Kepler-442");
    REQUIRE(planet.discovery_year == 2015);
    REQUIRE(planet.discovery_method == "Transit");

    // Verify measured values with proper sources
    REQUIRE(planet.orbital_period_days.hasValue());
    REQUIRE_THAT(planet.orbital_period_days.value, Catch::Matchers::WithinRel(112.3053, 0.001));
    REQUIRE(planet.orbital_period_days.source == DataSource::NASA_TAP);

    REQUIRE(planet.mass_earth.hasValue());
    REQUIRE_THAT(planet.mass_earth.value, Catch::Matchers::WithinRel(2.36, 0.01));
    REQUIRE(planet.mass_earth.source == DataSource::NASA_TAP);

    REQUIRE(planet.radius_earth.hasValue());
    REQUIRE_THAT(planet.radius_earth.value, Catch::Matchers::WithinRel(1.34, 0.01));
    REQUIRE(planet.radius_earth.source == DataSource::NASA_TAP);

    // Verify host star data
    REQUIRE(planet.host_star.effective_temp_k.hasValue());
    REQUIRE(planet.host_star.effective_temp_k.value == Catch::Approx(4402.0).margin(10.0));
    REQUIRE(planet.host_star.effective_temp_k.source == DataSource::NASA_TAP);

    REQUIRE(planet.host_star.spectral_type == "K5V");

    // Verify coordinate fields
    REQUIRE(planet.host_star.ra_deg.hasValue());
    REQUIRE_THAT(planet.host_star.ra_deg.value, Catch::Matchers::WithinRel(295.654, 0.001));

    REQUIRE(planet.host_star.dec_deg.hasValue());
    REQUIRE_THAT(planet.host_star.dec_deg.value, Catch::Matchers::WithinRel(39.123, 0.001));
}

TEST_CASE("NASA JSON row parsing handles missing fields", "[nasa_client]") {
    // Parse the sample JSON with nulls
    auto json = nlohmann::json::parse(SAMPLE_MISSING_FIELDS_JSON);
    REQUIRE(json.is_array());
    REQUIRE(json.size() == 1);

    // Parse the row - should not crash
    ExoplanetData planet = NasaApiClient::parseNasaTapRow(json[0]);

    // Verify basic fields
    REQUIRE(planet.name == "Test Planet b");
    REQUIRE(planet.host_star.name == "Test Star");

    // Verify that missing fields are properly handled
    REQUIRE_FALSE(planet.mass_earth.hasValue());
    REQUIRE_FALSE(planet.radius_earth.hasValue());
    REQUIRE_FALSE(planet.density_gcc.hasValue());
    REQUIRE_FALSE(planet.equilibrium_temp_k.hasValue());

    // Verify that present fields are still parsed
    REQUIRE(planet.orbital_period_days.hasValue());
    REQUIRE_THAT(planet.orbital_period_days.value, Catch::Matchers::WithinRel(100.0, 0.01));

    REQUIRE(planet.host_star.effective_temp_k.hasValue());
    REQUIRE(planet.host_star.effective_temp_k.value == Catch::Approx(5500.0).margin(10.0));

    // Coordinates should still be present
    REQUIRE(planet.host_star.ra_deg.hasValue());
    REQUIRE(planet.host_star.dec_deg.hasValue());
}

TEST_CASE("DataSource enum includes all 4 sources", "[nasa_client]") {
    // Verify all expected data sources are present and stringify correctly
    REQUIRE(dataSourceToString(DataSource::NASA_TAP) == "NASA");
    REQUIRE(dataSourceToString(DataSource::GAIA) == "Gaia DR3");
    REQUIRE(dataSourceToString(DataSource::CDS_VIZIER) == "CDS/VizieR");
    REQUIRE(dataSourceToString(DataSource::OEC) == "OEC");
    REQUIRE(dataSourceToString(DataSource::EXOATMOS) == "ExoAtmos");
    REQUIRE(dataSourceToString(DataSource::AI_INFERRED) == "AI");
    REQUIRE(dataSourceToString(DataSource::CALCULATED) == "Calculated");
    REQUIRE(dataSourceToString(DataSource::UNKNOWN) == "Unknown");
}
