#pragma once

#include <string>
#include <future>
#include <memory>
#include <optional>
#include <nlohmann/json.hpp>

namespace astrocore {

struct BedrockConfig {
    std::string region = "us-east-1";
    // Claude 4.5 Sonnet on Bedrock (inference profile)
    std::string model_id = "us.anthropic.claude-sonnet-4-5-20250929-v1:0";
    int max_tokens = 4096;
    double temperature = 0.7;  // Higher for more varied/creative planet generation
    std::string access_key_id;      // If empty, read from environment
    std::string secret_access_key;  // If empty, read from environment
};

struct InferenceRequest {
    std::string system_prompt;
    std::string user_prompt;
};

struct InferenceResponse {
    bool success = false;
    nlohmann::json inferred_values;
    std::string reasoning;
    std::vector<std::string> assumptions;
    std::string raw_response;
    std::string error_message;
    double latency_ms = 0.0;
};

class BedrockClient {
public:
    explicit BedrockClient(const BedrockConfig& config = {});
    ~BedrockClient();

    // Prevent copying
    BedrockClient(const BedrockClient&) = delete;
    BedrockClient& operator=(const BedrockClient&) = delete;

    // Async inference
    std::future<InferenceResponse> infer(const InferenceRequest& request);

    // Synchronous inference
    InferenceResponse inferSync(const InferenceRequest& request);

    // Check if credentials are configured
    bool hasValidCredentials() const;

    // Test connection to Bedrock
    bool testConnection();

    // Set model ID (for switching between Sonnet/Haiku/etc)
    void setModelId(const std::string& modelId);
    const std::string& getModelId() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    // Build the API request body
    std::string buildRequestBody(const InferenceRequest& request) const;

    // Parse Claude's response
    InferenceResponse parseResponse(const std::string& response) const;
};

}  // namespace astrocore
