#include "data/ExoplanetEuClient.hpp"
#include "core/Logger.hpp"
#include <curl/curl.h>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <algorithm>
#include <cctype>

namespace astrocore {

struct ExoplanetEuClient::Impl {
    ExoplanetEuConfig config;
    CURL* curl = nullptr;
    std::string responseBuffer;
    std::vector<ExoplanetEuData> cachedCatalog;
    bool catalogLoaded = false;
    std::chrono::steady_clock::time_point lastCatalogLoad;

    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        auto* self = static_cast<Impl*>(userp);
        self->responseBuffer.append(static_cast<char*>(contents), size * nmemb);
        return size * nmemb;
    }

    // Parse a CSV line handling quoted fields
    static std::vector<std::string> parseCSVLine(const std::string& line) {
        std::vector<std::string> fields;
        std::string field;
        bool inQuotes = false;

        for (size_t i = 0; i < line.size(); ++i) {
            char c = line[i];
            if (c == '"') {
                inQuotes = !inQuotes;
            } else if (c == ',' && !inQuotes) {
                fields.push_back(field);
                field.clear();
            } else {
                field += c;
            }
        }
        fields.push_back(field);
        return fields;
    }

    // Safe double parsing
    static std::optional<double> parseDouble(const std::string& s) {
        if (s.empty()) return std::nullopt;
        try {
            return std::stod(s);
        } catch (...) {
            return std::nullopt;
        }
    }

    // Safe int parsing
    static std::optional<int> parseInt(const std::string& s) {
        if (s.empty()) return std::nullopt;
        try {
            return std::stoi(s);
        } catch (...) {
            return std::nullopt;
        }
    }
};

ExoplanetEuClient::ExoplanetEuClient(const ExoplanetEuConfig& config)
    : m_impl(std::make_unique<Impl>())
{
    m_impl->config = config;
    m_impl->curl = curl_easy_init();

    if (!m_impl->curl) {
        LOG_ERROR("ExoplanetEu: Failed to initialize CURL");
    }

    if (config.use_cache) {
        std::filesystem::create_directories(config.cache_directory);
    }
}

ExoplanetEuClient::~ExoplanetEuClient() {
    if (m_impl->curl) {
        curl_easy_cleanup(m_impl->curl);
    }
}

std::future<bool> ExoplanetEuClient::downloadCatalog() {
    return std::async(std::launch::async, [this]() {
        if (!m_impl->curl) return false;

        std::string csvData;
        std::string cachePath = m_impl->config.cache_directory + "/exoplanet_eu_catalog.csv";

        // Try loading from disk cache first
        if (m_impl->config.use_cache) {
            std::error_code ec;
            if (std::filesystem::exists(cachePath, ec)) {
                auto lastWrite = std::filesystem::last_write_time(cachePath, ec);
                auto age = std::filesystem::file_time_type::clock::now() - lastWrite;
                auto ageHours = std::chrono::duration_cast<std::chrono::hours>(age).count();

                if (ageHours < 24) {
                    std::ifstream f(cachePath);
                    if (f.good()) {
                        csvData.assign(std::istreambuf_iterator<char>(f),
                                       std::istreambuf_iterator<char>());
                        LOG_INFO("ExoplanetEu: Using cached catalog ({} hours old, {:.1f} MB)",
                                 ageHours, csvData.size() / 1e6);
                    }
                }
            }
        }

        // Download if no cache hit
        if (csvData.empty()) {
            LOG_INFO("ExoplanetEu: Downloading full catalog...");
            m_impl->responseBuffer.clear();

            curl_easy_setopt(m_impl->curl, CURLOPT_URL, m_impl->config.api_endpoint.c_str());
            curl_easy_setopt(m_impl->curl, CURLOPT_WRITEFUNCTION, Impl::writeCallback);
            curl_easy_setopt(m_impl->curl, CURLOPT_WRITEDATA, m_impl.get());
            curl_easy_setopt(m_impl->curl, CURLOPT_TIMEOUT, 120L);
            curl_easy_setopt(m_impl->curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(m_impl->curl, CURLOPT_USERAGENT, "AstroCore/0.1.0");

            CURLcode res = curl_easy_perform(m_impl->curl);
            if (res != CURLE_OK) {
                LOG_ERROR("ExoplanetEu: Download failed: {}", curl_easy_strerror(res));
                return false;
            }

            csvData = std::move(m_impl->responseBuffer);

            // Save to disk cache
            if (m_impl->config.use_cache && !csvData.empty()) {
                std::filesystem::create_directories(m_impl->config.cache_directory);
                std::ofstream out(cachePath, std::ios::binary);
                if (out.good()) {
                    out.write(csvData.data(), static_cast<std::streamsize>(csvData.size()));
                    LOG_INFO("ExoplanetEu: Saved catalog cache ({:.1f} MB)", csvData.size() / 1e6);
                }
            }
        }

        // Parse CSV
        std::istringstream stream(csvData);
        std::string line;
        std::vector<std::string> headers;

        // Read header
        if (std::getline(stream, line)) {
            headers = Impl::parseCSVLine(line);
        }

        // Find column indices for fields we care about
        // Based on exoplanet.eu CSV format (98 columns)
        int iName = 0, iMass = 2, iRadius = 8, iPeriod = 11, iSma = 14;
        int iEcc = 17, iInc = 20, iTempCalc = 53, iTempMeas = 56;
        int iHotSpot = 57, iAlbedo = 58, iAlbedoErrMin = 59, iAlbedoErrMax = 60;
        int iLogG = 61, iDetType = 63, iAltNames = 66, iMolecules = 67;
        int iStarName = 68, iStarDist = 76, iStarMet = 79, iStarMass = 82;
        int iStarRadius = 85, iStarSpType = 88, iStarAge = 89, iStarTeff = 92;

        m_impl->cachedCatalog.clear();
        int count = 0;

        while (std::getline(stream, line)) {
            if (line.empty()) continue;

            auto fields = Impl::parseCSVLine(line);
            if (fields.size() < 68) continue;  // Need at least molecules column

            ExoplanetEuData data;
            data.name = fields[iName];

            // Physical properties
            data.mass_earth = Impl::parseDouble(fields[iMass]);
            data.radius_earth = Impl::parseDouble(fields[iRadius]);
            data.temp_calculated_k = Impl::parseDouble(fields[iTempCalc]);

            // GOLD: Measured temperature (actual observation!)
            if (fields.size() > static_cast<size_t>(iTempMeas)) {
                data.temp_measured_k = Impl::parseDouble(fields[iTempMeas]);
            }

            // GOLD: Geometric albedo (actual observation!)
            if (fields.size() > static_cast<size_t>(iAlbedo)) {
                data.geometric_albedo = Impl::parseDouble(fields[iAlbedo]);
            }

            // GOLD: Detected molecules (actual observation!)
            if (fields.size() > static_cast<size_t>(iMolecules) && !fields[iMolecules].empty()) {
                data.molecules_detected = fields[iMolecules];
            }

            // Hot spot longitude (from phase curves)
            if (fields.size() > static_cast<size_t>(iHotSpot)) {
                data.hot_point_lon = Impl::parseDouble(fields[iHotSpot]);
            }

            // Orbital parameters
            data.period_days = Impl::parseDouble(fields[iPeriod]);
            data.semi_major_axis_au = Impl::parseDouble(fields[iSma]);
            data.eccentricity = Impl::parseDouble(fields[iEcc]);
            data.inclination_deg = Impl::parseDouble(fields[iInc]);

            // Detection type
            if (fields.size() > static_cast<size_t>(iDetType)) {
                data.detection_type = fields[iDetType];
            }

            // Star properties
            if (fields.size() > static_cast<size_t>(iStarName)) {
                data.star_name = fields[iStarName];
            }
            if (fields.size() > static_cast<size_t>(iStarTeff)) {
                data.star_temp_k = Impl::parseDouble(fields[iStarTeff]);
            }
            if (fields.size() > static_cast<size_t>(iStarRadius)) {
                data.star_radius_solar = Impl::parseDouble(fields[iStarRadius]);
            }
            if (fields.size() > static_cast<size_t>(iStarMass)) {
                data.star_mass_solar = Impl::parseDouble(fields[iStarMass]);
            }
            if (fields.size() > static_cast<size_t>(iStarMet)) {
                data.star_metallicity = Impl::parseDouble(fields[iStarMet]);
            }
            if (fields.size() > static_cast<size_t>(iStarAge)) {
                data.star_age_gyr = Impl::parseDouble(fields[iStarAge]);
            }
            if (fields.size() > static_cast<size_t>(iStarSpType)) {
                data.star_spectral_type = fields[iStarSpType];
            }

            m_impl->cachedCatalog.push_back(data);
            count++;
        }

        m_impl->catalogLoaded = true;
        m_impl->lastCatalogLoad = std::chrono::steady_clock::now();

        // Log stats
        int withMolecules = 0, withAlbedo = 0, withMeasuredTemp = 0;
        for (const auto& p : m_impl->cachedCatalog) {
            if (p.molecules_detected.has_value()) withMolecules++;
            if (p.geometric_albedo.has_value()) withAlbedo++;
            if (p.temp_measured_k.has_value()) withMeasuredTemp++;
        }

        LOG_INFO("ExoplanetEu: Loaded {} planets ({} with molecules, {} with albedo, {} with measured temp)",
                 count, withMolecules, withAlbedo, withMeasuredTemp);

        return true;
    });
}

std::future<std::optional<ExoplanetEuData>> ExoplanetEuClient::queryByName(const std::string& planetName) {
    return std::async(std::launch::async, [this, planetName]() {
        return queryByNameSync(planetName);
    });
}

std::optional<ExoplanetEuData> ExoplanetEuClient::queryByNameSync(const std::string& planetName) {
    // Ensure catalog is loaded
    if (!m_impl->catalogLoaded) {
        LOG_INFO("ExoplanetEu: Catalog not loaded, downloading...");
        auto future = downloadCatalog();
        future.wait();
        if (!future.get()) {
            LOG_ERROR("ExoplanetEu: Failed to download catalog");
            return std::nullopt;
        }
    }

    // Normalize search name (lowercase, remove ALL spaces for robust matching)
    auto normalize = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        s.erase(std::remove(s.begin(), s.end(), ' '), s.end());
        return s;
    };

    std::string searchNorm = normalize(planetName);
    LOG_DEBUG("ExoplanetEu: Searching for '{}' (normalized: '{}')", planetName, searchNorm);

    // Search in catalog
    for (const auto& planet : m_impl->cachedCatalog) {
        std::string nameNorm = normalize(planet.name);

        // Match if normalized names are equal or one contains the other
        if (nameNorm == searchNorm ||
            nameNorm.find(searchNorm) != std::string::npos ||
            searchNorm.find(nameNorm) != std::string::npos) {
            LOG_INFO("ExoplanetEu: Found {} (molecules: {}, albedo: {}, temp_meas: {})",
                      planet.name,
                      planet.molecules_detected.value_or("none"),
                      planet.geometric_albedo.has_value() ? std::to_string(*planet.geometric_albedo) : "none",
                      planet.temp_measured_k.has_value() ? std::to_string(*planet.temp_measured_k) + "K" : "none");
            return planet;
        }
    }

    LOG_WARN("ExoplanetEu: No match found for '{}' in {} planets", planetName, m_impl->cachedCatalog.size());
    return std::nullopt;
}

void ExoplanetEuClient::mergeIntoExoplanetData(ExoplanetData& nasaData, const ExoplanetEuData& euData) {
    // PRIORITY: Real observations from exoplanet.eu

    // Detected molecules - THIS IS GOLD for coloring
    if (euData.molecules_detected.has_value() && !euData.molecules_detected->empty()) {
        nasaData.atmosphere_composition.value = *euData.molecules_detected;
        nasaData.atmosphere_composition.source = DataSource::EXOATMOS;  // Actual observation!
        nasaData.atmosphere_composition.confidence = 0.95f;
        LOG_INFO("Merged DETECTED MOLECULES: {}", *euData.molecules_detected);
    }

    // Geometric albedo - actual measured reflectivity
    if (euData.geometric_albedo.has_value()) {
        nasaData.albedo.value = *euData.geometric_albedo;
        nasaData.albedo.source = DataSource::EXOATMOS;
        nasaData.albedo.confidence = 0.9f;
        LOG_INFO("Merged MEASURED ALBEDO: {:.3f}", *euData.geometric_albedo);
    }

    // Measured temperature (actual observation, not calculated!)
    if (euData.temp_measured_k.has_value()) {
        // Use measured temp if NASA only has calculated
        if (!nasaData.equilibrium_temp_k.hasValue() ||
            nasaData.equilibrium_temp_k.source != DataSource::EXOATMOS) {
            nasaData.equilibrium_temp_k.value = *euData.temp_measured_k;
            nasaData.equilibrium_temp_k.source = DataSource::EXOATMOS;
            nasaData.equilibrium_temp_k.ai_reasoning = "Directly measured via emission/transmission spectroscopy";
            LOG_INFO("Merged MEASURED TEMP: {:.0f} K", *euData.temp_measured_k);
        }
    }

    // Fill in missing NASA data with EU data (lower priority)
    if (!nasaData.mass_earth.hasValue() && euData.mass_earth.has_value()) {
        nasaData.mass_earth.value = *euData.mass_earth;
        nasaData.mass_earth.source = DataSource::CALCULATED;  // Different source
    }
    if (!nasaData.radius_earth.hasValue() && euData.radius_earth.has_value()) {
        nasaData.radius_earth.value = *euData.radius_earth;
        nasaData.radius_earth.source = DataSource::CALCULATED;
    }
    if (!nasaData.orbital_period_days.hasValue() && euData.period_days.has_value()) {
        nasaData.orbital_period_days.value = *euData.period_days;
        nasaData.orbital_period_days.source = DataSource::CALCULATED;
    }

    // Star properties
    if (!nasaData.host_star.metallicity.hasValue() && euData.star_metallicity.has_value()) {
        nasaData.host_star.metallicity.value = *euData.star_metallicity;
        nasaData.host_star.metallicity.source = DataSource::CALCULATED;
    }
    if (!nasaData.host_star.age_gyr.hasValue() && euData.star_age_gyr.has_value()) {
        nasaData.host_star.age_gyr.value = *euData.star_age_gyr;
        nasaData.host_star.age_gyr.source = DataSource::CALCULATED;
    }
}

bool ExoplanetEuClient::testConnection() {
    if (!m_impl->curl) return false;

    curl_easy_setopt(m_impl->curl, CURLOPT_URL, m_impl->config.api_endpoint.c_str());
    curl_easy_setopt(m_impl->curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(m_impl->curl, CURLOPT_TIMEOUT, 5L);

    CURLcode res = curl_easy_perform(m_impl->curl);
    curl_easy_setopt(m_impl->curl, CURLOPT_NOBODY, 0L);

    return res == CURLE_OK;
}

}  // namespace astrocore
