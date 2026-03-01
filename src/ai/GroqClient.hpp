#pragma once

#include <string>
#include <optional>
#include <memory>

namespace astrocore {

enum class GroqModel {
    KIMI_K2,           // moonshotai/kimi-k2-instruct-0905
    LLAMA_70B,         // llama-3.3-70b-versatile
    LLAMA_8B,          // llama-3.1-8b-instant
    MIXTRAL            // mixtral-8x7b-32768
};

struct GroqConfig {
    std::string api_key;
    GroqModel model = GroqModel::KIMI_K2;
    int max_tokens = 4096;
    double temperature = 0.7;
};

class GroqClient {
public:
    explicit GroqClient(const GroqConfig& config);
    ~GroqClient();

    // Query the model
    std::optional<std::string> query(const std::string& systemPrompt,
                                     const std::string& userPrompt);

    // Change model
    void setModel(GroqModel model);
    GroqModel getModel() const { return m_model; }

    // Get model string for API
    static std::string modelToString(GroqModel model);

private:
    std::string m_apiKey;
    GroqModel m_model;
    int m_maxTokens;
    double m_temperature;
};

}  // namespace astrocore
