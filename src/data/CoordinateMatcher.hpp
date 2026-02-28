#pragma once
#include <string>
#include <cmath>

namespace astrocore {

class CoordinateMatcher {
public:
    static constexpr double DEFAULT_THRESHOLD_ARCSEC = 5.0;

    // Name-first matching: returns true if names match (case-insensitive, normalized)
    static bool nameMatch(const std::string& name1, const std::string& name2);

    // Angular distance in arcseconds between two (RA, Dec) positions using haversine
    static double angularDistance(double ra1_deg, double dec1_deg, double ra2_deg, double dec2_deg);

    // Check if two positions are within threshold arcseconds
    static bool withinThreshold(double ra1_deg, double dec1_deg,
                                double ra2_deg, double dec2_deg,
                                double threshold_arcsec = DEFAULT_THRESHOLD_ARCSEC);

    // Combined: name-first, coordinate fallback
    static bool isMatch(const std::string& name1, double ra1_deg, double dec1_deg,
                        const std::string& name2, double ra2_deg, double dec2_deg,
                        double threshold_arcsec = DEFAULT_THRESHOLD_ARCSEC);

private:
    static std::string normalize(const std::string& name);
};

}  // namespace astrocore
