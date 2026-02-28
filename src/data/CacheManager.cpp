#include "data/CacheManager.hpp"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <chrono>

namespace astrocore {
namespace fs = std::filesystem;

CacheManager::CacheManager(const std::string& subdirectory,
                           const std::string& base_directory,
                           int ttl_days)
    : m_cacheDir(base_directory + "/" + subdirectory)
    , m_ttlDays(ttl_days) {
    // Create cache directory if it doesn't exist
    fs::create_directories(m_cacheDir);
}

std::string CacheManager::getCachePath(const std::string& planetName) const {
    // Sanitize planet name: lowercase, replace spaces with underscores
    std::string sanitized = planetName;
    std::transform(sanitized.begin(), sanitized.end(), sanitized.begin(), ::tolower);
    std::replace(sanitized.begin(), sanitized.end(), ' ', '_');

    // Remove any other problematic characters
    sanitized.erase(std::remove_if(sanitized.begin(), sanitized.end(),
        [](char c) { return !std::isalnum(c) && c != '_' && c != '-'; }),
        sanitized.end());

    return m_cacheDir + "/" + sanitized + ".json";
}

bool CacheManager::isExpired(const std::string& path) const {
    if (!fs::exists(path)) {
        return true;
    }

    // Get file modification time
    auto ftime = fs::last_write_time(path);
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
    );
    auto file_time = std::chrono::system_clock::to_time_t(sctp);

    // Check if older than TTL
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto age_seconds = std::difftime(now_time_t, file_time);
    auto ttl_seconds = m_ttlDays * 24 * 3600;

    return age_seconds > ttl_seconds;
}

bool CacheManager::store(const std::string& planetName, const ExoplanetData& data) {
    try {
        std::string cachePath = getCachePath(planetName);

        // Create JSON with metadata
        nlohmann::json cache_object;

        // Add metadata
        auto now = std::chrono::system_clock::now();
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::gmtime(&now_time_t);
        char timestamp[32];
        std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", &tm);

        cache_object["metadata"]["cached_at"] = std::string(timestamp);
        cache_object["metadata"]["ttl_days"] = m_ttlDays;

        // Add planet data
        cache_object["data"] = data.toJson();

        // Write to file
        std::ofstream file(cachePath);
        if (!file.is_open()) {
            return false;
        }

        file << cache_object.dump(2);  // Pretty print with 2-space indent
        file.close();

        return true;
    } catch (...) {
        return false;
    }
}

std::optional<ExoplanetData> CacheManager::retrieve(const std::string& planetName) {
    try {
        std::string cachePath = getCachePath(planetName);

        // Check if file exists and not expired
        if (!fs::exists(cachePath) || isExpired(cachePath)) {
            return std::nullopt;
        }

        // Read file
        std::ifstream file(cachePath);
        if (!file.is_open()) {
            return std::nullopt;
        }

        nlohmann::json cache_object;
        file >> cache_object;
        file.close();

        // Parse planet data from "data" field
        if (!cache_object.contains("data")) {
            return std::nullopt;
        }

        return ExoplanetData::fromJson(cache_object["data"]);
    } catch (...) {
        return std::nullopt;
    }
}

bool CacheManager::isCached(const std::string& planetName) {
    std::string cachePath = getCachePath(planetName);
    return fs::exists(cachePath) && !isExpired(cachePath);
}

std::vector<std::string> CacheManager::listCached() {
    std::vector<std::string> cached;

    try {
        if (!fs::exists(m_cacheDir)) {
            return cached;
        }

        for (const auto& entry : fs::directory_iterator(m_cacheDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                if (!isExpired(entry.path().string())) {
                    // Extract planet name from filename
                    std::string filename = entry.path().stem().string();

                    // Reverse sanitization: replace underscores with spaces
                    std::replace(filename.begin(), filename.end(), '_', ' ');

                    cached.push_back(filename);
                }
            }
        }
    } catch (...) {
        // Return empty list on error
    }

    return cached;
}

void CacheManager::clearExpired() {
    try {
        if (!fs::exists(m_cacheDir)) {
            return;
        }

        for (const auto& entry : fs::directory_iterator(m_cacheDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                if (isExpired(entry.path().string())) {
                    fs::remove(entry.path());
                }
            }
        }
    } catch (...) {
        // Ignore errors
    }
}

void CacheManager::clearAll() {
    try {
        if (fs::exists(m_cacheDir)) {
            fs::remove_all(m_cacheDir);
            fs::create_directories(m_cacheDir);
        }
    } catch (...) {
        // Ignore errors
    }
}

}  // namespace astrocore
