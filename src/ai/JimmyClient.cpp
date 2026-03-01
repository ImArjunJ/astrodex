#include "ai/JimmyClient.hpp"
#include "core/Logger.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <chrono>

namespace astrocore {

struct JimmyClient::Impl {
    CURL* curl = nullptr;
    std::string responseBuffer;

    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        auto* self = static_cast<Impl*>(userp);
        self->responseBuffer.append(static_cast<char*>(contents), size * nmemb);
        return size * nmemb;
    }
};

JimmyClient::JimmyClient(const JimmyConfig& config)
    : m_config(config), m_impl(std::make_unique<Impl>())
{
    m_impl->curl = curl_easy_init();
    if (!m_impl->curl) {
        LOG_ERROR("JimmyClient: Failed to initialize CURL");
    }
}

JimmyClient::~JimmyClient() {
    if (m_impl->curl) {
        curl_easy_cleanup(m_impl->curl);
    }
}

std::string JimmyClient::modelToString(JimmyModel model) {
    switch (model) {
        case JimmyModel::LLAMA_8B: return "llama3.1-8B";
        case JimmyModel::LLAMA_70B: return "llama3.1-70B";
        case JimmyModel::QWEN_72B: return "qwen2.5-72B";
        case JimmyModel::MISTRAL_7B: return "mistral-7B";
        default: return "qwen2.5-72B";
    }
}

std::string JimmyClient::buildRequestJson(const std::string& systemPrompt,
                                           const std::string& userPrompt) const {
    nlohmann::json request;
    request["messages"] = nlohmann::json::array();
    request["messages"].push_back({
        {"role", "user"},
        {"content", userPrompt}
    });
    request["chatOptions"] = {
        {"selectedModel", modelToString(m_config.model)},
        {"systemPrompt", systemPrompt},
        {"topK", m_config.top_k}
    };
    request["attachment"] = nullptr;
    return request.dump();
}

std::optional<std::string> JimmyClient::parseResponse(const std::string& response) const {
    // Response format: "actual response text<|stats|>{...}<|/stats|>"
    // We need to extract everything before <|stats|>

    size_t statsPos = response.find("<|stats|>");
    if (statsPos != std::string::npos) {
        std::string text = response.substr(0, statsPos);
        // Trim whitespace
        while (!text.empty() && std::isspace(text.back())) {
            text.pop_back();
        }
        return text;
    }

    // No stats marker, return as-is
    return response;
}

std::optional<std::string> JimmyClient::query(const std::string& systemPrompt,
                                               const std::string& userPrompt) {
    if (!m_impl->curl) {
        LOG_ERROR("JimmyClient: CURL not initialized");
        return std::nullopt;
    }

    auto startTime = std::chrono::steady_clock::now();

    m_impl->responseBuffer.clear();
    std::string requestJson = buildRequestJson(systemPrompt, userPrompt);

    // Set up headers
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: */*");
    headers = curl_slist_append(headers, "Origin: https://chatjimmy.ai");
    headers = curl_slist_append(headers, "User-Agent: AstroSplat/0.1.0");

    curl_easy_setopt(m_impl->curl, CURLOPT_URL, m_config.endpoint.c_str());
    curl_easy_setopt(m_impl->curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(m_impl->curl, CURLOPT_POSTFIELDS, requestJson.c_str());
    curl_easy_setopt(m_impl->curl, CURLOPT_WRITEFUNCTION, Impl::writeCallback);
    curl_easy_setopt(m_impl->curl, CURLOPT_WRITEDATA, m_impl.get());
    curl_easy_setopt(m_impl->curl, CURLOPT_TIMEOUT, static_cast<long>(m_config.timeout_seconds));

    CURLcode res = curl_easy_perform(m_impl->curl);
    curl_slist_free_all(headers);

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    if (res != CURLE_OK) {
        LOG_ERROR("JimmyClient: Request failed: {}", curl_easy_strerror(res));
        return std::nullopt;
    }

    long httpCode = 0;
    curl_easy_getinfo(m_impl->curl, CURLINFO_RESPONSE_CODE, &httpCode);

    if (httpCode != 200) {
        LOG_ERROR("JimmyClient: HTTP {}", httpCode);
        LOG_DEBUG("JimmyClient: Response: {}", m_impl->responseBuffer.substr(0, 500));
        return std::nullopt;
    }

    auto result = parseResponse(m_impl->responseBuffer);
    if (result.has_value()) {
        LOG_INFO("JimmyClient: Response received in {}ms ({} model)",
                 duration.count(), modelToString(m_config.model));
    }

    return result;
}

bool JimmyClient::testConnection() {
    auto result = query("You are a test assistant.", "Say 'ok' if you can hear me.");
    return result.has_value();
}

}  // namespace astrocore
