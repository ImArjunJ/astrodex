#pragma once

#include <string>
#include <optional>
#include <memory>

namespace astrocore {

enum class JimmyModel {
    LLAMA_8B,
    LLAMA_70B,
    QWEN_72B,
    MISTRAL_7B
};

struct JimmyConfig {
    std::string endpoint = "https://chatjimmy.ai/api/chat";
    JimmyModel model = JimmyModel::QWEN_72B;  // Best for scientific reasoning
    int top_k = 8;
    int timeout_seconds = 30;
};

class JimmyClient {
public:
    explicit JimmyClient(const JimmyConfig& config = {});
    ~JimmyClient();

    JimmyClient(const JimmyClient&) = delete;
    JimmyClient& operator=(const JimmyClient&) = delete;

    // Send a prompt and get response
    std::optional<std::string> query(const std::string& systemPrompt,
                                      const std::string& userPrompt);

    // Test if the API is reachable
    bool testConnection();

    // Get model name string
    static std::string modelToString(JimmyModel model);

    // Set model
    void setModel(JimmyModel model) { m_config.model = model; }
    JimmyModel getModel() const { return m_config.model; }

private:
    JimmyConfig m_config;
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    std::string buildRequestJson(const std::string& systemPrompt,
                                  const std::string& userPrompt) const;
    std::optional<std::string> parseResponse(const std::string& response) const;
};

}  // namespace astrocore
