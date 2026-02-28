#pragma once

#include "data/ExoplanetData.hpp"
#include <memory>
#include <future>

namespace astrocore {

class BedrockClient;

class InferenceEngine {
public:
    InferenceEngine();
    ~InferenceEngine();

    // Check if AI inference is available
    bool isAvailable() const;

    // Fill missing parameters using AI inference
    std::future<ExoplanetData> fillMissingParameters(ExoplanetData data);
    ExoplanetData fillMissingParametersSync(ExoplanetData data);

    // Infer specific categories
    void inferAtmosphere(ExoplanetData& data);
    void inferRenderHints(ExoplanetData& data);

private:
    std::unique_ptr<BedrockClient> m_bedrock;

    // Apply inferred values to data structure
    void applyInferredValues(ExoplanetData& data, const nlohmann::json& values, const std::string& reasoning);
};

}  // namespace astrocore
