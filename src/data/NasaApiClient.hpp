#pragma once

#include "data/ExoplanetData.hpp"
#include <string>
#include <vector>
#include <future>
#include <memory>

namespace astrocore {

struct NasaApiConfig {
    std::string tap_endpoint = "https://exoplanetarchive.ipac.caltech.edu/TAP/sync";
    int timeout_seconds = 30;
    bool use_cache = true;
    std::string cache_directory = ".cache/nasa";
};

class NasaApiClient {
public:
    explicit NasaApiClient(const NasaApiConfig& config = {});
    ~NasaApiClient();

    // Prevent copying
    NasaApiClient(const NasaApiClient&) = delete;
    NasaApiClient& operator=(const NasaApiClient&) = delete;

    // Query methods - return futures for async operation
    std::future<std::vector<ExoplanetData>> queryByName(const std::string& name);
    std::future<std::vector<ExoplanetData>> queryByHostStar(const std::string& starName);
    std::future<std::vector<ExoplanetData>> queryHabitableZone();
    std::future<std::vector<ExoplanetData>> queryAll(int limit = 100);

    // Synchronous query
    std::vector<ExoplanetData> queryByNameSync(const std::string& name);

    // Get popular/well-known exoplanets
    std::vector<ExoplanetData> getNotableExoplanets();

    // Check if API is reachable
    bool testConnection();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    // Build ADQL query
    std::string buildADQL(const std::string& whereClause, int limit = 0) const;

    // Execute query and parse results
    std::vector<ExoplanetData> executeQuery(const std::string& adql);

    // Parse NASA TAP JSON response
    ExoplanetData parseRow(const nlohmann::json& row) const;

    // URL encode string
    static std::string urlEncode(const std::string& str);
};

}  // namespace astrocore
