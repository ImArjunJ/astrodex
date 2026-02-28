#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "data/CdsClient.hpp"
#include <nlohmann/json.hpp>

using namespace astrocore;

TEST_CASE("SIMBAD ADQL name query has correct structure", "[cds_client]") {
    std::string query = CdsClient::buildSimbadNameQuery("Kepler-442");

    REQUIRE(query.find("basic") != std::string::npos);
    REQUIRE(query.find("ident") != std::string::npos);
    REQUIRE(query.find("Kepler-442") != std::string::npos);
    REQUIRE(query.find("main_id") != std::string::npos);
    REQUIRE(query.find("ra") != std::string::npos);
    REQUIRE(query.find("dec") != std::string::npos);
    REQUIRE(query.find("sp_type") != std::string::npos);
}

TEST_CASE("SIMBAD JSON row parsing extracts coordinates", "[cds_client]") {
    // Object format (SIMBAD TAP typical response)
    nlohmann::json row = {
        {"main_id", "Kepler-442"},
        {"ra", 285.36},
        {"dec", 50.22},
        {"sp_type", "K4V"}
    };

    auto star = CdsClient::parseSimbadRow(row);

    REQUIRE(star.found == true);
    REQUIRE(star.canonical_name == "Kepler-442");
    REQUIRE_THAT(star.ra_deg, Catch::Matchers::WithinAbs(285.36, 0.01));
    REQUIRE_THAT(star.dec_deg, Catch::Matchers::WithinAbs(50.22, 0.01));
    REQUIRE(star.spectral_type == "K4V");

    // Array format fallback
    nlohmann::json arrayRow = nlohmann::json::array({"Kepler-442", 285.36, 50.22, "K4V"});
    auto star2 = CdsClient::parseSimbadRow(arrayRow);
    REQUIRE(star2.found == true);
    REQUIRE(star2.canonical_name == "Kepler-442");

    // Null row
    nlohmann::json nullRow = nullptr;
    auto star3 = CdsClient::parseSimbadRow(nullRow);
    REQUIRE(star3.found == false);
}

TEST_CASE("B/pastel ADQL coordinate query has correct structure", "[cds_client]") {
    std::string query = CdsClient::buildPastelCoordQuery(285.36, 50.22, 5.0);

    REQUIRE(query.find("B/pastel") != std::string::npos);
    REQUIRE(query.find("CIRCLE") != std::string::npos);
    REQUIRE(query.find("ICRS") != std::string::npos);
    REQUIRE(query.find("285.36") != std::string::npos);
    REQUIRE(query.find("50.22") != std::string::npos);
    REQUIRE(query.find("Teff") != std::string::npos);
    REQUIRE(query.find("[Fe/H]") != std::string::npos);
}

TEST_CASE("B/pastel JSON row parsing extracts metallicity", "[cds_client]") {
    // Array format: [Name, Teff, e_Teff, logg, e_logg, [Fe/H], e_[Fe/H]]
    nlohmann::json row = nlohmann::json::array({
        "Kepler-442",  // Name
        5324,          // Teff
        50,            // e_Teff
        4.45,          // logg
        0.05,          // e_logg
        -0.12,         // [Fe/H]
        0.04           // e_[Fe/H]
    });

    HostStarData star = CdsClient::parsePastelRow(row);

    REQUIRE(star.effective_temp_k.hasValue());
    REQUIRE(star.effective_temp_k.value == 5324);
    REQUIRE(star.effective_temp_k.source == DataSource::CDS_VIZIER);
    REQUIRE(star.effective_temp_k.uncertainty.has_value());
    REQUIRE_THAT(star.effective_temp_k.uncertainty.value(), Catch::Matchers::WithinAbs(50.0, 0.1));

    REQUIRE(star.metallicity.hasValue());
    REQUIRE_THAT(star.metallicity.value, Catch::Matchers::WithinAbs(-0.12, 0.01));
    REQUIRE(star.metallicity.source == DataSource::CDS_VIZIER);
    REQUIRE(star.metallicity.uncertainty.has_value());
    REQUIRE_THAT(star.metallicity.uncertainty.value(), Catch::Matchers::WithinAbs(0.04, 0.001));

    // Null fields handled gracefully
    nlohmann::json sparseRow = nlohmann::json::array({
        "Kepler-442",  // Name
        nullptr,       // Teff
        nullptr,       // e_Teff
        nullptr,       // logg
        nullptr,       // e_logg
        nullptr,       // [Fe/H]
        nullptr        // e_[Fe/H]
    });

    HostStarData sparse = CdsClient::parsePastelRow(sparseRow);
    REQUIRE(!sparse.effective_temp_k.hasValue());
    REQUIRE(!sparse.metallicity.hasValue());
}
