#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "data/CoordinateMatcher.hpp"

using namespace astrocore;

TEST_CASE("CoordinateMatcher name matching", "[coordinate_matcher]") {
    SECTION("Identical names match (case-insensitive)") {
        REQUIRE(CoordinateMatcher::nameMatch("Kepler-442", "Kepler-442"));
        REQUIRE(CoordinateMatcher::nameMatch("kepler-442", "KEPLER-442"));
        REQUIRE(CoordinateMatcher::nameMatch("Trappist-1", "TRAPPIST-1"));
    }

    SECTION("Name normalization handles hyphens and spaces") {
        REQUIRE(CoordinateMatcher::nameMatch("Kepler-442", "Kepler 442"));
        REQUIRE(CoordinateMatcher::nameMatch("kepler-442", "kepler 442"));
        REQUIRE(CoordinateMatcher::nameMatch("HD 209458", "HD-209458"));
    }

    SECTION("Different names don't match") {
        REQUIRE_FALSE(CoordinateMatcher::nameMatch("Kepler-442", "Kepler-186"));
        REQUIRE_FALSE(CoordinateMatcher::nameMatch("", "Kepler-442"));
        REQUIRE_FALSE(CoordinateMatcher::nameMatch("Kepler-442", ""));
    }

    SECTION("Whitespace trimming works") {
        REQUIRE(CoordinateMatcher::nameMatch("  Kepler-442  ", "Kepler-442"));
        REQUIRE(CoordinateMatcher::nameMatch("Kepler-442", "  kepler 442  "));
    }
}

TEST_CASE("CoordinateMatcher angular distance", "[coordinate_matcher]") {
    SECTION("Same position returns 0 distance") {
        double dist = CoordinateMatcher::angularDistance(180.0, 45.0, 180.0, 45.0);
        REQUIRE_THAT(dist, Catch::Matchers::WithinAbs(0.0, 0.01));
    }

    SECTION("Two stars within 5 arcsec match by coordinates") {
        // 2 arcsec separation (well within 5)
        double ra1 = 180.0;
        double dec1 = 45.0;
        double ra2 = 180.0 + (2.0 / 3600.0); // 2 arcsec in RA at equator
        double dec2 = 45.0;

        double dist = CoordinateMatcher::angularDistance(ra1, dec1, ra2, dec2);
        REQUIRE(dist < 5.0);
        REQUIRE(CoordinateMatcher::withinThreshold(ra1, dec1, ra2, dec2, 5.0));
    }

    SECTION("Two stars >5 arcsec apart do NOT match") {
        // 10 arcsec separation
        double ra1 = 180.0;
        double dec1 = 45.0;
        double ra2 = 180.0 + (10.0 / 3600.0);
        double dec2 = 45.0;

        double dist = CoordinateMatcher::angularDistance(ra1, dec1, ra2, dec2);
        REQUIRE(dist > 5.0);
        REQUIRE_FALSE(CoordinateMatcher::withinThreshold(ra1, dec1, ra2, dec2, 5.0));
    }

    SECTION("RA wrapping at 0/360 boundary") {
        // RA=359.999 deg and RA=0.001 deg should be close (0.002 deg = 7.2 arcsec)
        // But with haversine, this should compute correctly as small distance
        double ra1 = 359.9994;  // ~2 arcsec from 0
        double dec1 = 0.0;
        double ra2 = 0.0006;    // ~2 arcsec from 0
        double dec2 = 0.0;

        double dist = CoordinateMatcher::angularDistance(ra1, dec1, ra2, dec2);
        // Total separation should be ~4 arcsec (within 5)
        REQUIRE(dist < 5.0);
        REQUIRE(CoordinateMatcher::withinThreshold(ra1, dec1, ra2, dec2, 5.0));
    }

    SECTION("Pole behavior: high Dec objects close regardless of RA") {
        // At Dec=89.999, objects with very different RAs are still close
        double ra1 = 0.0;
        double dec1 = 89.999;
        double ra2 = 180.0;  // Opposite side of pole
        double dec2 = 89.999;

        double dist = CoordinateMatcher::angularDistance(ra1, dec1, ra2, dec2);
        // At near-pole, RA difference matters little
        REQUIRE(dist < 10.0);  // Should be very small
    }
}

TEST_CASE("CoordinateMatcher integrated matching", "[coordinate_matcher]") {
    SECTION("Name match takes precedence (fast path)") {
        // Same name, even if coordinates are far apart (shouldn't happen in reality)
        REQUIRE(CoordinateMatcher::isMatch("Kepler-442", 180.0, 45.0,
                                            "kepler-442", 200.0, 50.0));
    }

    SECTION("Coordinate fallback when names differ") {
        // Different names, but within 5 arcsec
        double ra1 = 180.0;
        double dec1 = 45.0;
        double ra2 = 180.0 + (2.0 / 3600.0);
        double dec2 = 45.0;

        REQUIRE(CoordinateMatcher::isMatch("Star-A", ra1, dec1,
                                            "Star-B", ra2, dec2, 5.0));
    }

    SECTION("No match when names differ and coordinates too far") {
        REQUIRE_FALSE(CoordinateMatcher::isMatch("Star-A", 180.0, 45.0,
                                                  "Star-B", 200.0, 50.0, 5.0));
    }
}
