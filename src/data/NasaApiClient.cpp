#include "data/NasaApiClient.hpp"
#include "core/Logger.hpp"
#include <curl/curl.h>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <algorithm>
#include <cctype>

namespace astrocore {

struct NasaApiClient::Impl {
    NasaApiConfig config;
    CURL* curl = nullptr;
    std::string responseBuffer;

    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        auto* self = static_cast<Impl*>(userp);
        self->responseBuffer.append(static_cast<char*>(contents), size * nmemb);
        return size * nmemb;
    }
};

NasaApiClient::NasaApiClient(const NasaApiConfig& config)
    : m_impl(std::make_unique<Impl>())
{
    m_impl->config = config;
    m_impl->curl = curl_easy_init();

    if (!m_impl->curl) {
        LOG_ERROR("Failed to initialize CURL");
    }

    // Create cache directory if needed
    if (config.use_cache) {
        std::filesystem::create_directories(config.cache_directory);
    }
}

NasaApiClient::~NasaApiClient() {
    if (m_impl->curl) {
        curl_easy_cleanup(m_impl->curl);
    }
}

std::string NasaApiClient::urlEncode(const std::string& str) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : str) {
        if (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else if (c == ' ') {
            escaped << '+';
        } else {
            escaped << '%' << std::setw(2) << static_cast<int>(static_cast<unsigned char>(c));
        }
    }

    return escaped.str();
}

std::string NasaApiClient::buildADQL(const std::string& whereClause, int limit) const {
    std::ostringstream adql;

    // Select columns from the planetary systems (ps) table
    // TOP must come right after SELECT in ADQL
    adql << "SELECT ";
    if (limit > 0) {
        adql << "TOP " << limit << " ";
    }
    adql << "pl_name, hostname, "
         << "pl_orbper, pl_orbpererr1, "
         << "pl_orbsmax, pl_orbsmaxerr1, "
         << "pl_orbeccen, pl_orbeccenerr1, "
         << "pl_bmasse, pl_bmasseerr1, "
         << "pl_rade, pl_radeerr1, "
         << "pl_dens, pl_denserr1, "
         << "pl_eqt, pl_eqterr1, "
         << "st_teff, st_tefferr1, "
         << "st_rad, st_raderr1, "
         << "st_mass, st_masserr1, "
         << "st_lum, "
         << "st_spectype, "
         << "sy_dist, "
         << "disc_year, discoverymethod "
         << "FROM ps "
         << "WHERE default_flag = 1";

    if (!whereClause.empty()) {
        adql << " AND " << whereClause;
    }

    adql << " ORDER BY pl_name";

    return adql.str();
}

std::vector<ExoplanetData> NasaApiClient::executeQuery(const std::string& adql) {
    if (!m_impl->curl) {
        LOG_ERROR("CURL not initialized");
        return {};
    }

    m_impl->responseBuffer.clear();

    // Build URL with TAP query parameter
    std::string url = m_impl->config.tap_endpoint +
                      "?query=" + urlEncode(adql) +
                      "&format=json";

    LOG_DEBUG("NASA API query: {}", url);

    curl_easy_setopt(m_impl->curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(m_impl->curl, CURLOPT_WRITEFUNCTION, Impl::writeCallback);
    curl_easy_setopt(m_impl->curl, CURLOPT_WRITEDATA, m_impl.get());
    curl_easy_setopt(m_impl->curl, CURLOPT_TIMEOUT, static_cast<long>(m_impl->config.timeout_seconds));
    curl_easy_setopt(m_impl->curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(m_impl->curl, CURLOPT_USERAGENT, "AstroCore/0.1.0");

    CURLcode res = curl_easy_perform(m_impl->curl);

    if (res != CURLE_OK) {
        LOG_ERROR("NASA API request failed: {}", curl_easy_strerror(res));
        return {};
    }

    long httpCode = 0;
    curl_easy_getinfo(m_impl->curl, CURLINFO_RESPONSE_CODE, &httpCode);

    if (httpCode != 200) {
        LOG_ERROR("NASA API returned HTTP {}", httpCode);
        return {};
    }

    // Parse JSON response
    try {
        LOG_DEBUG("NASA API response size: {} bytes", m_impl->responseBuffer.size());

        // Check for error responses
        if (m_impl->responseBuffer.find("ERROR") != std::string::npos ||
            m_impl->responseBuffer.find("error") != std::string::npos) {
            LOG_ERROR("NASA API error: {}", m_impl->responseBuffer.substr(0, 500));
            return {};
        }

        auto json = nlohmann::json::parse(m_impl->responseBuffer);
        std::vector<ExoplanetData> results;

        // NASA TAP can return data as array directly or nested
        nlohmann::json dataArray;

        if (json.is_array()) {
            dataArray = json;
        } else if (json.contains("data")) {
            dataArray = json["data"];
        } else if (json.contains("results")) {
            dataArray = json["results"];
        } else {
            // Log the structure for debugging
            LOG_DEBUG("JSON structure: {}", json.dump().substr(0, 300));
            dataArray = json;
        }

        if (dataArray.is_array()) {
            for (const auto& row : dataArray) {
                try {
                    results.push_back(parseRow(row));
                } catch (const std::exception& e) {
                    LOG_WARN("Failed to parse exoplanet row: {}", e.what());
                }
            }
        }

        LOG_INFO("NASA API returned {} exoplanets", results.size());
        return results;

    } catch (const nlohmann::json::exception& e) {
        LOG_ERROR("Failed to parse NASA API response: {} - Response: {}",
                  e.what(), m_impl->responseBuffer.substr(0, 200));
        return {};
    }
}

ExoplanetData NasaApiClient::parseRow(const nlohmann::json& row) const {
    ExoplanetData data;

    // Helper to safely get nullable values
    auto getValue = [&row](const char* key) -> std::optional<double> {
        if (row.contains(key) && !row[key].is_null()) {
            return row[key].get<double>();
        }
        return std::nullopt;
    };

    auto getString = [&row](const char* key) -> std::string {
        if (row.contains(key) && !row[key].is_null()) {
            return row[key].get<std::string>();
        }
        return "";
    };

    // Basic info
    data.name = getString("pl_name");
    data.discovery_method = getString("discoverymethod");
    if (row.contains("disc_year") && !row["disc_year"].is_null()) {
        data.discovery_year = row["disc_year"].get<int>();
    }

    // Host star
    data.host_star.name = getString("hostname");
    if (auto val = getValue("st_teff")) {
        data.host_star.effective_temp_k.value = *val;
        data.host_star.effective_temp_k.source = DataSource::NASA_TAP;
        if (auto err = getValue("st_tefferr1")) {
            data.host_star.effective_temp_k.uncertainty = std::abs(*err);
        }
    }
    if (auto val = getValue("st_rad")) {
        data.host_star.radius_solar.value = *val;
        data.host_star.radius_solar.source = DataSource::NASA_TAP;
    }
    if (auto val = getValue("st_mass")) {
        data.host_star.mass_solar.value = *val;
        data.host_star.mass_solar.source = DataSource::NASA_TAP;
    }
    if (auto val = getValue("st_lum")) {
        data.host_star.luminosity_solar.value = std::pow(10.0, *val);  // Convert from log
        data.host_star.luminosity_solar.source = DataSource::NASA_TAP;
    }
    data.host_star.spectral_type = getString("st_spectype");
    if (auto val = getValue("sy_dist")) {
        data.host_star.distance_pc.value = *val;
        data.host_star.distance_pc.source = DataSource::NASA_TAP;
    }

    // Orbital parameters
    if (auto val = getValue("pl_orbper")) {
        data.orbital_period_days.value = *val;
        data.orbital_period_days.source = DataSource::NASA_TAP;
        if (auto err = getValue("pl_orbpererr1")) {
            data.orbital_period_days.uncertainty = std::abs(*err);
        }
    }
    if (auto val = getValue("pl_orbsmax")) {
        data.semi_major_axis_au.value = *val;
        data.semi_major_axis_au.source = DataSource::NASA_TAP;
    }
    if (auto val = getValue("pl_orbeccen")) {
        data.eccentricity.value = *val;
        data.eccentricity.source = DataSource::NASA_TAP;
    }

    // Physical parameters
    if (auto val = getValue("pl_bmasse")) {
        data.mass_earth.value = *val;
        data.mass_earth.source = DataSource::NASA_TAP;
        if (auto err = getValue("pl_bmasseerr1")) {
            data.mass_earth.uncertainty = std::abs(*err);
        }
    }
    if (auto val = getValue("pl_rade")) {
        data.radius_earth.value = *val;
        data.radius_earth.source = DataSource::NASA_TAP;
        if (auto err = getValue("pl_radeerr1")) {
            data.radius_earth.uncertainty = std::abs(*err);
        }
    }
    if (auto val = getValue("pl_dens")) {
        data.density_gcc.value = *val;
        data.density_gcc.source = DataSource::NASA_TAP;
    }
    if (auto val = getValue("pl_eqt")) {
        data.equilibrium_temp_k.value = *val;
        data.equilibrium_temp_k.source = DataSource::NASA_TAP;
    }

    // Calculate derived values
    data.calculateDerivedValues();

    return data;
}

// NASA Exoplanet Archive uses "Kepler-452 b" (space before planet letter).
// Normalise user input so "Kepler-452b" and "TRAPPIST-1e" etc. are found.
static std::string normalizePlanetName(const std::string& name) {
    if (name.size() >= 2) {
        char last       = name.back();
        char secondLast = name[name.size() - 2];
        // Insert space when the name ends with [digit][lowercase-letter] (no space already)
        if (std::islower(static_cast<unsigned char>(last)) &&
            (std::isdigit(static_cast<unsigned char>(secondLast)) ||
             std::isupper(static_cast<unsigned char>(secondLast)))) {
            return name.substr(0, name.size() - 1) + " " + last;
        }
    }
    return name;
}

std::future<std::vector<ExoplanetData>> NasaApiClient::queryByName(const std::string& name) {
    return std::async(std::launch::async, [this, name]() {
        std::string normalized = normalizePlanetName(name);
        std::string lowerName  = normalized;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        std::string where = "LOWER(pl_name) LIKE '%" + lowerName + "%'";
        return executeQuery(buildADQL(where, 50));
    });
}

std::vector<ExoplanetData> NasaApiClient::queryByNameSync(const std::string& name) {
    std::string normalized = normalizePlanetName(name);
    std::string lowerName  = normalized;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    std::string where = "LOWER(pl_name) LIKE '%" + lowerName + "%'";
    return executeQuery(buildADQL(where, 50));
}

std::future<std::vector<ExoplanetData>> NasaApiClient::queryByHostStar(const std::string& starName) {
    return std::async(std::launch::async, [this, starName]() {
        std::string where = "hostname LIKE '%" + starName + "%'";
        return executeQuery(buildADQL(where));
    });
}

std::future<std::vector<ExoplanetData>> NasaApiClient::queryHabitableZone() {
    return std::async(std::launch::async, [this]() {
        // Query planets with temperature roughly in habitable range
        std::string where = "pl_eqt BETWEEN 200 AND 350";
        return executeQuery(buildADQL(where, 100));
    });
}

std::future<std::vector<ExoplanetData>> NasaApiClient::queryAll(int limit) {
    return std::async(std::launch::async, [this, limit]() {
        return executeQuery(buildADQL("", limit));
    });
}

std::vector<ExoplanetData> NasaApiClient::getNotableExoplanets() {
    // Query some well-known exoplanets
    std::vector<std::string> notable = {
        "Kepler-442 b",
        "Kepler-452 b",
        "TRAPPIST-1 e",
        "TRAPPIST-1 f",
        "Proxima Cen b",
        "TOI-700 d",
        "Kepler-22 b",
        "K2-18 b",
        "LHS 1140 b",
        "Ross 128 b"
    };

    std::vector<ExoplanetData> results;
    for (const auto& name : notable) {
        auto planets = queryByNameSync(name);
        if (!planets.empty()) {
            results.push_back(planets[0]);
        }
    }

    return results;
}

bool NasaApiClient::testConnection() {
    if (!m_impl->curl) return false;

    // Simple HEAD request to check if endpoint is reachable
    curl_easy_setopt(m_impl->curl, CURLOPT_URL, m_impl->config.tap_endpoint.c_str());
    curl_easy_setopt(m_impl->curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(m_impl->curl, CURLOPT_TIMEOUT, 5L);

    CURLcode res = curl_easy_perform(m_impl->curl);

    // Reset options
    curl_easy_setopt(m_impl->curl, CURLOPT_NOBODY, 0L);

    return res == CURLE_OK;
}

}  // namespace astrocore
