#pragma once

#include "data/ExoplanetData.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <future>
#include <set>
#include <string>

namespace astrocore {

class BedrockClient;

class InferenceEngine {
public:
    InferenceEngine();
    ~InferenceEngine();

    // Check if AI inference is available
    bool isAvailable() const;

    // Fill missing ExoplanetData parameters using AI inference
    std::future<ExoplanetData> fillMissingParameters(ExoplanetData data);
    ExoplanetData fillMissingParametersSync(ExoplanetData data);

    // Infer specific categories of ExoplanetData fields
    void inferAtmosphere(ExoplanetData& data);
    void inferRenderHints(ExoplanetData& data);

    // Ask Claude for numeric PlanetParams overrides keyed by field name.
    // Only fields NOT in skipFields will be requested (fill-empty-slots philosophy).
    // analogContext: description of closest solar-system analog for Claude's reference.
    // Returns a JSON object ready to pass to ExoplanetMapper::applyAIRenderOverrides().
    // Returns an empty object if AI is unavailable or the call fails.
    nlohmann::json inferRenderParamsSync(const ExoplanetData&         data,
                                         const std::string&           analogContext = "",
                                         const std::set<std::string>& skipFields    = {});
    std::future<nlohmann::json> inferRenderParams(ExoplanetData        data,
                                                   std::string          analogContext = "",
                                                   std::set<std::string> skipFields   = {});

private:
    std::unique_ptr<BedrockClient> m_bedrock;

    void applyInferredValues(ExoplanetData& data,
                             const nlohmann::json& values,
                             const std::string& reasoning);
};

}  // namespace astrocore
