#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "data/GaiaClient.hpp"
#include <nlohmann/json.hpp>

using namespace astrocore;

TEST_CASE("Gaia ADQL coordinate query has correct structure", "[gaia_client]") {
    std::string query = GaiaClient::buildCoordQuery(285.36, 50.22, 5.0);

    REQUIRE(query.find("gaiadr3") != std::string::npos);
    REQUIRE(query.find("CIRCLE") != std::string::npos);
    REQUIRE(query.find("ICRS") != std::string::npos);
    REQUIRE(query.find("285.36") != std::string::npos);
    REQUIRE(query.find("50.22") != std::string::npos);
    REQUIRE(query.find("astrophysical_parameters") != std::string::npos);
}

TEST_CASE("Gaia JSON row parsing extracts stellar parameters", "[gaia_client]") {
    // Mock Gaia TAP response row (array format)
    nlohmann::json row = nlohmann::json::array({
        12345,           // source_id (0)
        285.36,          // ra (1)
        50.22,           // dec (2)
        2.85,            // parallax (3)
        0.05,            // parallax_error (4)
        9.5,             // phot_g_mean_mag (5)
        0.85,            // bp_rp (6)
        5324,            // teff_gspphot (7)
        5200,            // teff_gspphot_lower (8)
        5448,            // teff_gspphot_upper (9)
        4.45,            // logg_gspphot (10)
        4.40,            // logg_gspphot_lower (11)
        4.50,            // logg_gspphot_upper (12)
        -0.12,           // mh_gspphot (13)
        -0.15,           // mh_gspphot_lower (14)
        -0.09,           // mh_gspphot_upper (15)
        351.0,           // distance_gspphot (16)
        340.0,           // distance_gspphot_lower (17)
        362.0,           // distance_gspphot_upper (18)
        0.87,            // radius_flame (19)
        0.85,            // radius_flame_lower (20)
        0.89,            // radius_flame_upper (21)
        0.68,            // lum_flame (22)
        0.65,            // lum_flame_lower (23)
        0.71,            // lum_flame_upper (24)
        6.2,             // age_flame (25)
        5.8,             // age_flame_lower (26)
        6.6,             // age_flame_upper (27)
        0.98,            // mass_flame (28)
        0.95,            // mass_flame_lower (29)
        1.01             // mass_flame_upper (30)
    });

    HostStarData star = GaiaClient::parseGaiaRow(row);

    // Check basic parsing
    REQUIRE(star.effective_temp_k.hasValue());
    REQUIRE(star.effective_temp_k.value == 5324);
    REQUIRE(star.effective_temp_k.source == DataSource::GAIA);
    REQUIRE(star.effective_temp_k.uncertainty.has_value());
    REQUIRE_THAT(star.effective_temp_k.uncertainty.value(), Catch::Matchers::WithinAbs(124.0, 1.0));

    REQUIRE(star.distance_pc.hasValue());
    REQUIRE(star.distance_pc.value == 351.0);
    REQUIRE(star.distance_pc.source == DataSource::GAIA);

    REQUIRE(star.luminosity_solar.hasValue());
    REQUIRE_THAT(star.luminosity_solar.value, Catch::Matchers::WithinAbs(0.68, 0.01));
    REQUIRE(star.luminosity_solar.source == DataSource::GAIA);

    REQUIRE(star.metallicity.hasValue());
    REQUIRE_THAT(star.metallicity.value, Catch::Matchers::WithinAbs(-0.12, 0.01));
    REQUIRE(star.metallicity.source == DataSource::GAIA);

    REQUIRE(star.radius_solar.hasValue());
    REQUIRE_THAT(star.radius_solar.value, Catch::Matchers::WithinAbs(0.87, 0.01));
    REQUIRE(star.radius_solar.source == DataSource::GAIA);

    REQUIRE(star.mass_solar.hasValue());
    REQUIRE_THAT(star.mass_solar.value, Catch::Matchers::WithinAbs(0.98, 0.01));
    REQUIRE(star.mass_solar.source == DataSource::GAIA);

    REQUIRE(star.age_gyr.hasValue());
    REQUIRE_THAT(star.age_gyr.value, Catch::Matchers::WithinAbs(6.2, 0.1));
    REQUIRE(star.age_gyr.source == DataSource::GAIA);

    REQUIRE(star.ra_deg.hasValue());
    REQUIRE_THAT(star.ra_deg.value, Catch::Matchers::WithinAbs(285.36, 0.01));

    REQUIRE(star.dec_deg.hasValue());
    REQUIRE_THAT(star.dec_deg.value, Catch::Matchers::WithinAbs(50.22, 0.01));
}

TEST_CASE("Gaia row parsing handles null fields", "[gaia_client]") {
    // Row with mostly null values
    nlohmann::json row = nlohmann::json::array({
        12345,           // source_id (0)
        285.36,          // ra (1)
        50.22,           // dec (2)
        nullptr,         // parallax (3)
        nullptr,         // parallax_error (4)
        9.5,             // phot_g_mean_mag (5)
        nullptr,         // bp_rp (6)
        nullptr,         // teff_gspphot (7)
        nullptr,         // teff_gspphot_lower (8)
        nullptr,         // teff_gspphot_upper (9)
        nullptr,         // logg_gspphot (10)
        nullptr,         // logg_gspphot_lower (11)
        nullptr,         // logg_gspphot_upper (12)
        nullptr,         // mh_gspphot (13)
        nullptr,         // mh_gspphot_lower (14)
        nullptr,         // mh_gspphot_upper (15)
        nullptr,         // distance_gspphot (16)
        nullptr,         // distance_gspphot_lower (17)
        nullptr,         // distance_gspphot_upper (18)
        nullptr,         // radius_flame (19)
        nullptr,         // radius_flame_lower (20)
        nullptr,         // radius_flame_upper (21)
        nullptr,         // lum_flame (22)
        nullptr,         // lum_flame_lower (23)
        nullptr,         // lum_flame_upper (24)
        nullptr,         // age_flame (25)
        nullptr,         // age_flame_lower (26)
        nullptr,         // age_flame_upper (27)
        nullptr,         // mass_flame (28)
        nullptr,         // mass_flame_lower (29)
        nullptr          // mass_flame_upper (30)
    });

    HostStarData star = GaiaClient::parseGaiaRow(row);

    // Should not crash
    REQUIRE(!star.effective_temp_k.hasValue());
    REQUIRE(!star.distance_pc.hasValue());
    REQUIRE(!star.luminosity_solar.hasValue());
    REQUIRE(!star.metallicity.hasValue());

    // Coordinates should still parse
    REQUIRE(star.ra_deg.hasValue());
    REQUIRE(star.dec_deg.hasValue());
}

TEST_CASE("Gaia radius conversion uses 5 arcsec default", "[gaia_client]") {
    std::string query = GaiaClient::buildCoordQuery(100.0, 45.0, 5.0);

    // 5 arcsec = 5/3600 degrees = 0.001388889...
    REQUIRE(query.find("CIRCLE") != std::string::npos);

    // Check that the radius is approximately 0.00138889
    std::string radius_str = query.substr(query.find_last_of(',') + 1);
    radius_str = radius_str.substr(0, radius_str.find(')'));

    double radius = std::stod(radius_str);
    REQUIRE_THAT(radius, Catch::Matchers::WithinAbs(0.001388889, 0.0000001));
}
