#pragma once
#include "data/ExoplanetData.hpp"
#include <string>
#include <optional>
#include <vector>

namespace astrocore {

class CacheManager {
public:
    explicit CacheManager(const std::string& subdirectory = "fused",
                          const std::string& base_directory = ".cache",
                          int ttl_days = 30);

    // Store a fused ExoplanetData record
    bool store(const std::string& planetName, const ExoplanetData& data);

    // Retrieve a cached record (returns nullopt if missing or expired)
    std::optional<ExoplanetData> retrieve(const std::string& planetName);

    // Check if a record is cached and not expired
    bool isCached(const std::string& planetName);

    // List all cached planet names
    std::vector<std::string> listCached();

    // Clear expired entries
    void clearExpired();

    // Clear all cache
    void clearAll();

private:
    std::string m_cacheDir;
    int m_ttlDays;

    std::string getCachePath(const std::string& planetName) const;
    bool isExpired(const std::string& path) const;
};

}  // namespace astrocore
