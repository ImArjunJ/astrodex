#include "data/ExoMastClient.hpp"
#include "core/Logger.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <regex>

namespace astrocore {

struct ExoMastClient::Impl {
    ExoMastConfig config;
    CURL* curl = nullptr;
    std::string responseBuffer;

    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        auto* self = static_cast<Impl*>(userp);
        self->responseBuffer.append(static_cast<char*>(contents), size * nmemb);
        return size * nmemb;
    }

    // Normalize planet name for API queries
    static std::string normalizePlanetName(const std::string& name) {
        std::string normalized = name;
        // Replace spaces with %20 for URL
        std::string result;
        for (char c : normalized) {
            if (c == ' ') {
                result += "%20";
            } else {
                result += c;
            }
        }
        return result;
    }
};

ExoMastClient::ExoMastClient(const ExoMastConfig& config)
    : m_impl(std::make_unique<Impl>())
{
    m_impl->config = config;
    m_impl->curl = curl_easy_init();

    if (!m_impl->curl) {
        LOG_ERROR("ExoMAST: Failed to initialize CURL");
    }

    if (config.use_cache) {
        std::filesystem::create_directories(config.cache_directory);
    }
}

ExoMastClient::~ExoMastClient() {
    if (m_impl->curl) {
        curl_easy_cleanup(m_impl->curl);
    }
}

std::future<std::optional<AtmosphericData>> ExoMastClient::queryAtmosphere(const std::string& planetName) {
    return std::async(std::launch::async, [this, planetName]() {
        return queryAtmosphereSync(planetName);
    });
}

std::optional<AtmosphericData> ExoMastClient::queryAtmosphereSync(const std::string& planetName) {
    if (!m_impl->curl) {
        LOG_ERROR("ExoMAST: CURL not initialized");
        return std::nullopt;
    }

    LOG_INFO("ExoMAST: Querying atmospheric data for {}", planetName);

    m_impl->responseBuffer.clear();

    // Build URL for ExoMAST spectra endpoint
    std::string normalizedName = Impl::normalizePlanetName(planetName);
    std::string url = m_impl->config.api_endpoint + "/exoplanets/" + normalizedName + "/spectra/";

    LOG_DEBUG("ExoMAST URL: {}", url);

    curl_easy_setopt(m_impl->curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(m_impl->curl, CURLOPT_WRITEFUNCTION, Impl::writeCallback);
    curl_easy_setopt(m_impl->curl, CURLOPT_WRITEDATA, m_impl.get());
    curl_easy_setopt(m_impl->curl, CURLOPT_TIMEOUT, static_cast<long>(m_impl->config.timeout_seconds));
    curl_easy_setopt(m_impl->curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(m_impl->curl, CURLOPT_USERAGENT, "AstroCore/0.1.0");

    CURLcode res = curl_easy_perform(m_impl->curl);

    if (res != CURLE_OK) {
        LOG_WARN("ExoMAST: Request failed for {}: {}", planetName, curl_easy_strerror(res));
        return std::nullopt;
    }

    long httpCode = 0;
    curl_easy_getinfo(m_impl->curl, CURLINFO_RESPONSE_CODE, &httpCode);

    if (httpCode == 404) {
        LOG_DEBUG("ExoMAST: No spectral data for {}", planetName);
        return std::nullopt;
    }

    if (httpCode != 200) {
        LOG_WARN("ExoMAST: HTTP {} for {}", httpCode, planetName);
        return std::nullopt;
    }

    try {
        return parseAtmosphereResponse(m_impl->responseBuffer);
    } catch (const std::exception& e) {
        LOG_ERROR("ExoMAST: Failed to parse response for {}: {}", planetName, e.what());
        return std::nullopt;
    }
}

AtmosphericData ExoMastClient::parseAtmosphereResponse(const std::string& jsonStr) const {
    AtmosphericData data;

    auto json = nlohmann::json::parse(jsonStr);

    // Check if we have spectra
    if (!json.is_array() || json.empty()) {
        return data;
    }

    // Parse available spectra
    for (const auto& spectrum : json) {
        SpectralObservation obs;

        if (spectrum.contains("planet_name")) {
            data.planet_name = spectrum["planet_name"].get<std::string>();
            obs.planet_name = data.planet_name;
        }

        if (spectrum.contains("spectrum_type")) {
            obs.observation_type = spectrum["spectrum_type"].get<std::string>();
            if (obs.observation_type == "transmission") {
                data.has_transmission_spectrum = true;
            } else if (obs.observation_type == "emission") {
                data.has_emission_spectrum = true;
            }
        }

        if (spectrum.contains("instrument")) {
            obs.instrument = spectrum["instrument"].get<std::string>();
            data.best_instrument = obs.instrument;  // Last one wins, could be improved
        }

        if (spectrum.contains("wavelength_min")) {
            obs.wavelength_min_um = spectrum["wavelength_min"].get<double>();
        }
        if (spectrum.contains("wavelength_max")) {
            obs.wavelength_max_um = spectrum["wavelength_max"].get<double>();
        }

        // Parse detected species if present
        if (spectrum.contains("detected_species") && spectrum["detected_species"].is_array()) {
            for (const auto& species : spectrum["detected_species"]) {
                AtmosphericDetection det;
                if (species.is_string()) {
                    det.molecule = species.get<std::string>();
                } else if (species.is_object()) {
                    if (species.contains("molecule")) {
                        det.molecule = species["molecule"].get<std::string>();
                    }
                    if (species.contains("abundance")) {
                        det.abundance = species["abundance"].get<double>();
                    }
                    if (species.contains("significance")) {
                        det.detection_significance = species["significance"].get<double>();
                    }
                }
                det.instrument = obs.instrument;
                obs.detections.push_back(det);
                data.detections.push_back(det);
            }
        }

        data.observations.push_back(obs);
        data.num_observations++;
    }

    // Log what we found
    if (!data.detections.empty()) {
        std::string molecules;
        for (const auto& det : data.detections) {
            if (!molecules.empty()) molecules += ", ";
            molecules += det.molecule;
        }
        LOG_INFO("ExoMAST: Found atmospheric detections for {}: {}", data.planet_name, molecules);
    }

    return data;
}

std::vector<AtmosphericDetection> ExoMastClient::parseDetections(const std::string& jsonStr) const {
    std::vector<AtmosphericDetection> detections;

    try {
        auto json = nlohmann::json::parse(jsonStr);

        if (json.is_array()) {
            for (const auto& item : json) {
                AtmosphericDetection det;
                if (item.contains("molecule")) {
                    det.molecule = item["molecule"].get<std::string>();
                }
                if (item.contains("abundance")) {
                    det.abundance = item["abundance"].get<double>();
                }
                if (item.contains("detection_significance")) {
                    det.detection_significance = item["detection_significance"].get<double>();
                }
                if (item.contains("instrument")) {
                    det.instrument = item["instrument"].get<std::string>();
                }
                if (item.contains("reference")) {
                    det.reference = item["reference"].get<std::string>();
                }
                if (item.contains("is_upper_limit")) {
                    det.is_upper_limit = item["is_upper_limit"].get<bool>();
                }
                detections.push_back(det);
            }
        }
    } catch (const std::exception& e) {
        LOG_WARN("ExoMAST: Failed to parse detections: {}", e.what());
    }

    return detections;
}

std::future<std::vector<std::string>> ExoMastClient::getPlanetsWithSpectra() {
    return std::async(std::launch::async, [this]() {
        std::vector<std::string> planets;

        if (!m_impl->curl) return planets;

        m_impl->responseBuffer.clear();

        std::string url = m_impl->config.api_endpoint + "/exoplanets/";

        curl_easy_setopt(m_impl->curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(m_impl->curl, CURLOPT_WRITEFUNCTION, Impl::writeCallback);
        curl_easy_setopt(m_impl->curl, CURLOPT_WRITEDATA, m_impl.get());
        curl_easy_setopt(m_impl->curl, CURLOPT_TIMEOUT, 60L);
        curl_easy_setopt(m_impl->curl, CURLOPT_USERAGENT, "AstroCore/0.1.0");

        CURLcode res = curl_easy_perform(m_impl->curl);
        if (res != CURLE_OK) return planets;

        try {
            auto json = nlohmann::json::parse(m_impl->responseBuffer);
            if (json.is_array()) {
                for (const auto& item : json) {
                    if (item.contains("planet_name")) {
                        planets.push_back(item["planet_name"].get<std::string>());
                    }
                }
            }
        } catch (...) {
            LOG_WARN("ExoMAST: Failed to parse planets list");
        }

        LOG_INFO("ExoMAST: Found {} planets with spectral data", planets.size());
        return planets;
    });
}

bool ExoMastClient::hasSpectralData(const std::string& planetName) {
    auto result = queryAtmosphereSync(planetName);
    return result.has_value() && result->num_observations > 0;
}

bool ExoMastClient::testConnection() {
    if (!m_impl->curl) return false;

    m_impl->responseBuffer.clear();

    curl_easy_setopt(m_impl->curl, CURLOPT_URL, m_impl->config.api_endpoint.c_str());
    curl_easy_setopt(m_impl->curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(m_impl->curl, CURLOPT_TIMEOUT, 5L);

    CURLcode res = curl_easy_perform(m_impl->curl);
    curl_easy_setopt(m_impl->curl, CURLOPT_NOBODY, 0L);

    return res == CURLE_OK;
}

}  // namespace astrocore
