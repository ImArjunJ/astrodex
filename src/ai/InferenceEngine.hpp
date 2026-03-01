#pragma once

#include "data/ExoplanetData.hpp"
#include <memory>
#include <future>
#include <optional>
#include <string>
#include <vector>
#include <unordered_map>

namespace astrocore {

class BedrockClient;
class JimmyClient;
class GroqClient;
struct PlanetParams;

// Available inference backends
enum class InferenceBackend {
    AWS_BEDROCK,        // Claude Sonnet via AWS Bedrock (slower but highest quality)
    AWS_BEDROCK_HAIKU,  // Claude Haiku via AWS Bedrock (fast, good quality)
    GROQ_KIMI_K2,       // Kimi K2 via Groq (very fast, good quality)
    JIMMY_QWEN_72B,     // Qwen 72B via chatjimmy.ai (fast, good quality)
    JIMMY_LLAMA_70B,    // Llama 70B via chatjimmy.ai (fast)
    JIMMY_LLAMA_8B,     // Llama 8B via chatjimmy.ai (fastest, lower quality)
    NONE                // No inference available
};

class InferenceEngine {
public:
    InferenceEngine();
    ~InferenceEngine();

    // Check if AI inference is available
    bool isAvailable() const;

    // Get/set current backend
    InferenceBackend getBackend() const { return m_currentBackend; }
    void setBackend(InferenceBackend backend);

    // Get available backends (based on what's configured/reachable)
    std::vector<InferenceBackend> getAvailableBackends() const;

    // Get human-readable name for backend
    static std::string backendToString(InferenceBackend backend);

    // Fill missing parameters using AI inference
    std::future<ExoplanetData> fillMissingParameters(ExoplanetData data);
    ExoplanetData fillMissingParametersSync(ExoplanetData data);

    // Infer specific categories
    void inferAtmosphere(ExoplanetData& data);
    void inferRenderHints(ExoplanetData& data);

    // Generate full rendering parameters from exoplanet data using AI
    std::optional<PlanetParams> generateRenderParams(const ExoplanetData& data);
    std::future<std::optional<PlanetParams>> generateRenderParamsAsync(const ExoplanetData& data);

private:
    std::unique_ptr<BedrockClient> m_bedrock;
    std::unique_ptr<JimmyClient> m_jimmy;
    std::unique_ptr<GroqClient> m_groq;
    InferenceBackend m_currentBackend = InferenceBackend::NONE;
    bool m_bedrockAvailable = false;
    bool m_jimmyAvailable = false;
    bool m_groqAvailable = false;

    // Send query to current backend
    std::optional<std::string> queryBackend(const std::string& systemPrompt,
                                             const std::string& userPrompt);

    // Apply inferred values to data structure
    void applyInferredValues(ExoplanetData& data, const nlohmann::json& values, const std::string& reasoning);

    // Parse AI response JSON into PlanetParams
    std::optional<PlanetParams> parseRenderParamsJson(const std::string& json);
};

}  // namespace astrocore
