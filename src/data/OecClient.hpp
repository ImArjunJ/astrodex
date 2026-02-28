#pragma once

#include "data/ExoplanetData.hpp"
#include <string>
#include <vector>
#include <future>
#include <memory>

namespace astrocore {

struct OecConfig {
    // GitHub raw content base URL for OEC systems
    std::string base_url = "https://raw.githubusercontent.com/OpenExoplanetCatalogue/open_exoplanet_catalogue/master/systems/";
    int timeout_seconds = 30;
    bool use_cache = true;
    std::string cache_directory = ".cache/oec";
    int cache_ttl_days = 7;  // OEC updates frequently from community
};

class OecClient {
public:
    explicit OecClient(const OecConfig& config = {});
    ~OecClient();

    OecClient(const OecClient&) = delete;
    OecClient& operator=(const OecClient&) = delete;

    // Query by planet name — fetches XML for the system
    std::future<std::vector<ExoplanetData>> queryByName(const std::string& planetName);
    std::vector<ExoplanetData> queryByNameSync(const std::string& planetName);

    // Parse XML string into ExoplanetData records (public for testing)
    static std::vector<ExoplanetData> parseSystemXml(const std::string& xmlContent);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    // Fetch XML from GitHub raw content
    std::string fetchSystemXml(const std::string& systemName);
};

}  // namespace astrocore
