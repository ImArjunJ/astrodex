#pragma once
#include "data/ExoplanetData.hpp"
#include <string>
#include <future>
#include <memory>

namespace astrocore {

struct CdsConfig {
    // SIMBAD TAP for name resolution and basic stellar parameters
    std::string simbad_tap_endpoint = "https://simbad.u-strasbg.fr/simbad/sim-tap/sync";
    // VizieR TAP for detailed catalog queries
    std::string vizier_tap_endpoint = "https://tapvizier.u-strasbg.fr/TAPVizieR/tap/sync";
    int timeout_seconds = 30;
    bool use_cache = true;
    std::string cache_directory = ".cache/cds";
    int cache_ttl_days = 30;
};

class CdsClient {
public:
    explicit CdsClient(const CdsConfig& config = {});
    ~CdsClient();

    CdsClient(const CdsClient&) = delete;
    CdsClient& operator=(const CdsClient&) = delete;

    // Resolve star name to coordinates via SIMBAD
    struct ResolvedStar {
        std::string canonical_name;
        double ra_deg = 0.0;
        double dec_deg = 0.0;
        std::string spectral_type;
        bool found = false;
    };
    std::future<ResolvedStar> resolveStarName(const std::string& starName);
    ResolvedStar resolveStarNameSync(const std::string& starName);

    // Query VizieR for host star enrichment (metallicity, age, Teff from B/pastel catalog)
    std::future<HostStarData> queryHostStarEnrichment(const std::string& starName);
    HostStarData queryHostStarEnrichmentSync(const std::string& starName);

    // Query by coordinates fallback
    std::future<HostStarData> queryHostStarByCoords(double ra_deg, double dec_deg, double radius_arcsec = 5.0);
    HostStarData queryHostStarByCoordsSync(double ra_deg, double dec_deg, double radius_arcsec = 5.0);

    // Public for testing
    static ResolvedStar parseSimbadRow(const nlohmann::json& row);
    static HostStarData parsePastelRow(const nlohmann::json& row);
    static std::string buildSimbadNameQuery(const std::string& starName);
    static std::string buildPastelCoordQuery(double ra_deg, double dec_deg, double radius_arcsec);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    nlohmann::json executeQuery(const std::string& endpoint, const std::string& adql);
};

}  // namespace astrocore
