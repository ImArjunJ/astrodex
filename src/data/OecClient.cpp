#include "data/OecClient.hpp"
#include "core/Logger.hpp"
#include <pugixml.hpp>
#include <curl/curl.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <cmath>
#include <cctype>

namespace astrocore {

namespace fs = std::filesystem;

// Helper functions
namespace {
    // URL encode a string
    std::string urlEncode(const std::string& str) {
        CURL* curl = curl_easy_init();
        if (!curl) return str;

        char* encoded = curl_easy_escape(curl, str.c_str(), str.length());
        std::string result(encoded);
        curl_free(encoded);
        curl_easy_cleanup(curl);
        return result;
    }

    // Extract system name from planet name (e.g., "Kepler-442 b" -> "Kepler-442")
    std::string extractSystemName(const std::string& planetName) {
        // OEC convention: planet names end with a space and a single letter/digit
        // Remove trailing designator to get system name
        auto pos = planetName.find_last_of(' ');
        if (pos != std::string::npos && pos + 1 < planetName.length()) {
            // Check if what follows is a single character designation
            std::string designation = planetName.substr(pos + 1);
            if (designation.length() <= 2) {  // Single letter or letter+digit
                return planetName.substr(0, pos);
            }
        }
        return planetName;  // Return as-is if no pattern match
    }

    // Convert HMS (hours minutes seconds) to degrees
    double hmsToDegreesRA(const std::string& hms) {
        // Format: "HH MM SS" or "HH MM SS.S"
        std::istringstream iss(hms);
        double hours = 0, minutes = 0, seconds = 0;
        iss >> hours >> minutes >> seconds;
        return (hours + minutes/60.0 + seconds/3600.0) * 15.0;  // RA in hours * 15 = degrees
    }

    // Convert DMS (degrees minutes seconds) to degrees
    double dmsToDeclinesDec(const std::string& dms) {
        // Format: "+DD MM SS" or "-DD MM SS"
        std::istringstream iss(dms);
        double degrees = 0, minutes = 0, seconds = 0;
        char sign = '+';

        // Read sign if present
        std::string trimmed = dms;
        while (!trimmed.empty() && std::isspace(trimmed[0])) trimmed.erase(0, 1);
        if (!trimmed.empty() && (trimmed[0] == '+' || trimmed[0] == '-')) {
            sign = trimmed[0];
            trimmed.erase(0, 1);
        }

        std::istringstream iss2(trimmed);
        iss2 >> degrees >> minutes >> seconds;

        double result = std::abs(degrees) + minutes/60.0 + seconds/3600.0;
        return (sign == '-') ? -result : result;
    }

    // SHA256 hash for cache keys (simplified - using std::hash)
    std::string hashString(const std::string& str) {
        std::hash<std::string> hasher;
        size_t hash = hasher(str);
        std::ostringstream oss;
        oss << std::hex << hash;
        return oss.str();
    }
}

// PIMPL implementation
struct OecClient::Impl {
    OecConfig config;
    CURL* curl = nullptr;
    std::string responseBuffer;

    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        size_t totalSize = size * nmemb;
        std::string* buffer = static_cast<std::string*>(userp);
        buffer->append(static_cast<char*>(contents), totalSize);
        return totalSize;
    }

    Impl(const OecConfig& cfg) : config(cfg) {
        curl = curl_easy_init();
        if (!curl) {
            LOG_ERROR("Failed to initialize CURL for OecClient");
        }

        // Create cache directory if it doesn't exist
        if (config.use_cache) {
            fs::create_directories(config.cache_directory);
        }
    }

    ~Impl() {
        if (curl) {
            curl_easy_cleanup(curl);
        }
    }

    std::string getCachePath(const std::string& systemName) const {
        std::string filename = hashString(systemName) + ".xml";
        return config.cache_directory + "/" + filename;
    }

    bool isCacheValid(const std::string& cachePath) const {
        if (!fs::exists(cachePath)) return false;

        auto lastWrite = fs::last_write_time(cachePath);
        auto now = fs::file_time_type::clock::now();
        auto age = std::chrono::duration_cast<std::chrono::hours>(now - lastWrite);

        int maxAgeHours = config.cache_ttl_days * 24;
        return age.count() < maxAgeHours;
    }

    std::optional<std::string> readCache(const std::string& cachePath) const {
        if (!config.use_cache || !isCacheValid(cachePath)) {
            return std::nullopt;
        }

        std::ifstream file(cachePath);
        if (!file.is_open()) return std::nullopt;

        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    void writeCache(const std::string& cachePath, const std::string& data) const {
        if (!config.use_cache) return;

        std::ofstream file(cachePath);
        if (file.is_open()) {
            file << data;
        }
    }
};

OecClient::OecClient(const OecConfig& config)
    : m_impl(std::make_unique<Impl>(config)) {
}

OecClient::~OecClient() = default;

std::string OecClient::fetchSystemXml(const std::string& systemName) {
    // Check cache first
    std::string cachePath = m_impl->getCachePath(systemName);
    auto cached = m_impl->readCache(cachePath);
    if (cached) {
        LOG_INFO("OEC cache hit for system: {}", systemName);
        return *cached;
    }

    // Build URL
    std::string encodedName = urlEncode(systemName);
    std::string url = m_impl->config.base_url + encodedName + ".xml";

    LOG_INFO("Fetching OEC XML from: {}", url);

    // Setup CURL request
    m_impl->responseBuffer.clear();
    curl_easy_setopt(m_impl->curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(m_impl->curl, CURLOPT_WRITEFUNCTION, Impl::writeCallback);
    curl_easy_setopt(m_impl->curl, CURLOPT_WRITEDATA, &m_impl->responseBuffer);
    curl_easy_setopt(m_impl->curl, CURLOPT_TIMEOUT, m_impl->config.timeout_seconds);
    curl_easy_setopt(m_impl->curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(m_impl->curl, CURLOPT_USERAGENT, "Astrodex/1.0");

    CURLcode res = curl_easy_perform(m_impl->curl);

    if (res != CURLE_OK) {
        LOG_ERROR("CURL error fetching OEC system {}: {}", systemName, curl_easy_strerror(res));
        return "";
    }

    long httpCode = 0;
    curl_easy_getinfo(m_impl->curl, CURLINFO_RESPONSE_CODE, &httpCode);

    if (httpCode == 404) {
        LOG_WARN("OEC system not found: {}", systemName);
        return "";
    }

    if (httpCode != 200) {
        LOG_ERROR("HTTP error {} fetching OEC system: {}", httpCode, systemName);
        return "";
    }

    // Cache the response
    m_impl->writeCache(cachePath, m_impl->responseBuffer);

    return m_impl->responseBuffer;
}

std::vector<ExoplanetData> OecClient::parseSystemXml(const std::string& xmlContent) {
    std::vector<ExoplanetData> planets;

    if (xmlContent.empty()) {
        return planets;
    }

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(xmlContent.c_str());

    if (!result) {
        LOG_ERROR("OEC XML parse error: {}", result.description());
        return planets;
    }

    pugi::xml_node systemNode = doc.child("system");
    if (!systemNode) {
        LOG_ERROR("OEC XML missing <system> root node");
        return planets;
    }

    // Extract system-level data
    std::string systemName = systemNode.child_value("name");
    double systemDistance = 0.0;
    if (systemNode.child("distance")) {
        systemDistance = systemNode.child("distance").text().as_double();
    }

    // Parse RA/Dec from system node
    double ra_deg = 0.0;
    double dec_deg = 0.0;
    if (systemNode.child("rightascension")) {
        std::string raStr = systemNode.child_value("rightascension");
        ra_deg = hmsToDegreesRA(raStr);
    }
    if (systemNode.child("declination")) {
        std::string decStr = systemNode.child_value("declination");
        dec_deg = dmsToDeclinesDec(decStr);
    }

    // Recursive function to find all planet nodes
    constexpr int MAX_DEPTH = 20;
    std::function<void(pugi::xml_node, pugi::xml_node, int)> findPlanets;
    findPlanets = [&](pugi::xml_node node, pugi::xml_node parentStar, int depth) {
        if (depth > MAX_DEPTH) {
            LOG_WARN("OEC XML recursion depth limit ({}) reached, skipping subtree", MAX_DEPTH);
            return;
        }
        for (pugi::xml_node child : node.children()) {
            if (std::string(child.name()) == "planet") {
                // Found a planet - parse it
                ExoplanetData planet;

                // Planet name
                planet.name = child.child_value("name");
                if (planet.name.empty()) continue;  // Skip planets without names

                // Discovery info
                planet.discovery_method = child.child_value("discoverymethod");
                if (child.child("discoveryyear")) {
                    planet.discovery_year = child.child("discoveryyear").text().as_int();
                }

                // Parse planetary parameters
                // Mass - OEC uses Jupiter masses, convert to Earth masses
                if (child.child("mass")) {
                    double mass_jupiter = child.child("mass").text().as_double();
                    planet.mass_earth.value = mass_jupiter * constants::JUPITER_TO_EARTH_MASS;
                    planet.mass_earth.source = DataSource::OEC;

                    // Check for uncertainty
                    if (child.child("mass").attribute("errorminus")) {
                        double error = child.child("mass").attribute("errorminus").as_double();
                        planet.mass_earth.uncertainty = error * constants::JUPITER_TO_EARTH_MASS;
                    }
                }

                // Radius - OEC uses Jupiter radii, convert to Earth radii
                if (child.child("radius")) {
                    double radius_jupiter = child.child("radius").text().as_double();
                    planet.radius_earth.value = radius_jupiter * constants::JUPITER_TO_EARTH_RADIUS;
                    planet.radius_earth.source = DataSource::OEC;

                    if (child.child("radius").attribute("errorminus")) {
                        double error = child.child("radius").attribute("errorminus").as_double();
                        planet.radius_earth.uncertainty = error * constants::JUPITER_TO_EARTH_RADIUS;
                    }
                }

                // Orbital parameters
                if (child.child("period")) {
                    planet.orbital_period_days.value = child.child("period").text().as_double();
                    planet.orbital_period_days.source = DataSource::OEC;
                }

                if (child.child("semimajoraxis")) {
                    planet.semi_major_axis_au.value = child.child("semimajoraxis").text().as_double();
                    planet.semi_major_axis_au.source = DataSource::OEC;
                }

                if (child.child("eccentricity")) {
                    planet.eccentricity.value = child.child("eccentricity").text().as_double();
                    planet.eccentricity.source = DataSource::OEC;
                }

                // Temperature
                if (child.child("temperature")) {
                    planet.equilibrium_temp_k.value = child.child("temperature").text().as_double();
                    planet.equilibrium_temp_k.source = DataSource::OEC;
                }

                // Parse host star data from parent star node
                if (parentStar) {
                    planet.host_star.name = parentStar.child_value("name");

                    if (parentStar.child("mass")) {
                        planet.host_star.mass_solar.value = parentStar.child("mass").text().as_double();
                        planet.host_star.mass_solar.source = DataSource::OEC;
                    }

                    if (parentStar.child("radius")) {
                        planet.host_star.radius_solar.value = parentStar.child("radius").text().as_double();
                        planet.host_star.radius_solar.source = DataSource::OEC;
                    }

                    if (parentStar.child("temperature")) {
                        planet.host_star.effective_temp_k.value = parentStar.child("temperature").text().as_double();
                        planet.host_star.effective_temp_k.source = DataSource::OEC;
                    }

                    if (parentStar.child("metallicity")) {
                        planet.host_star.metallicity.value = parentStar.child("metallicity").text().as_double();
                        planet.host_star.metallicity.source = DataSource::OEC;
                    }

                    planet.host_star.spectral_type = parentStar.child_value("spectraltype");

                    if (parentStar.child("age")) {
                        planet.host_star.age_gyr.value = parentStar.child("age").text().as_double();
                        planet.host_star.age_gyr.source = DataSource::OEC;
                    }
                }

                // Set system-level data
                if (systemDistance > 0) {
                    planet.host_star.distance_pc.value = systemDistance;
                    planet.host_star.distance_pc.source = DataSource::OEC;
                }

                if (ra_deg != 0.0) {
                    planet.host_star.ra_deg.value = ra_deg;
                    planet.host_star.ra_deg.source = DataSource::OEC;
                }

                if (dec_deg != 0.0) {
                    planet.host_star.dec_deg.value = dec_deg;
                    planet.host_star.dec_deg.source = DataSource::OEC;
                }

                // Calculate derived values
                planet.calculateDerivedValues();

                planets.push_back(planet);
            }
            else if (std::string(child.name()) == "star") {
                // Found a star - recursively search for planets within it
                findPlanets(child, child, depth + 1);
            }
            else if (std::string(child.name()) == "binary") {
                // Found a binary system - recursively search within it
                findPlanets(child, parentStar, depth + 1);
            }
            else {
                // Other node types - continue recursing
                findPlanets(child, parentStar, depth + 1);
            }
        }
    };

    // Start recursive search from system root
    findPlanets(systemNode, pugi::xml_node(), 0);

    LOG_INFO("OEC parsed {} planets from system {}", planets.size(), systemName);
    return planets;
}

std::vector<ExoplanetData> OecClient::queryByNameSync(const std::string& planetName) {
    // Extract system name from planet name
    std::string systemName = extractSystemName(planetName);

    // Fetch XML
    std::string xml = fetchSystemXml(systemName);
    if (xml.empty()) {
        return {};
    }

    // Parse XML
    return parseSystemXml(xml);
}

std::future<std::vector<ExoplanetData>> OecClient::queryByName(const std::string& planetName) {
    return std::async(std::launch::async, [this, planetName]() {
        return queryByNameSync(planetName);
    });
}

std::vector<ExoplanetData> OecClient::queryAllSync() {
    // Fetch combined catalogue from OEC GitHub release (plain XML, not gzipped)
    static const char* CATALOGUE_URL =
        "https://raw.githubusercontent.com/OpenExoplanetCatalogue/"
        "oec_tables/master/comma_separated/open_exoplanet_catalogue.txt";

    // Check cache first
    std::string cachePath = m_impl->config.cache_directory + "/oec_catalogue.csv";
    auto cached = m_impl->readCache(cachePath);
    std::string responseBuffer;

    if (cached) {
        LOG_DEBUG("OEC catalogue cache hit");
        responseBuffer = *cached;
    } else {
        LOG_INFO("OEC: Fetching full catalogue from GitHub...");

        if (!m_impl->curl) return {};

        curl_easy_setopt(m_impl->curl, CURLOPT_URL, CATALOGUE_URL);
        curl_easy_setopt(m_impl->curl, CURLOPT_WRITEFUNCTION, Impl::writeCallback);
        curl_easy_setopt(m_impl->curl, CURLOPT_WRITEDATA, &responseBuffer);
        curl_easy_setopt(m_impl->curl, CURLOPT_TIMEOUT, 60L);
        curl_easy_setopt(m_impl->curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(m_impl->curl, CURLOPT_USERAGENT, "AstroCore/0.1.0");

        CURLcode res = curl_easy_perform(m_impl->curl);
        if (res != CURLE_OK) {
            LOG_ERROR("OEC catalogue fetch failed: {}", curl_easy_strerror(res));
            return {};
        }

        long httpCode = 0;
        curl_easy_getinfo(m_impl->curl, CURLINFO_RESPONSE_CODE, &httpCode);
        if (httpCode != 200) {
            LOG_ERROR("OEC catalogue returned HTTP {}", httpCode);
            return {};
        }

        m_impl->writeCache(cachePath, responseBuffer);
    }

    // Parse CSV: each row is a planet
    // OEC CSV columns (comma-separated):
    // 0:pl_name, 1:binary_flag, 2:mass_j, 3:radius_j, 4:period, 5:semi_major_axis,
    // 6:eccentricity, 7:periastron, 8:longitude, 9:ascending_node, 10:inclination,
    // 11:eq_temp, 12:age, 13:discovery_method, 14:discovery_year, 15:last_updated,
    // 16:ra, 17:dec, 18:dist_pc, 19:host_mass_sun, 20:host_radius_sun,
    // 21:host_metallicity, 22:host_teff, 23:host_age
    std::vector<ExoplanetData> results;
    std::istringstream stream(responseBuffer);
    std::string line;

    // Skip header line if present
    if (std::getline(stream, line) && line.find("# ") == 0) {
        // It was a comment/header, continue
    } else {
        // First line was data, reprocess
        stream.clear();
        stream.str(responseBuffer);
    }

    auto parseField = [](const std::string& s) -> double {
        if (s.empty()) return 0.0;
        try { return std::stod(s); } catch (...) { return 0.0; }
    };

    while (std::getline(stream, line)) {
        if (line.empty() || line[0] == '#') continue;

        // Split by comma
        std::vector<std::string> fields;
        std::istringstream lineStream(line);
        std::string field;
        while (std::getline(lineStream, field, ',')) {
            fields.push_back(field);
        }

        if (fields.size() < 15) continue;  // Minimum fields needed

        ExoplanetData planet;
        planet.name = fields[0];

        // Physical data (OEC uses Jupiter units)
        double massJ = parseField(fields.size() > 2 ? fields[2] : "");
        if (massJ > 0) {
            planet.mass_earth = MeasuredValue<double>(massJ * 317.828, DataSource::OEC);  // Jupiter to Earth masses
        }

        double radiusJ = parseField(fields.size() > 3 ? fields[3] : "");
        if (radiusJ > 0) {
            planet.radius_earth = MeasuredValue<double>(radiusJ * 11.209, DataSource::OEC);  // Jupiter to Earth radii
        }

        double period = parseField(fields.size() > 4 ? fields[4] : "");
        if (period > 0) {
            planet.orbital_period_days = MeasuredValue<double>(period, DataSource::OEC);
        }

        double sma = parseField(fields.size() > 5 ? fields[5] : "");
        if (sma > 0) {
            planet.semi_major_axis_au = MeasuredValue<double>(sma, DataSource::OEC);
        }

        double ecc = parseField(fields.size() > 6 ? fields[6] : "");
        if (ecc > 0) {
            planet.eccentricity = MeasuredValue<double>(ecc, DataSource::OEC);
        }

        double incl = parseField(fields.size() > 10 ? fields[10] : "");
        if (incl > 0) {
            planet.inclination_deg = MeasuredValue<double>(incl, DataSource::OEC);
        }

        double eqTemp = parseField(fields.size() > 11 ? fields[11] : "");
        if (eqTemp > 0) {
            planet.equilibrium_temp_k = MeasuredValue<double>(eqTemp, DataSource::OEC);
        }

        // Discovery info
        if (fields.size() > 13) planet.discovery_method = fields[13];
        if (fields.size() > 14) {
            int year = static_cast<int>(parseField(fields[14]));
            if (year > 1900) planet.discovery_year = year;
        }

        // Host star data
        if (fields.size() > 18) {
            double dist = parseField(fields[18]);
            if (dist > 0) planet.host_star.distance_pc = MeasuredValue<double>(dist, DataSource::OEC);
        }
        if (fields.size() > 19) {
            double hMass = parseField(fields[19]);
            if (hMass > 0) planet.host_star.mass_solar = MeasuredValue<double>(hMass, DataSource::OEC);
        }
        if (fields.size() > 20) {
            double hRad = parseField(fields[20]);
            if (hRad > 0) planet.host_star.radius_solar = MeasuredValue<double>(hRad, DataSource::OEC);
        }
        if (fields.size() > 21) {
            double met = parseField(fields[21]);
            if (met != 0) planet.host_star.metallicity = MeasuredValue<double>(met, DataSource::OEC);
        }
        if (fields.size() > 22) {
            double hTeff = parseField(fields[22]);
            if (hTeff > 0) planet.host_star.effective_temp_k = MeasuredValue<double>(hTeff, DataSource::OEC);
        }

        // RA/Dec for cross-matching
        if (fields.size() > 16) {
            double ra = parseField(fields[16]);
            if (ra > 0) planet.host_star.ra_deg = MeasuredValue<double>(ra, DataSource::OEC);
        }
        if (fields.size() > 17) {
            double dec = parseField(fields[17]);
            if (std::abs(dec) > 0.001) planet.host_star.dec_deg = MeasuredValue<double>(dec, DataSource::OEC);
        }

        planet.calculateDerivedValues();
        results.push_back(std::move(planet));
    }

    LOG_INFO("OEC: Parsed {} planets from catalogue", results.size());
    return results;
}

}  // namespace astrocore
