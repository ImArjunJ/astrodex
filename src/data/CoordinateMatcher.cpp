#include "data/CoordinateMatcher.hpp"
#include <algorithm>
#include <cctype>

namespace astrocore {

static constexpr double DEG_TO_RAD = M_PI / 180.0;
static constexpr double RAD_TO_ARCSEC = 3600.0 * 180.0 / M_PI;

std::string CoordinateMatcher::normalize(const std::string& name) {
    std::string result;
    result.reserve(name.size());

    for (char c : name) {
        if (std::isspace(static_cast<unsigned char>(c))) continue;
        if (c == '-') continue;
        result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    return result;
}

bool CoordinateMatcher::nameMatch(const std::string& name1, const std::string& name2) {
    std::string n1 = normalize(name1);
    std::string n2 = normalize(name2);

    if (n1.empty() || n2.empty()) return false;
    return n1 == n2;
}

double CoordinateMatcher::angularDistance(double ra1_deg, double dec1_deg,
                                          double ra2_deg, double dec2_deg) {
    // Haversine formula — handles RA wrapping and pole behavior correctly
    double ra1 = ra1_deg * DEG_TO_RAD;
    double dec1 = dec1_deg * DEG_TO_RAD;
    double ra2 = ra2_deg * DEG_TO_RAD;
    double dec2 = dec2_deg * DEG_TO_RAD;

    double dra = ra2 - ra1;
    double ddec = dec2 - dec1;

    double a = std::sin(ddec / 2.0) * std::sin(ddec / 2.0) +
               std::cos(dec1) * std::cos(dec2) *
               std::sin(dra / 2.0) * std::sin(dra / 2.0);

    double c = 2.0 * std::asin(std::sqrt(std::min(1.0, a)));

    return c * RAD_TO_ARCSEC;
}

bool CoordinateMatcher::withinThreshold(double ra1_deg, double dec1_deg,
                                         double ra2_deg, double dec2_deg,
                                         double threshold_arcsec) {
    return angularDistance(ra1_deg, dec1_deg, ra2_deg, dec2_deg) <= threshold_arcsec;
}

bool CoordinateMatcher::isMatch(const std::string& name1, double ra1_deg, double dec1_deg,
                                 const std::string& name2, double ra2_deg, double dec2_deg,
                                 double threshold_arcsec) {
    // Name-first (fast path)
    if (nameMatch(name1, name2)) return true;

    // Coordinate fallback
    return withinThreshold(ra1_deg, dec1_deg, ra2_deg, dec2_deg, threshold_arcsec);
}

}  // namespace astrocore
