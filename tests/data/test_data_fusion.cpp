#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "data/DataFusionEngine.hpp"

using namespace astrocore;
using Catch::Matchers::WithinAbs;

TEST_CASE("DataFusionEngine: selectBestMeasurement picks lowest uncertainty", "[fusion_engine]") {
    std::vector<MeasuredValue<double>> candidates;

    // Candidate 1: NASA with uncertainty 0.5
    MeasuredValue<double> nasa_val(10.0, DataSource::NASA_TAP);
    nasa_val.uncertainty = 0.5;
    candidates.push_back(nasa_val);

    // Candidate 2: Gaia with uncertainty 0.2 (better)
    MeasuredValue<double> gaia_val(10.1, DataSource::GAIA);
    gaia_val.uncertainty = 0.2;
    candidates.push_back(gaia_val);

    // Candidate 3: OEC with uncertainty 1.0 (worse)
    MeasuredValue<double> oec_val(9.9, DataSource::OEC);
    oec_val.uncertainty = 1.0;
    candidates.push_back(oec_val);

    auto best = DataFusionEngine::selectBestMeasurement(candidates);

    REQUIRE(best.hasValue());
    REQUIRE_THAT(best.value, WithinAbs(10.1, 0.01));
    REQUIRE(best.source == DataSource::GAIA);
    REQUIRE(best.uncertainty.has_value());
    REQUIRE_THAT(*best.uncertainty, WithinAbs(0.2, 0.01));
}

TEST_CASE("DataFusionEngine: source priority when uncertainties equal", "[fusion_engine]") {
    std::vector<MeasuredValue<double>> candidates;

    // All have same uncertainty - priority should be NASA > Gaia > CDS > OEC
    MeasuredValue<double> oec_val(9.0, DataSource::OEC);
    oec_val.uncertainty = 1.0;
    candidates.push_back(oec_val);

    MeasuredValue<double> nasa_val(10.0, DataSource::NASA_TAP);
    nasa_val.uncertainty = 1.0;
    candidates.push_back(nasa_val);

    MeasuredValue<double> gaia_val(11.0, DataSource::GAIA);
    gaia_val.uncertainty = 1.0;
    candidates.push_back(gaia_val);

    MeasuredValue<double> cds_val(12.0, DataSource::CDS_VIZIER);
    cds_val.uncertainty = 1.0;
    candidates.push_back(cds_val);

    auto best = DataFusionEngine::selectBestMeasurement(candidates);

    REQUIRE(best.hasValue());
    REQUIRE_THAT(best.value, WithinAbs(10.0, 0.01));
    REQUIRE(best.source == DataSource::NASA_TAP);
}

TEST_CASE("DataFusionEngine: prefer value with uncertainty over value without", "[fusion_engine]") {
    std::vector<MeasuredValue<double>> candidates;

    // Candidate 1: NASA without uncertainty
    MeasuredValue<double> nasa_val(10.0, DataSource::NASA_TAP);
    candidates.push_back(nasa_val);

    // Candidate 2: Gaia with uncertainty (should win)
    MeasuredValue<double> gaia_val(11.0, DataSource::GAIA);
    gaia_val.uncertainty = 0.5;
    candidates.push_back(gaia_val);

    auto best = DataFusionEngine::selectBestMeasurement(candidates);

    REQUIRE(best.hasValue());
    REQUIRE_THAT(best.value, WithinAbs(11.0, 0.01));
    REQUIRE(best.source == DataSource::GAIA);
    REQUIRE(best.uncertainty.has_value());
}

TEST_CASE("DataFusionEngine: mergeHostStarData combines fields from multiple sources", "[fusion_engine]") {
    std::vector<HostStarData> sources;

    // Gaia source: has Teff and parallax
    HostStarData gaia;
    gaia.name = "Kepler-442";
    gaia.effective_temp_k.value = 5750.0;
    gaia.effective_temp_k.uncertainty = 50.0;
    gaia.effective_temp_k.source = DataSource::GAIA;
    gaia.distance_pc.value = 368.0;
    gaia.distance_pc.uncertainty = 10.0;
    gaia.distance_pc.source = DataSource::GAIA;
    sources.push_back(gaia);

    // CDS source: has metallicity and different Teff (worse uncertainty)
    HostStarData cds;
    cds.name = "Kepler-442";
    cds.effective_temp_k.value = 5800.0;
    cds.effective_temp_k.uncertainty = 100.0;
    cds.effective_temp_k.source = DataSource::CDS_VIZIER;
    cds.metallicity.value = 0.1;
    cds.metallicity.uncertainty = 0.05;
    cds.metallicity.source = DataSource::CDS_VIZIER;
    sources.push_back(cds);

    auto merged = DataFusionEngine::mergeHostStarData(sources);

    REQUIRE(merged.name == "Kepler-442");

    // Teff: Gaia should win (lower uncertainty)
    REQUIRE(merged.effective_temp_k.hasValue());
    REQUIRE_THAT(merged.effective_temp_k.value, WithinAbs(5750.0, 1.0));
    REQUIRE(merged.effective_temp_k.source == DataSource::GAIA);

    // Metallicity: CDS only
    REQUIRE(merged.metallicity.hasValue());
    REQUIRE_THAT(merged.metallicity.value, WithinAbs(0.1, 0.01));
    REQUIRE(merged.metallicity.source == DataSource::CDS_VIZIER);

    // Distance: Gaia only
    REQUIRE(merged.distance_pc.hasValue());
    REQUIRE_THAT(merged.distance_pc.value, WithinAbs(368.0, 1.0));
    REQUIRE(merged.distance_pc.source == DataSource::GAIA);
}

TEST_CASE("DataFusionEngine: mergeExoplanetData combines NASA and OEC data", "[fusion_engine]") {
    std::vector<ExoplanetData> sources;

    // NASA record: has mass and radius
    ExoplanetData nasa;
    nasa.name = "Kepler-442 b";
    nasa.discovery_method = "Transit";
    nasa.discovery_year = 2015;
    nasa.mass_earth.value = 2.3;
    nasa.mass_earth.uncertainty = 0.5;
    nasa.mass_earth.source = DataSource::NASA_TAP;
    nasa.radius_earth.value = 1.34;
    nasa.radius_earth.uncertainty = 0.1;
    nasa.radius_earth.source = DataSource::NASA_TAP;
    sources.push_back(nasa);

    // OEC record: has different mass (worse uncertainty) and orbital period
    ExoplanetData oec;
    oec.name = "Kepler-442 b";
    oec.mass_earth.value = 2.5;
    oec.mass_earth.uncertainty = 1.0;
    oec.mass_earth.source = DataSource::OEC;
    oec.orbital_period_days.value = 112.3;
    oec.orbital_period_days.uncertainty = 0.5;
    oec.orbital_period_days.source = DataSource::OEC;
    sources.push_back(oec);

    auto merged = DataFusionEngine::mergeExoplanetData(sources);

    REQUIRE(merged.name == "Kepler-442 b");
    REQUIRE(merged.discovery_method == "Transit");
    REQUIRE(merged.discovery_year == 2015);

    // Mass: NASA should win (lower uncertainty)
    REQUIRE(merged.mass_earth.hasValue());
    REQUIRE_THAT(merged.mass_earth.value, WithinAbs(2.3, 0.01));
    REQUIRE(merged.mass_earth.source == DataSource::NASA_TAP);

    // Radius: NASA only
    REQUIRE(merged.radius_earth.hasValue());
    REQUIRE_THAT(merged.radius_earth.value, WithinAbs(1.34, 0.01));
    REQUIRE(merged.radius_earth.source == DataSource::NASA_TAP);

    // Orbital period: OEC only
    REQUIRE(merged.orbital_period_days.hasValue());
    REQUIRE_THAT(merged.orbital_period_days.value, WithinAbs(112.3, 0.1));
    REQUIRE(merged.orbital_period_days.source == DataSource::OEC);
}

TEST_CASE("DataFusionEngine: provenance tracking on merged fields", "[fusion_engine]") {
    std::vector<MeasuredValue<double>> candidates;

    MeasuredValue<double> nasa_val(10.0, DataSource::NASA_TAP);
    nasa_val.uncertainty = 0.5;
    candidates.push_back(nasa_val);

    MeasuredValue<double> gaia_val(10.1, DataSource::GAIA);
    gaia_val.uncertainty = 0.3;
    candidates.push_back(gaia_val);

    auto best = DataFusionEngine::selectBestMeasurement(candidates);

    // Verify provenance is preserved
    REQUIRE(best.source == DataSource::GAIA);
    REQUIRE(best.uncertainty.has_value());
    REQUIRE_THAT(*best.uncertainty, WithinAbs(0.3, 0.01));

    // The selected measurement should retain all metadata
    REQUIRE(best.confidence == 1.0f);
}
