#include "data/CacheManager.hpp"

namespace astrocore {

CacheManager::CacheManager(const std::string& subdirectory,
                           const std::string& base_directory,
                           int ttl_days)
    : m_cacheDir(base_directory + "/" + subdirectory)
    , m_ttlDays(ttl_days) {
}

bool CacheManager::store(const std::string& planetName, const ExoplanetData& data) {
    // Stub implementation for Task 2
    return true;
}

std::optional<ExoplanetData> CacheManager::retrieve(const std::string& planetName) {
    // Stub implementation for Task 2
    return std::nullopt;
}

bool CacheManager::isCached(const std::string& planetName) {
    return false;
}

std::vector<std::string> CacheManager::listCached() {
    return {};
}

void CacheManager::clearExpired() {
}

void CacheManager::clearAll() {
}

std::string CacheManager::getCachePath(const std::string& planetName) const {
    return "";
}

bool CacheManager::isExpired(const std::string& path) const {
    return true;
}

}  // namespace astrocore
