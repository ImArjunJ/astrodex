#include "data/GaiaClient.hpp"
#include "core/Logger.hpp"
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cmath>
#include <mutex>

namespace astrocore {

namespace {
    std::once_flag g_gaia_curl_init;
}

// PIMPL implementation
struct GaiaClient::Impl {
    GaiaConfig config;
    CURL* curl = nullptr;

    Impl(const GaiaConfig& cfg) : config(cfg) {
        std::call_once(g_gaia_curl_init, []() { curl_global_init(CURL_GLOBAL_DEFAULT); });
        curl = curl_easy_init();

        // Check for authentication from environment variables
        if (const char* username = std::getenv("GAIA_USERNAME")) {
            config.username = username;
        }
        if (const char* password = std::getenv("GAIA_PASSWORD")) {
            config.password = password;
        }

        // Create cache directory if needed
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
        return config.cache_directory + "/gaia_" + std::to_string(hash) + ".json";
    }

    std::optional<std::string> readCache(const std::string& cachePath) const {
        if (!config.use_cache || !std::filesystem::exists(cachePath)) {
            return std::nullopt;
        }

        // Check TTL
        auto lastWrite = std::filesystem::last_write_time(cachePath);
        auto now = std::filesystem::file_time_type::clock::now();
        auto age = std::chrono::duration_cast<std::chrono::hours>(now - lastWrite).count() / 24;

        if (age > config.cache_ttl_days) {
            LOG_DEBUG("Cache expired for: {}", cachePath);
            return std::nullopt;
        }

        std::ifstream file(cachePath);
        if (!file) return std::nullopt;

        std::stringstream buffer;
        buffer << file.rdbuf();
        LOG_DEBUG("Cache hit: {}", cachePath);
        return buffer.str();
    }

    void writeCache(const std::string& cachePath, const std::string& data) const {
        if (!config.use_cache) return;

        std::ofstream file(cachePath);
        if (file) {
            file << data;
            LOG_DEBUG("Cache written: {}", cachePath);
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

GaiaClient::GaiaClient(const GaiaConfig& config)
    : m_impl(std::make_unique<Impl>(config)) {
}

GaiaClient::~GaiaClient() = default;

std::string GaiaClient::buildCoordQuery(double ra_deg, double dec_deg, double radius_arcsec) {
    double radius_deg = radius_arcsec / 3600.0;

    std::ostringstream query;
    query << "SELECT TOP 5 g.source_id, g.ra, g.dec, g.parallax, g.parallax_error, ";
    query << "g.phot_g_mean_mag, g.bp_rp, ";
    query << "a.teff_gspphot, a.teff_gspphot_lower, a.teff_gspphot_upper, ";
    query << "a.logg_gspphot, a.logg_gspphot_lower, a.logg_gspphot_upper, ";
    query << "a.mh_gspphot, a.mh_gspphot_lower, a.mh_gspphot_upper, ";
    query << "a.distance_gspphot, a.distance_gspphot_lower, a.distance_gspphot_upper, ";
    query << "a.radius_flame, a.radius_flame_lower, a.radius_flame_upper, ";
    query << "a.lum_flame, a.lum_flame_lower, a.lum_flame_upper, ";
    query << "a.age_flame, a.age_flame_lower, a.age_flame_upper, ";
    query << "a.mass_flame, a.mass_flame_lower, a.mass_flame_upper ";
    query << "FROM gaiadr3.gaia_source AS g ";
    query << "JOIN gaiadr3.astrophysical_parameters AS a ON g.source_id = a.source_id ";
    query << "WHERE CONTAINS(POINT('ICRS', g.ra, g.dec), ";
    query << "CIRCLE('ICRS', " << ra_deg << ", " << dec_deg << ", " << radius_deg << "))=1 ";
    query << "ORDER BY g.phot_g_mean_mag ASC";

    return query.str();
}

std::string GaiaClient::buildNameQuery(const std::string& starName) const {
    // For name-based queries, we rely on coordinate fallback since Gaia doesn't have direct name indexing
    // This is a placeholder - in practice, the DataFusionEngine will resolve name -> coords via NASA/SIMBAD first
    // For testing purposes, we return an empty result
    return "";
}

nlohmann::json GaiaClient::executeQuery(const std::string& adql) {
    if (!m_impl->curl) {
        LOG_ERROR("CURL not initialized");
        return nlohmann::json::array();
    }

    // Check cache first
    std::string cachePath = m_impl->getCachePath(adql);
    if (auto cached = m_impl->readCache(cachePath)) {
        try {
            return nlohmann::json::parse(*cached);
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to parse cached JSON: {}", e.what());
        }
    }

    // Build query URL
    std::string url = m_impl->config.tap_endpoint;
    std::string requestParam = "REQUEST=doQuery";
    std::string langParam = "LANG=ADQL";
    std::string formatParam = "FORMAT=json";
    std::string queryParam = "QUERY=" + m_impl->urlEncode(adql);

    url += "?" + requestParam + "&" + langParam + "&" + formatParam + "&" + queryParam;

    LOG_DEBUG("Gaia query: {}", url);

    // Execute HTTP request
    std::string response;
    curl_easy_setopt(m_impl->curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(m_impl->curl, CURLOPT_WRITEFUNCTION, Impl::writeCallback);
    curl_easy_setopt(m_impl->curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(m_impl->curl, CURLOPT_TIMEOUT, m_impl->config.timeout_seconds);
    curl_easy_setopt(m_impl->curl, CURLOPT_FOLLOWLOCATION, 1L);

    // Add authentication if provided
    if (!m_impl->config.username.empty() && !m_impl->config.password.empty()) {
        std::string userpwd = m_impl->config.username + ":" + m_impl->config.password;
        curl_easy_setopt(m_impl->curl, CURLOPT_USERPWD, userpwd.c_str());
    }

    CURLcode res = curl_easy_perform(m_impl->curl);

    if (res != CURLE_OK) {
        LOG_ERROR("CURL error: {}", curl_easy_strerror(res));
        return nlohmann::json::array();
    }

    // Parse JSON response
    try {
        auto json = nlohmann::json::parse(response);

        // Write to cache
        m_impl->writeCache(cachePath, response);

        // Gaia TAP returns data in "data" array
        if (json.contains("data") && json["data"].is_array()) {
            return json["data"];
        }

        return json;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse Gaia JSON: {}", e.what());
        return nlohmann::json::array();
    }
}

HostStarData GaiaClient::parseGaiaRow(const nlohmann::json& row) {
    HostStarData star;

    if (row.is_null() || !row.is_array()) {
        return star;
    }

    // Gaia TAP returns arrays where each element corresponds to a column
    // Order matches the SELECT statement in buildCoordQuery
    int idx = 0;

    auto getDouble = [&](int index) -> std::optional<double> {
        if (index >= row.size()) return std::nullopt;
        if (row[index].is_null()) return std::nullopt;
        if (row[index].is_number()) return row[index].get<double>();
        return std::nullopt;
    };

    // source_id (0), ra (1), dec (2)
    if (auto ra = getDouble(1)) {
        star.ra_deg = MeasuredValue<double>(*ra, DataSource::GAIA);
    }
    if (auto dec = getDouble(2)) {
        star.dec_deg = MeasuredValue<double>(*dec, DataSource::GAIA);
    }

    // parallax (3), parallax_error (4)
    auto parallax = getDouble(3);
    auto parallax_err = getDouble(4);

    // phot_g_mean_mag (5), bp_rp (6) - skip for now

    // teff_gspphot (7), teff_gspphot_lower (8), teff_gspphot_upper (9)
    if (auto teff = getDouble(7)) {
        star.effective_temp_k.value = *teff;
        star.effective_temp_k.source = DataSource::GAIA;

        auto teff_lower = getDouble(8);
        auto teff_upper = getDouble(9);
        if (teff_lower && teff_upper) {
            star.effective_temp_k.uncertainty = (*teff_upper - *teff_lower) / 2.0;
        }
    }

    // logg_gspphot (10), logg_gspphot_lower (11), logg_gspphot_upper (12) - skip, not in HostStarData

    // mh_gspphot (13), mh_gspphot_lower (14), mh_gspphot_upper (15)
    if (auto mh = getDouble(13)) {
        star.metallicity.value = *mh;
        star.metallicity.source = DataSource::GAIA;

        auto mh_lower = getDouble(14);
        auto mh_upper = getDouble(15);
        if (mh_lower && mh_upper) {
            star.metallicity.uncertainty = (*mh_upper - *mh_lower) / 2.0;
        }
    }

    // distance_gspphot (16), distance_gspphot_lower (17), distance_gspphot_upper (18)
    if (auto dist = getDouble(16)) {
        star.distance_pc.value = *dist;
        star.distance_pc.source = DataSource::GAIA;

        auto dist_lower = getDouble(17);
        auto dist_upper = getDouble(18);
        if (dist_lower && dist_upper) {
            star.distance_pc.uncertainty = (*dist_upper - *dist_lower) / 2.0;
        }
    } else if (parallax && *parallax > 0) {
        // Fallback: compute distance from parallax
        star.distance_pc.value = 1000.0 / *parallax;
        star.distance_pc.source = DataSource::GAIA;
        if (parallax_err && *parallax_err > 0) {
            // Error propagation: d_err = d * (parallax_err / parallax)
            star.distance_pc.uncertainty = star.distance_pc.value * (*parallax_err / *parallax);
        }
    }

    // radius_flame (19), radius_flame_lower (20), radius_flame_upper (21)
    if (auto radius = getDouble(19)) {
        star.radius_solar.value = *radius;
        star.radius_solar.source = DataSource::GAIA;

        auto radius_lower = getDouble(20);
        auto radius_upper = getDouble(21);
        if (radius_lower && radius_upper) {
            star.radius_solar.uncertainty = (*radius_upper - *radius_lower) / 2.0;
        }
    }

    // lum_flame (22), lum_flame_lower (23), lum_flame_upper (24)
    if (auto lum = getDouble(22)) {
        star.luminosity_solar.value = *lum;
        star.luminosity_solar.source = DataSource::GAIA;

        auto lum_lower = getDouble(23);
        auto lum_upper = getDouble(24);
        if (lum_lower && lum_upper) {
            star.luminosity_solar.uncertainty = (*lum_upper - *lum_lower) / 2.0;
        }
    }

    // age_flame (25), age_flame_lower (26), age_flame_upper (27)
    if (auto age = getDouble(25)) {
        star.age_gyr.value = *age;
        star.age_gyr.source = DataSource::GAIA;

        auto age_lower = getDouble(26);
        auto age_upper = getDouble(27);
        if (age_lower && age_upper) {
            star.age_gyr.uncertainty = (*age_upper - *age_lower) / 2.0;
        }
    }

    // mass_flame (28), mass_flame_lower (29), mass_flame_upper (30)
    if (auto mass = getDouble(28)) {
        star.mass_solar.value = *mass;
        star.mass_solar.source = DataSource::GAIA;

        auto mass_lower = getDouble(29);
        auto mass_upper = getDouble(30);
        if (mass_lower && mass_upper) {
            star.mass_solar.uncertainty = (*mass_upper - *mass_lower) / 2.0;
        }
    }

    return star;
}

HostStarData GaiaClient::queryHostStarByCoordsSync(double ra_deg, double dec_deg, double radius_arcsec) {
    std::string adql = buildCoordQuery(ra_deg, dec_deg, radius_arcsec);
    auto results = executeQuery(adql);

    if (results.is_array() && !results.empty()) {
        // Return the first (brightest) result
        return parseGaiaRow(results[0]);
    }

    return HostStarData{};
}

HostStarData GaiaClient::queryHostStarSync(const std::string& starName) {
    // Name-based queries not directly supported by Gaia
    // In production, this would resolve name->coords via SIMBAD first
    LOG_DEBUG("GaiaClient::queryHostStarSync - name resolution not implemented, use coordinate query");
    return HostStarData{};
}

std::future<HostStarData> GaiaClient::queryHostStar(const std::string& starName) {
    return std::async(std::launch::async, [this, starName]() {
        return queryHostStarSync(starName);
    });
}

std::future<HostStarData> GaiaClient::queryHostStarByCoords(double ra_deg, double dec_deg, double radius_arcsec) {
    return std::async(std::launch::async, [this, ra_deg, dec_deg, radius_arcsec]() {
        return queryHostStarByCoordsSync(ra_deg, dec_deg, radius_arcsec);
    });
}

bool GaiaClient::testConnection() {
    // Simple test query
    std::string testQuery = "SELECT TOP 1 source_id FROM gaiadr3.gaia_source";
    auto results = executeQuery(testQuery);
    return !results.empty();
}

}  // namespace astrocore
