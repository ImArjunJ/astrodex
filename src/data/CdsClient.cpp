#include "data/CdsClient.hpp"
#include "core/Logger.hpp"
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <mutex>

namespace astrocore {

namespace {
    std::once_flag g_cds_curl_init;
}

struct CdsClient::Impl {
    CdsConfig config;
    CURL* curl = nullptr;

    Impl(const CdsConfig& cfg) : config(cfg) {
        std::call_once(g_cds_curl_init, []() { curl_global_init(CURL_GLOBAL_DEFAULT); });
        curl = curl_easy_init();

        if (config.use_cache) {
            std::filesystem::create_directories(config.cache_directory);
        }
    }

    ~Impl() {
        if (curl) {
            curl_easy_cleanup(curl);
        }
    }

    std::string getCachePath(const std::string& queryKey) const {
        size_t hash = std::hash<std::string>{}(queryKey);
        return config.cache_directory + "/cds_" + std::to_string(hash) + ".json";
    }

    std::optional<std::string> readCache(const std::string& cachePath) const {
        if (!config.use_cache || !std::filesystem::exists(cachePath)) {
            return std::nullopt;
        }

        auto lastWrite = std::filesystem::last_write_time(cachePath);
        auto now = std::filesystem::file_time_type::clock::now();
        auto age = std::chrono::duration_cast<std::chrono::hours>(now - lastWrite).count() / 24;

        if (age > config.cache_ttl_days) {
            LOG_DEBUG("CDS cache expired for: {}", cachePath);
            return std::nullopt;
        }

        std::ifstream file(cachePath);
        if (!file) return std::nullopt;

        std::stringstream buffer;
        buffer << file.rdbuf();
        LOG_DEBUG("CDS cache hit: {}", cachePath);
        return buffer.str();
    }

    void writeCache(const std::string& cachePath, const std::string& data) const {
        if (!config.use_cache) return;

        std::ofstream file(cachePath);
        if (file) {
            file << data;
            LOG_DEBUG("CDS cache written: {}", cachePath);
        }
    }

    static size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
        size_t totalSize = size * nmemb;
        output->append(static_cast<const char*>(contents), totalSize);
        return totalSize;
    }

    std::string urlEncode(const std::string& str) {
        char* encoded = curl_easy_escape(curl, str.c_str(), static_cast<int>(str.length()));
        std::string result(encoded);
        curl_free(encoded);
        return result;
    }
};

CdsClient::CdsClient(const CdsConfig& config)
    : m_impl(std::make_unique<Impl>(config)) {
}

CdsClient::~CdsClient() = default;

std::string CdsClient::buildSimbadNameQuery(const std::string& starName) {
    // Escape single quotes to prevent ADQL injection
    std::string escaped = starName;
    std::string::size_type pos = 0;
    while ((pos = escaped.find('\'', pos)) != std::string::npos) {
        escaped.replace(pos, 1, "''");
        pos += 2;
    }

    std::ostringstream query;
    query << "SELECT TOP 1 main_id, ra, dec, sp_type "
          << "FROM basic JOIN ident ON oidref = oid "
          << "WHERE id = '" << escaped << "'";
    return query.str();
}

std::string CdsClient::buildPastelCoordQuery(double ra_deg, double dec_deg, double radius_arcsec) {
    double radius_deg = radius_arcsec / 3600.0;

    std::ostringstream query;
    query << "SELECT TOP 5 \"Name\", \"Teff\", \"e_Teff\", \"logg\", \"e_logg\", "
          << "\"[Fe/H]\", \"e_[Fe/H]\" "
          << "FROM \"B/pastel/pastel\" "
          << "WHERE 1=CONTAINS(POINT('ICRS', RAJ2000, DEJ2000), "
          << "CIRCLE('ICRS', " << ra_deg << ", " << dec_deg << ", " << radius_deg << ")) "
          << "ORDER BY \"Teff\" DESC";
    return query.str();
}

nlohmann::json CdsClient::executeQuery(const std::string& endpoint, const std::string& adql) {
    if (!m_impl->curl) {
        LOG_ERROR("CDS CURL not initialized");
        return nlohmann::json::array();
    }

    std::string cacheKey = endpoint + "|" + adql;
    std::string cachePath = m_impl->getCachePath(cacheKey);
    if (auto cached = m_impl->readCache(cachePath)) {
        try {
            return nlohmann::json::parse(*cached);
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to parse CDS cached JSON: {}", e.what());
        }
    }

    std::string url = endpoint;
    std::string params = "REQUEST=doQuery&LANG=ADQL&FORMAT=json&QUERY=" + m_impl->urlEncode(adql);
    url += "?" + params;

    LOG_DEBUG("CDS query: {}", url);

    std::string response;
    curl_easy_setopt(m_impl->curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(m_impl->curl, CURLOPT_WRITEFUNCTION, Impl::writeCallback);
    curl_easy_setopt(m_impl->curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(m_impl->curl, CURLOPT_TIMEOUT, m_impl->config.timeout_seconds);
    curl_easy_setopt(m_impl->curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(m_impl->curl);

    if (res != CURLE_OK) {
        LOG_ERROR("CDS CURL error: {}", curl_easy_strerror(res));
        return nlohmann::json::array();
    }

    try {
        auto json = nlohmann::json::parse(response);
        m_impl->writeCache(cachePath, response);

        // TAP responses typically have "data" array
        if (json.contains("data") && json["data"].is_array()) {
            return json["data"];
        }

        return json;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse CDS JSON: {}", e.what());
        return nlohmann::json::array();
    }
}

CdsClient::ResolvedStar CdsClient::parseSimbadRow(const nlohmann::json& row) {
    ResolvedStar star;

    if (row.is_null()) return star;

    // SIMBAD TAP can return object or array format
    if (row.is_object()) {
        if (row.contains("main_id") && !row["main_id"].is_null()) {
            star.canonical_name = row["main_id"].get<std::string>();
            star.found = true;
        }
        if (row.contains("ra") && !row["ra"].is_null()) {
            star.ra_deg = row["ra"].get<double>();
        }
        if (row.contains("dec") && !row["dec"].is_null()) {
            star.dec_deg = row["dec"].get<double>();
        }
        if (row.contains("sp_type") && !row["sp_type"].is_null()) {
            star.spectral_type = row["sp_type"].get<std::string>();
        }
    } else if (row.is_array() && row.size() >= 4) {
        // Array format: [main_id, ra, dec, sp_type]
        if (!row[0].is_null()) {
            star.canonical_name = row[0].get<std::string>();
            star.found = true;
        }
        if (!row[1].is_null()) star.ra_deg = row[1].get<double>();
        if (!row[2].is_null()) star.dec_deg = row[2].get<double>();
        if (!row[3].is_null()) star.spectral_type = row[3].get<std::string>();
    }

    return star;
}

HostStarData CdsClient::parsePastelRow(const nlohmann::json& row) {
    HostStarData star;

    if (row.is_null()) return star;

    auto getField = [&](const nlohmann::json& r, const std::string& key) -> std::optional<double> {
        if (r.is_object()) {
            if (r.contains(key) && !r[key].is_null() && r[key].is_number()) {
                return r[key].get<double>();
            }
        }
        return std::nullopt;
    };

    // Array format: [Name, Teff, e_Teff, logg, e_logg, [Fe/H], e_[Fe/H]]
    if (row.is_array() && row.size() >= 7) {
        // Teff (index 1) + e_Teff (index 2)
        if (!row[1].is_null() && row[1].is_number()) {
            star.effective_temp_k.value = row[1].get<double>();
            star.effective_temp_k.source = DataSource::CDS_VIZIER;
            if (!row[2].is_null() && row[2].is_number()) {
                star.effective_temp_k.uncertainty = row[2].get<double>();
            }
        }

        // [Fe/H] (index 5) + e_[Fe/H] (index 6)
        if (!row[5].is_null() && row[5].is_number()) {
            star.metallicity.value = row[5].get<double>();
            star.metallicity.source = DataSource::CDS_VIZIER;
            if (!row[6].is_null() && row[6].is_number()) {
                star.metallicity.uncertainty = row[6].get<double>();
            }
        }
    } else if (row.is_object()) {
        if (auto teff = getField(row, "Teff")) {
            star.effective_temp_k.value = *teff;
            star.effective_temp_k.source = DataSource::CDS_VIZIER;
            if (auto e_teff = getField(row, "e_Teff")) {
                star.effective_temp_k.uncertainty = *e_teff;
            }
        }
        if (auto feh = getField(row, "[Fe/H]")) {
            star.metallicity.value = *feh;
            star.metallicity.source = DataSource::CDS_VIZIER;
            if (auto e_feh = getField(row, "e_[Fe/H]")) {
                star.metallicity.uncertainty = *e_feh;
            }
        }
    }

    return star;
}

CdsClient::ResolvedStar CdsClient::resolveStarNameSync(const std::string& starName) {
    std::string adql = buildSimbadNameQuery(starName);
    auto results = executeQuery(m_impl->config.simbad_tap_endpoint, adql);

    if (results.is_array() && !results.empty()) {
        return parseSimbadRow(results[0]);
    }

    return ResolvedStar{};
}

HostStarData CdsClient::queryHostStarEnrichmentSync(const std::string& starName) {
    // Step 1: Resolve name to coordinates via SIMBAD
    auto resolved = resolveStarNameSync(starName);
    if (!resolved.found) {
        LOG_DEBUG("CDS: Could not resolve star name '{}' via SIMBAD", starName);
        return HostStarData{};
    }

    // Store spectral type from SIMBAD
    HostStarData star;
    if (!resolved.spectral_type.empty()) {
        star.spectral_type = resolved.spectral_type;
    }
    star.ra_deg = MeasuredValue<double>(resolved.ra_deg, DataSource::CDS_VIZIER);
    star.dec_deg = MeasuredValue<double>(resolved.dec_deg, DataSource::CDS_VIZIER);

    // Step 2: Query B/pastel for enrichment
    std::string pastelQuery = buildPastelCoordQuery(resolved.ra_deg, resolved.dec_deg, 5.0);
    auto pastelResults = executeQuery(m_impl->config.vizier_tap_endpoint, pastelQuery);

    if (pastelResults.is_array() && !pastelResults.empty()) {
        auto pastelStar = parsePastelRow(pastelResults[0]);
        // Merge pastel data into star
        if (pastelStar.effective_temp_k.hasValue()) {
            star.effective_temp_k = pastelStar.effective_temp_k;
        }
        if (pastelStar.metallicity.hasValue()) {
            star.metallicity = pastelStar.metallicity;
        }
    }

    return star;
}

HostStarData CdsClient::queryHostStarByCoordsSync(double ra_deg, double dec_deg, double radius_arcsec) {
    HostStarData star;
    star.ra_deg = MeasuredValue<double>(ra_deg, DataSource::CDS_VIZIER);
    star.dec_deg = MeasuredValue<double>(dec_deg, DataSource::CDS_VIZIER);

    std::string pastelQuery = buildPastelCoordQuery(ra_deg, dec_deg, radius_arcsec);
    auto pastelResults = executeQuery(m_impl->config.vizier_tap_endpoint, pastelQuery);

    if (pastelResults.is_array() && !pastelResults.empty()) {
        auto pastelStar = parsePastelRow(pastelResults[0]);
        if (pastelStar.effective_temp_k.hasValue()) {
            star.effective_temp_k = pastelStar.effective_temp_k;
        }
        if (pastelStar.metallicity.hasValue()) {
            star.metallicity = pastelStar.metallicity;
        }
    }

    return star;
}

std::future<CdsClient::ResolvedStar> CdsClient::resolveStarName(const std::string& starName) {
    return std::async(std::launch::async, [this, starName]() {
        return resolveStarNameSync(starName);
    });
}

std::future<HostStarData> CdsClient::queryHostStarEnrichment(const std::string& starName) {
    return std::async(std::launch::async, [this, starName]() {
        return queryHostStarEnrichmentSync(starName);
    });
}

std::future<HostStarData> CdsClient::queryHostStarByCoords(double ra_deg, double dec_deg, double radius_arcsec) {
    return std::async(std::launch::async, [this, ra_deg, dec_deg, radius_arcsec]() {
        return queryHostStarByCoordsSync(ra_deg, dec_deg, radius_arcsec);
    });
}

}  // namespace astrocore
