#pragma once
#include "data/ExoplanetData.hpp"
#include <string>
#include <future>
#include <memory>

namespace astrocore {

struct GaiaConfig {
    std::string tap_endpoint = "https://gea.esac.esa.int/tap-server/tap/sync";
    int timeout_seconds = 60;  // Gaia queries can be slower
    bool use_cache = true;
    std::string cache_directory = ".cache/gaia";
    int cache_ttl_days = 90;  // Gaia DR3 is a frozen release
    // Optional authentication (from environment variables)
    std::string username;  // GAIA_USERNAME env var
    std::string password;  // GAIA_PASSWORD env var
};

class GaiaClient {
public:
    explicit GaiaClient(const GaiaConfig& config = {});
    ~GaiaClient();

    GaiaClient(const GaiaClient&) = delete;
    GaiaClient& operator=(const GaiaClient&) = delete;

    // Query host star by name (via Gaia's external catalog cross-match tables)
    std::future<HostStarData> queryHostStar(const std::string& starName);
    HostStarData queryHostStarSync(const std::string& starName);

    // Query by coordinates (cone search) - fallback when name match fails
    std::future<HostStarData> queryHostStarByCoords(double ra_deg, double dec_deg, double radius_arcsec = 5.0);
    HostStarData queryHostStarByCoordsSync(double ra_deg, double dec_deg, double radius_arcsec = 5.0);

    // Parse Gaia TAP JSON row into HostStarData (public for testing)
    static HostStarData parseGaiaRow(const nlohmann::json& row);

    // Build ADQL for coordinate cone search (public for testing)
    static std::string buildCoordQuery(double ra_deg, double dec_deg, double radius_arcsec);

    bool testConnection();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    std::string buildNameQuery(const std::string& starName) const;
    nlohmann::json executeQuery(const std::string& adql);
};

}  // namespace astrocore
