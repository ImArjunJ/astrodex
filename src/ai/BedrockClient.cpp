#include "ai/BedrockClient.hpp"
#include "core/Logger.hpp"
#include <curl/curl.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cstdlib>
#include <fstream>
#include <cstdio>

namespace astrocore {

struct BedrockClient::Impl {
    BedrockConfig config;

    bool checkAwsCli() {
        // Check if AWS CLI is available
        FILE* pipe = popen("aws --version 2>&1", "r");
        if (!pipe) return false;
        char buffer[128];
        std::string result;
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        int exitCode = pclose(pipe);
        return exitCode == 0 && result.find("aws-cli") != std::string::npos;
    }
};

BedrockClient::BedrockClient(const BedrockConfig& config)
    : m_impl(std::make_unique<Impl>())
{
    m_impl->config = config;

    if (m_impl->checkAwsCli()) {
        LOG_INFO("AWS CLI available for Bedrock inference");
    } else {
        LOG_WARN("AWS CLI not found. AI inference will be disabled.");
    }
}

BedrockClient::~BedrockClient() = default;

bool BedrockClient::hasValidCredentials() const {
    return m_impl->checkAwsCli();
}

std::string BedrockClient::buildRequestBody(const InferenceRequest& request) const {
    nlohmann::json body;

    body["anthropic_version"] = "bedrock-2023-05-31";
    body["max_tokens"] = m_impl->config.max_tokens;
    body["temperature"] = m_impl->config.temperature;

    if (!request.system_prompt.empty()) {
        body["system"] = request.system_prompt;
    }

    body["messages"] = nlohmann::json::array({
        {{"role", "user"}, {"content", request.user_prompt}}
    });

    return body.dump();
}

InferenceResponse BedrockClient::parseResponse(const std::string& response) const {
    InferenceResponse result;
    result.raw_response = response;

    try {
        auto json = nlohmann::json::parse(response);

        // Check for error
        if (json.contains("error")) {
            result.success = false;
            result.error_message = json["error"].value("message", "Unknown error");
            return result;
        }

        // Extract content from Claude's response
        if (json.contains("content") && json["content"].is_array() && !json["content"].empty()) {
            std::string text = json["content"][0].value("text", "");

            // Strip markdown code blocks if present
            if (text.find("```json") != std::string::npos) {
                size_t start = text.find("```json") + 7;
                size_t end = text.find("```", start);
                if (end != std::string::npos) {
                    text = text.substr(start, end - start);
                }
            } else if (text.find("```") != std::string::npos) {
                size_t start = text.find("```") + 3;
                size_t end = text.find("```", start);
                if (end != std::string::npos) {
                    text = text.substr(start, end - start);
                }
            }

            // Trim whitespace
            while (!text.empty() && (text.front() == ' ' || text.front() == '\n' || text.front() == '\r')) {
                text.erase(0, 1);
            }
            while (!text.empty() && (text.back() == ' ' || text.back() == '\n' || text.back() == '\r')) {
                text.pop_back();
            }

            // Try to parse the text as JSON (Claude should return JSON)
            try {
                auto contentJson = nlohmann::json::parse(text);

                if (contentJson.contains("inferred_values")) {
                    result.inferred_values = contentJson["inferred_values"];
                }
                if (contentJson.contains("reasoning")) {
                    result.reasoning = contentJson["reasoning"].get<std::string>();
                }
                if (contentJson.contains("assumptions")) {
                    for (const auto& a : contentJson["assumptions"]) {
                        result.assumptions.push_back(a.get<std::string>());
                    }
                }

                result.success = true;
            } catch (const nlohmann::json::exception&) {
                // Claude didn't return valid JSON, try to extract anyway
                result.reasoning = text;
                result.success = false;
                result.error_message = "Claude response was not valid JSON";
            }
        }
    } catch (const nlohmann::json::exception& e) {
        result.success = false;
        result.error_message = std::string("Failed to parse Bedrock response: ") + e.what();
    }

    return result;
}

InferenceResponse BedrockClient::inferSync(const InferenceRequest& request) {
    InferenceResponse result;

    auto startTime = std::chrono::high_resolution_clock::now();

    // Build request body
    std::string payload = buildRequestBody(request);

    // Write payload to temp file
    std::string tempFile = "/tmp/bedrock_request.json";
    std::string outputFile = "/tmp/bedrock_response.json";
    {
        std::ofstream out(tempFile);
        out << payload;
    }

    // Use AWS CLI which handles SigV4 signing correctly
    std::string cmd = "aws bedrock-runtime invoke-model "
                      "--model-id '" + m_impl->config.model_id + "' "
                      "--region " + m_impl->config.region + " "
                      "--body fileb://" + tempFile + " "
                      "--content-type application/json "
                      "--accept application/json "
                      + outputFile + " 2>&1";

    LOG_DEBUG("Bedrock CLI command: aws bedrock-runtime invoke-model --model-id '{}'", m_impl->config.model_id);

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        result.success = false;
        result.error_message = "Failed to execute AWS CLI";
        return result;
    }

    char buffer[256];
    std::string cmdOutput;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        cmdOutput += buffer;
    }
    int exitCode = pclose(pipe);

    auto endTime = std::chrono::high_resolution_clock::now();
    result.latency_ms = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    if (exitCode != 0) {
        result.success = false;
        result.error_message = "AWS CLI error: " + cmdOutput;
        LOG_ERROR("Bedrock CLI failed: {}", cmdOutput.substr(0, 300));
        return result;
    }

    // Read response from output file
    std::ifstream responseFile(outputFile);
    if (!responseFile.is_open()) {
        result.success = false;
        result.error_message = "Failed to read response file";
        return result;
    }

    std::stringstream responseBuffer;
    responseBuffer << responseFile.rdbuf();
    std::string response = responseBuffer.str();

    // Clean up temp files
    std::remove(tempFile.c_str());
    std::remove(outputFile.c_str());

    result = parseResponse(response);
    result.latency_ms = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    if (result.success) {
        LOG_INFO("Bedrock inference completed in {:.0f}ms", result.latency_ms);
    }

    return result;
}

std::future<InferenceResponse> BedrockClient::infer(const InferenceRequest& request) {
    return std::async(std::launch::async, [this, request]() {
        return inferSync(request);
    });
}

bool BedrockClient::testConnection() {
    return hasValidCredentials();
}

void BedrockClient::setModelId(const std::string& modelId) {
    m_impl->config.model_id = modelId;
    LOG_DEBUG("Bedrock model ID set to: {}", modelId);
}

const std::string& BedrockClient::getModelId() const {
    return m_impl->config.model_id;
}

}  // namespace astrocore
