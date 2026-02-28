#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "data/OecClient.hpp"
#include "core/Logger.hpp"

using namespace astrocore;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

// Initialize logger once for all tests
struct LoggerInitializer {
    LoggerInitializer() {
        Logger::init();
    }
};
static LoggerInitializer s_logInit;

TEST_CASE("OEC parser handles simple single-star system", "[oec_parser]") {
    std::string xml = R"(
        <system>
            <name>Test System</name>
            <rightascension>19 01 27</rightascension>
            <declination>+50 13 16</declination>
            <distance>351.0</distance>
            <star>
                <name>Test Star</name>
                <mass>0.98</mass>
                <radius>0.87</radius>
                <temperature>5324</temperature>
                <planet>
                    <name>Test System b</name>
                    <mass>0.13</mass>
                    <radius>0.11</radius>
                    <period>112.3</period>
                    <semimajoraxis>0.409</semimajoraxis>
                    <eccentricity>0.04</eccentricity>
                    <temperature>233</temperature>
                    <discoverymethod>transit</discoverymethod>
                    <discoveryyear>2015</discoveryyear>
                </planet>
            </star>
        </system>
    )";

    auto planets = OecClient::parseSystemXml(xml);

    REQUIRE(planets.size() == 1);

    const auto& planet = planets[0];
    CHECK(planet.name == "Test System b");

    // Check Jupiter to Earth mass conversion (0.13 * 317.8)
    REQUIRE(planet.mass_earth.hasValue());
    CHECK_THAT(planet.mass_earth.value, WithinAbs(0.13 * 317.8, 0.1));
    CHECK(planet.mass_earth.source == DataSource::OEC);

    // Check Jupiter to Earth radius conversion (0.11 * 11.2)
    REQUIRE(planet.radius_earth.hasValue());
    CHECK_THAT(planet.radius_earth.value, WithinAbs(0.11 * 11.2, 0.01));
    CHECK(planet.radius_earth.source == DataSource::OEC);

    // Check orbital parameters
    REQUIRE(planet.orbital_period_days.hasValue());
    CHECK_THAT(planet.orbital_period_days.value, WithinAbs(112.3, 0.1));
    CHECK(planet.orbital_period_days.source == DataSource::OEC);

    REQUIRE(planet.semi_major_axis_au.hasValue());
    CHECK_THAT(planet.semi_major_axis_au.value, WithinAbs(0.409, 0.001));

    REQUIRE(planet.eccentricity.hasValue());
    CHECK_THAT(planet.eccentricity.value, WithinAbs(0.04, 0.01));

    // Check temperature
    REQUIRE(planet.equilibrium_temp_k.hasValue());
    CHECK_THAT(planet.equilibrium_temp_k.value, WithinAbs(233.0, 1.0));

    // Check discovery info
    CHECK(planet.discovery_method == "transit");
    CHECK(planet.discovery_year == 2015);

    // Check host star
    CHECK(planet.host_star.name == "Test Star");
    REQUIRE(planet.host_star.mass_solar.hasValue());
    CHECK_THAT(planet.host_star.mass_solar.value, WithinAbs(0.98, 0.01));
    CHECK(planet.host_star.mass_solar.source == DataSource::OEC);

    REQUIRE(planet.host_star.radius_solar.hasValue());
    CHECK_THAT(planet.host_star.radius_solar.value, WithinAbs(0.87, 0.01));

    REQUIRE(planet.host_star.effective_temp_k.hasValue());
    CHECK_THAT(planet.host_star.effective_temp_k.value, WithinAbs(5324.0, 1.0));

    // Check system-level data
    REQUIRE(planet.host_star.distance_pc.hasValue());
    CHECK_THAT(planet.host_star.distance_pc.value, WithinAbs(351.0, 1.0));
    CHECK(planet.host_star.distance_pc.source == DataSource::OEC);
}

TEST_CASE("OEC parser handles binary system hierarchy", "[oec_parser]") {
    std::string xml = R"(
        <system>
            <name>Binary System</name>
            <rightascension>12 00 00</rightascension>
            <declination>-30 00 00</declination>
            <binary>
                <star>
                    <name>Star A</name>
                    <mass>1.1</mass>
                    <planet>
                        <name>Binary System AB b</name>
                        <mass>0.5</mass>
                        <period>7.5</period>
                    </planet>
                </star>
                <star>
                    <name>Star B</name>
                    <mass>0.9</mass>
                </star>
            </binary>
        </system>
    )";

    auto planets = OecClient::parseSystemXml(xml);

    REQUIRE(planets.size() == 1);

    const auto& planet = planets[0];
    CHECK(planet.name == "Binary System AB b");
    CHECK(planet.host_star.name == "Star A");

    REQUIRE(planet.mass_earth.hasValue());
    CHECK_THAT(planet.mass_earth.value, WithinAbs(0.5 * 317.8, 1.0));
    CHECK(planet.mass_earth.source == DataSource::OEC);

    REQUIRE(planet.orbital_period_days.hasValue());
    CHECK_THAT(planet.orbital_period_days.value, WithinAbs(7.5, 0.1));
}

TEST_CASE("OEC parser handles missing fields gracefully", "[oec_parser]") {
    std::string xml = R"(
        <system>
            <name>Sparse System</name>
            <star>
                <name>Sparse Star</name>
                <planet>
                    <name>Sparse Planet b</name>
                    <period>365.25</period>
                </planet>
            </star>
        </system>
    )";

    auto planets = OecClient::parseSystemXml(xml);

    REQUIRE(planets.size() == 1);

    const auto& planet = planets[0];
    CHECK(planet.name == "Sparse Planet b");

    // Planet should have period but not mass/radius/temperature
    REQUIRE(planet.orbital_period_days.hasValue());
    CHECK_THAT(planet.orbital_period_days.value, WithinAbs(365.25, 0.1));

    // Missing fields should not have values
    CHECK_FALSE(planet.mass_earth.hasValue());
    CHECK_FALSE(planet.radius_earth.hasValue());
    CHECK_FALSE(planet.equilibrium_temp_k.hasValue());
}

TEST_CASE("OEC parser handles uncertainty attributes", "[oec_parser]") {
    std::string xml = R"(
        <system>
            <name>Uncertain System</name>
            <star>
                <name>Uncertain Star</name>
                <planet>
                    <name>Uncertain Planet b</name>
                    <mass errorminus="0.01" errorplus="0.02">0.13</mass>
                    <radius errorminus="0.005" errorplus="0.01">0.11</radius>
                </planet>
            </star>
        </system>
    )";

    auto planets = OecClient::parseSystemXml(xml);

    REQUIRE(planets.size() == 1);

    const auto& planet = planets[0];

    // Check mass uncertainty (converted from Jupiter to Earth units)
    REQUIRE(planet.mass_earth.hasValue());
    CHECK_THAT(planet.mass_earth.value, WithinAbs(0.13 * 317.8, 0.1));
    REQUIRE(planet.mass_earth.uncertainty.has_value());
    CHECK_THAT(*planet.mass_earth.uncertainty, WithinAbs(0.01 * 317.8, 0.1));

    // Check radius uncertainty (converted from Jupiter to Earth units)
    REQUIRE(planet.radius_earth.hasValue());
    CHECK_THAT(planet.radius_earth.value, WithinAbs(0.11 * 11.2, 0.01));
    REQUIRE(planet.radius_earth.uncertainty.has_value());
    CHECK_THAT(*planet.radius_earth.uncertainty, WithinAbs(0.005 * 11.2, 0.01));
}

TEST_CASE("OEC RA/Dec conversion from HMS/DMS to degrees", "[oec_parser]") {
    std::string xml = R"(
        <system>
            <name>Coordinate Test System</name>
            <rightascension>19 01 27</rightascension>
            <declination>+50 13 16</declination>
            <star>
                <name>Coordinate Test Star</name>
                <planet>
                    <name>Coordinate Test b</name>
                    <period>100</period>
                </planet>
            </star>
        </system>
    )";

    auto planets = OecClient::parseSystemXml(xml);

    REQUIRE(planets.size() == 1);

    const auto& planet = planets[0];

    // RA "19 01 27" = (19 + 1/60 + 27/3600) * 15
    // = (19 + 0.01667 + 0.0075) * 15 = 19.02417 * 15 = 285.3625 degrees
    REQUIRE(planet.host_star.ra_deg.hasValue());
    CHECK_THAT(planet.host_star.ra_deg.value, WithinAbs(285.3625, 0.1));
    CHECK(planet.host_star.ra_deg.source == DataSource::OEC);

    // Dec "+50 13 16" = 50 + 13/60 + 16/3600
    // = 50 + 0.2167 + 0.0044 = 50.2211 degrees
    REQUIRE(planet.host_star.dec_deg.hasValue());
    CHECK_THAT(planet.host_star.dec_deg.value, WithinAbs(50.2211, 0.01));
    CHECK(planet.host_star.dec_deg.source == DataSource::OEC);
}
