#include "ai/GroqClient.hpp"
#include "core/Logger.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <chrono>

namespace astrocore {

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

GroqClient::GroqClient(const GroqConfig& config)
    : m_apiKey(config.api_key)
    , m_model(config.model)
    , m_maxTokens(config.max_tokens)
    , m_temperature(config.temperature)
{
    LOG_INFO("Groq client initialized with model: {}", modelToString(m_model));
}

GroqClient::~GroqClient() = default;

std::string GroqClient::modelToString(GroqModel model) {
    switch (model) {
        case GroqModel::KIMI_K2: return "moonshotai/kimi-k2-instruct-0905";
        case GroqModel::LLAMA_70B: return "llama-3.3-70b-versatile";
        case GroqModel::LLAMA_8B: return "llama-3.1-8b-instant";
        case GroqModel::MIXTRAL: return "mixtral-8x7b-32768";
        default: return "moonshotai/kimi-k2-instruct-0905";
    }
}

void GroqClient::setModel(GroqModel model) {
    m_model = model;
    LOG_INFO("Groq model changed to: {}", modelToString(model));
}

std::optional<std::string> GroqClient::query(const std::string& systemPrompt,
                                              const std::string& userPrompt) {
    auto startTime = std::chrono::high_resolution_clock::now();

    CURL* curl = curl_easy_init();
    if (!curl) {
        LOG_ERROR("Failed to initialize CURL for Groq");
        return std::nullopt;
    }

    // Build request JSON (OpenAI-compatible format)
    nlohmann::json requestBody;
    requestBody["model"] = modelToString(m_model);
    requestBody["max_tokens"] = m_maxTokens;
    requestBody["temperature"] = m_temperature;
    requestBody["seed"] = 42;  // Fixed seed for reproducible outputs
    requestBody["messages"] = nlohmann::json::array({
        {{"role", "system"}, {"content", systemPrompt}},
        {{"role", "user"}, {"content", userPrompt}}
    });

    std::string requestStr = requestBody.dump();
    std::string response;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    std::string authHeader = "Authorization: Bearer " + m_apiKey;
    headers = curl_slist_append(headers, authHeader.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, "https://api.groq.com/openai/v1/chat/completions");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestStr.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode res = curl_easy_perform(curl);

    auto endTime = std::chrono::high_resolution_clock::now();
    double latencyMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        LOG_ERROR("Groq request failed: {}", curl_easy_strerror(res));
        return std::nullopt;
    }

    // Parse response
    try {
        auto json = nlohmann::json::parse(response);

        // Check for error
        if (json.contains("error")) {
            std::string errMsg = json["error"].value("message", "Unknown error");
            LOG_ERROR("Groq API error: {}", errMsg);
            return std::nullopt;
        }

        // Extract content from OpenAI-compatible response
        if (json.contains("choices") && json["choices"].is_array() && !json["choices"].empty()) {
            auto& choice = json["choices"][0];
            if (choice.contains("message") && choice["message"].contains("content")) {
                std::string content = choice["message"]["content"].get<std::string>();
                LOG_INFO("Groq inference completed in {:.0f}ms ({} tokens)",
                         latencyMs,
                         json.value("usage", nlohmann::json{}).value("total_tokens", 0));
                return content;
            }
        }

        LOG_ERROR("Groq response missing expected fields");
        return std::nullopt;

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse Groq response: {}", e.what());
        LOG_DEBUG("Raw response: {}", response.substr(0, 500));
        return std::nullopt;
    }
}

}  // namespace astrocore
