#pragma once

#include <string>

namespace astrocore::test {

// Sample NASA TAP JSON responses for testing without HTTP calls
// This is NOT a mock in the gmock sense - just test fixture data

const char* SAMPLE_KEPLER_442B_JSON = R"([
    {
        "pl_name": "Kepler-442 b",
        "hostname": "Kepler-442",
        "pl_orbper": 112.3053,
        "pl_orbpererr1": 0.0012,
        "pl_orbsmax": 0.409,
        "pl_orbsmaxerr1": 0.012,
        "pl_orbeccen": 0.04,
        "pl_orbeccenerr1": 0.01,
        "pl_bmasse": 2.36,
        "pl_bmasseerr1": 0.35,
        "pl_rade": 1.34,
        "pl_radeerr1": 0.14,
        "pl_dens": 5.89,
        "pl_denserr1": 1.2,
        "pl_eqt": 233.0,
        "pl_eqterr1": 10.0,
        "st_teff": 4402.0,
        "st_tefferr1": 100.0,
        "st_rad": 0.60,
        "st_raderr1": 0.04,
        "st_mass": 0.61,
        "st_masserr1": 0.03,
        "st_lum": -1.523,
        "st_spectype": "K5V",
        "sy_dist": 370.5,
        "st_ra": 295.654,
        "st_dec": 39.123,
        "disc_year": 2015,
        "discoverymethod": "Transit"
    }
])";

const char* SAMPLE_MISSING_FIELDS_JSON = R"([
    {
        "pl_name": "Test Planet b",
        "hostname": "Test Star",
        "pl_orbper": 100.0,
        "pl_orbpererr1": null,
        "pl_orbsmax": null,
        "pl_orbsmaxerr1": null,
        "pl_orbeccen": null,
        "pl_orbeccenerr1": null,
        "pl_bmasse": null,
        "pl_bmasseerr1": null,
        "pl_rade": null,
        "pl_radeerr1": null,
        "pl_dens": null,
        "pl_denserr1": null,
        "pl_eqt": null,
        "pl_eqterr1": null,
        "st_teff": 5500.0,
        "st_tefferr1": null,
        "st_rad": null,
        "st_raderr1": null,
        "st_mass": null,
        "st_masserr1": null,
        "st_lum": null,
        "st_spectype": "G2V",
        "sy_dist": null,
        "st_ra": 180.0,
        "st_dec": 45.0,
        "disc_year": 2020,
        "discoverymethod": "Radial Velocity"
    }
])";

}  // namespace astrocore::test
