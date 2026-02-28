#include "ai/InferenceEngine.hpp"
#include "ai/BedrockClient.hpp"
#include "ai/PromptTemplates.hpp"
#include "core/Logger.hpp"
#include <thread>
#include <chrono>

namespace astrocore {

InferenceEngine::InferenceEngine()
    : m_bedrock(std::make_unique<BedrockClient>())
{
    if (m_bedrock->hasValidCredentials()) {
        LOG_INFO("AI Inference Engine initialized with AWS Bedrock");
    } else {
        LOG_WARN("AI Inference Engine: No AWS credentials, AI features disabled");
    }
}

InferenceEngine::~InferenceEngine() = default;

bool InferenceEngine::isAvailable() const {
    return m_bedrock && m_bedrock->hasValidCredentials();
}

void InferenceEngine::applyInferredValues(ExoplanetData& data,
                                          const nlohmann::json& values,
                                          const std::string& reasoning) {
    auto applyDouble = [&values, &reasoning](MeasuredValue<double>& target,
                                             const std::string& key) {
        if (target.hasValue()) return;  // CRITICAL: never overwrite measured data
        if (values.contains(key)) {
            const auto& v = values[key];
            if (v.contains("value")) {
                target.value = v["value"].get<double>();
                target.source = DataSource::AI_INFERRED;
                target.ai_reasoning = reasoning;

                // Set confidence based on the AI's stated confidence
                if (v.contains("confidence")) {
                    std::string conf = v["confidence"].get<std::string>();
                    if (conf == "high") target.confidence = 0.9f;
                    else if (conf == "medium") target.confidence = 0.7f;
                    else target.confidence = 0.5f;
                }
            }
        }
    };

    auto applyString = [&values, &reasoning](MeasuredValue<std::string>& target,
                                             const std::string& key) {
        if (!target.value.empty()) return;  // CRITICAL: never overwrite existing data
        if (values.contains(key)) {
            const auto& v = values[key];
            if (v.contains("value")) {
                if (v["value"].is_string()) {
                    target.value = v["value"].get<std::string>();
                } else {
                    target.value = v["value"].dump();  // Convert JSON to string
                }
                target.source = DataSource::AI_INFERRED;
                target.ai_reasoning = reasoning;
            }
        }
    };

    // Apply atmospheric values
    applyDouble(data.surface_pressure_atm, "surface_pressure_atm");
    applyDouble(data.albedo, "albedo");
    applyDouble(data.ocean_coverage_fraction, "ocean_coverage_fraction");
    applyDouble(data.cloud_coverage_fraction, "cloud_coverage_fraction");
    applyString(data.atmosphere_composition, "atmosphere_composition");

    // Apply render hints
    applyString(data.biome_classification, "biome_classification");
    applyString(data.surface_color_hint, "surface_color_hint");
}

void InferenceEngine::inferAtmosphere(ExoplanetData& data) {
    if (!isAvailable()) {
        LOG_DEBUG("AI inference not available, skipping atmosphere inference");
        return;
    }

    LOG_INFO("Inferring atmosphere for {}", data.name);

    InferenceRequest request;
    request.system_prompt = std::string(prompts::SYSTEM_PROMPT);
    request.user_prompt = prompts::buildAtmospherePrompt(data);

    auto response = m_bedrock->inferSync(request);

    // One retry on transient failure before giving up
    if (!response.success) {
        LOG_INFO("Retrying atmosphere inference for {} after transient failure", data.name);
        std::this_thread::sleep_for(std::chrono::seconds(2));
        response = m_bedrock->inferSync(request);
    }

    if (response.success) {
        applyInferredValues(data, response.inferred_values, response.reasoning);
        LOG_INFO("Atmosphere inference complete for {} ({:.0f}ms)",
                 data.name, response.latency_ms);
    } else {
        LOG_WARN("Atmosphere inference failed for {}: {}",
                 data.name, response.error_message);
    }
}

void InferenceEngine::inferRenderHints(ExoplanetData& data) {
    if (!isAvailable()) {
        return;
    }

    LOG_INFO("Inferring render hints for {}", data.name);

    InferenceRequest request;
    request.system_prompt = std::string(prompts::SYSTEM_PROMPT);
    request.user_prompt = prompts::buildRenderHintsPrompt(data);

    auto response = m_bedrock->inferSync(request);

    // One retry on transient failure before giving up
    if (!response.success) {
        LOG_INFO("Retrying render hints inference for {} after transient failure", data.name);
        std::this_thread::sleep_for(std::chrono::seconds(2));
        response = m_bedrock->inferSync(request);
    }

    if (response.success) {
        applyInferredValues(data, response.inferred_values, response.reasoning);
        LOG_INFO("Render hints inference complete for {}", data.name);
    } else {
        LOG_WARN("Render hints inference failed: {}", response.error_message);
    }
}

ExoplanetData InferenceEngine::fillMissingParametersSync(ExoplanetData data) {
    if (!isAvailable()) {
        return data;
    }

    // Infer atmosphere if missing key parameters.
    // Visual render parameters are handled separately by inferRenderParamsSync,
    // which uses the structured render-params prompt.
    if (!data.surface_pressure_atm.hasValue() ||
        !data.albedo.hasValue() ||
        !data.atmosphere_composition.hasValue()) {
        inferAtmosphere(data);
    }

    return data;
}

std::future<ExoplanetData> InferenceEngine::fillMissingParameters(ExoplanetData data) {
    return std::async(std::launch::async, [this, data = std::move(data)]() mutable {
        return fillMissingParametersSync(std::move(data));
    });
}

nlohmann::json InferenceEngine::inferRenderParamsSync(const ExoplanetData&         data,
                                                       const std::string&           analogContext,
                                                       const std::set<std::string>& skipFields) {
    if (!isAvailable()) {
        LOG_DEBUG("AI inference not available, skipping render params");
        return {};
    }

    LOG_INFO("Inferring render params for {}", data.name);

    InferenceRequest request;
    request.system_prompt = std::string(prompts::RENDER_PARAMS_SYSTEM_PROMPT);
    request.user_prompt   = prompts::buildRenderParamsPrompt(data, analogContext, skipFields);

    auto response = m_bedrock->inferSync(request);

    if (!response.success) {
        LOG_WARN("Render params inference failed for {}: {}", data.name, response.error_message);
        return {};
    }

    if (response.inferred_values.empty()) {
        LOG_WARN("Render params response was empty for {}", data.name);
        return {};
    }

    LOG_INFO("Render params inferred for {} ({:.0f}ms)", data.name, response.latency_ms);
    return response.inferred_values;
}

std::future<nlohmann::json> InferenceEngine::inferRenderParams(ExoplanetData        data,
                                                                std::string          analogContext,
                                                                std::set<std::string> skipFields) {
    return std::async(std::launch::async,
        [this, data = std::move(data),
               analogContext = std::move(analogContext),
               skipFields    = std::move(skipFields)]() {
            return inferRenderParamsSync(data, analogContext, skipFields);
        });
}

}  // namespace astrocore
