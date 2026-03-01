#include "ai/InferenceEngine.hpp"
#include "ai/BedrockClient.hpp"
#include "ai/JimmyClient.hpp"
#include "ai/GroqClient.hpp"
#include "ai/PromptTemplates.hpp"
#include "render/Renderer.hpp"
#include "core/Logger.hpp"
#include <cstdlib>

namespace astrocore {

// Helper to extract JSON from AI responses that might contain markdown code blocks
static std::string extractJsonFromResponse(const std::string& response) {
    std::string text = response;

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
    while (!text.empty() && (text.front() == ' ' || text.front() == '\n' || text.front() == '\r' || text.front() == '\t')) {
        text.erase(0, 1);
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\n' || text.back() == '\r' || text.back() == '\t')) {
        text.pop_back();
    }

    // Find JSON object boundaries
    auto start = text.find('{');
    auto end = text.rfind('}');
    if (start != std::string::npos && end != std::string::npos && end > start) {
        text = text.substr(start, end - start + 1);
    }

    return text;
}

InferenceEngine::InferenceEngine() {
    // Try to initialize Bedrock
    m_bedrock = std::make_unique<BedrockClient>();
    if (m_bedrock->hasValidCredentials()) {
        m_bedrockAvailable = true;
        LOG_INFO("AWS Bedrock available for inference");
    }

    // Try to initialize Jimmy (chatjimmy.ai)
    JimmyConfig jimmyConfig;
    jimmyConfig.model = JimmyModel::QWEN_72B;  // Best for scientific reasoning
    m_jimmy = std::make_unique<JimmyClient>(jimmyConfig);

    // Quick test if Jimmy is reachable (don't block startup, assume it works)
    m_jimmyAvailable = true;  // Assume available, will fail gracefully if not
    LOG_INFO("ChatJimmy.ai configured for fast inference (Qwen 72B)");

    // Initialize Groq (Kimi K2)
    const char* groqKey = std::getenv("GROQ_API_KEY");
    if (groqKey && groqKey[0] != '\0') {
        GroqConfig groqConfig;
        groqConfig.api_key = groqKey;
        groqConfig.model = GroqModel::KIMI_K2;
        groqConfig.temperature = 0.0;  // Fully deterministic output
        m_groq = std::make_unique<GroqClient>(groqConfig);
        m_groqAvailable = true;
        LOG_INFO("Groq configured for fast inference (Kimi K2, temp=0)");
    } else {
        m_groqAvailable = false;
        LOG_WARN("GROQ_API_KEY not set - Groq inference disabled");
    }

    // Select best available backend - prefer Groq (fastest with good quality)
    if (m_groqAvailable) {
        m_currentBackend = InferenceBackend::GROQ_KIMI_K2;
        LOG_INFO("Default inference backend: Groq (Kimi K2) - VERY FAST");
    } else if (m_jimmyAvailable) {
        m_currentBackend = InferenceBackend::JIMMY_QWEN_72B;
        LOG_INFO("Default inference backend: ChatJimmy.ai (Qwen 72B) - FAST");
    } else if (m_bedrockAvailable) {
        m_currentBackend = InferenceBackend::AWS_BEDROCK;
        LOG_INFO("Default inference backend: AWS Bedrock (Claude)");
    } else {
        m_currentBackend = InferenceBackend::NONE;
        LOG_WARN("No AI inference backends available");
    }
}

InferenceEngine::~InferenceEngine() = default;

bool InferenceEngine::isAvailable() const {
    return m_currentBackend != InferenceBackend::NONE;
}

std::string InferenceEngine::backendToString(InferenceBackend backend) {
    switch (backend) {
        case InferenceBackend::AWS_BEDROCK: return "Bedrock: Claude Sonnet (Best)";
        case InferenceBackend::AWS_BEDROCK_HAIKU: return "Bedrock: Claude Haiku (Fast)";
        case InferenceBackend::GROQ_KIMI_K2: return "Groq: Kimi K2 (Very Fast)";
        case InferenceBackend::JIMMY_QWEN_72B: return "Jimmy: Qwen 72B (Fast)";
        case InferenceBackend::JIMMY_LLAMA_70B: return "Jimmy: Llama 70B (Fast)";
        case InferenceBackend::JIMMY_LLAMA_8B: return "Jimmy: Llama 8B (Fastest)";
        case InferenceBackend::NONE: return "None";
        default: return "Unknown";
    }
}

void InferenceEngine::setBackend(InferenceBackend backend) {
    if (backend == m_currentBackend) return;

    // Update Jimmy model if using Jimmy backends
    if (m_jimmy) {
        switch (backend) {
            case InferenceBackend::JIMMY_QWEN_72B:
                m_jimmy->setModel(JimmyModel::QWEN_72B);
                break;
            case InferenceBackend::JIMMY_LLAMA_70B:
                m_jimmy->setModel(JimmyModel::LLAMA_70B);
                break;
            case InferenceBackend::JIMMY_LLAMA_8B:
                m_jimmy->setModel(JimmyModel::LLAMA_8B);
                break;
            default:
                break;
        }
    }

    // Update Bedrock model if using Bedrock backends
    if (m_bedrock) {
        switch (backend) {
            case InferenceBackend::AWS_BEDROCK:
                // Claude Sonnet 4.5
                m_bedrock->setModelId("us.anthropic.claude-sonnet-4-5-20250929-v1:0");
                break;
            case InferenceBackend::AWS_BEDROCK_HAIKU:
                // Claude Haiku 3.5
                m_bedrock->setModelId("us.anthropic.claude-3-5-haiku-20241022-v1:0");
                break;
            default:
                break;
        }
    }

    m_currentBackend = backend;
    LOG_INFO("Inference backend changed to: {}", backendToString(backend));
}

std::vector<InferenceBackend> InferenceEngine::getAvailableBackends() const {
    std::vector<InferenceBackend> backends;

    // Groq first (fastest with good quality)
    if (m_groqAvailable) {
        backends.push_back(InferenceBackend::GROQ_KIMI_K2);
    }
    if (m_jimmyAvailable) {
        backends.push_back(InferenceBackend::JIMMY_QWEN_72B);
        backends.push_back(InferenceBackend::JIMMY_LLAMA_70B);
        backends.push_back(InferenceBackend::JIMMY_LLAMA_8B);
    }
    if (m_bedrockAvailable) {
        backends.push_back(InferenceBackend::AWS_BEDROCK_HAIKU);  // Haiku first (faster)
        backends.push_back(InferenceBackend::AWS_BEDROCK);        // Sonnet (best quality)
    }

    return backends;
}

std::optional<std::string> InferenceEngine::queryBackend(const std::string& systemPrompt,
                                                          const std::string& userPrompt) {
    switch (m_currentBackend) {
        case InferenceBackend::GROQ_KIMI_K2:
            if (m_groq) {
                return m_groq->query(systemPrompt, userPrompt);
            }
            break;

        case InferenceBackend::JIMMY_QWEN_72B:
        case InferenceBackend::JIMMY_LLAMA_70B:
        case InferenceBackend::JIMMY_LLAMA_8B:
            if (m_jimmy) {
                return m_jimmy->query(systemPrompt, userPrompt);
            }
            break;

        case InferenceBackend::AWS_BEDROCK:
        case InferenceBackend::AWS_BEDROCK_HAIKU:
            if (m_bedrock && m_bedrock->hasValidCredentials()) {
                InferenceRequest request;
                request.system_prompt = systemPrompt;
                request.user_prompt = userPrompt;
                auto response = m_bedrock->inferSync(request);
                if (!response.raw_response.empty()) {
                    return response.raw_response;
                }
            }
            break;

        default:
            break;
    }
    return std::nullopt;
}

void InferenceEngine::applyInferredValues(ExoplanetData& data,
                                          const nlohmann::json& values,
                                          const std::string& reasoning) {
    auto applyDouble = [&values, &reasoning](MeasuredValue<double>& target,
                                             const std::string& key) {
        if (values.contains(key)) {
            const auto& v = values[key];
            if (v.contains("value")) {
                // Handle both number and string values from AI
                if (v["value"].is_number()) {
                    target.value = v["value"].get<double>();
                } else if (v["value"].is_string()) {
                    try {
                        target.value = std::stod(v["value"].get<std::string>());
                    } catch (...) {
                        return;  // Skip if can't parse
                    }
                } else {
                    return;  // Skip unknown types
                }
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

    LOG_INFO("Inferring atmosphere for {} using {}", data.name, backendToString(m_currentBackend));

    std::string systemPrompt = std::string(prompts::SYSTEM_PROMPT);
    std::string userPrompt = prompts::buildAtmospherePrompt(data);

    auto response = queryBackend(systemPrompt, userPrompt);

    if (response.has_value()) {
        try {
            // Extract JSON from response (handles markdown code blocks)
            std::string jsonText = extractJsonFromResponse(*response);
            auto j = nlohmann::json::parse(jsonText);

            // Handle different response formats
            nlohmann::json values;
            std::string reasoning;

            if (j.contains("inferred_values")) {
                values = j["inferred_values"];
                if (j.contains("reasoning")) {
                    reasoning = j["reasoning"].get<std::string>();
                }
            } else {
                // Assume the response IS the inferred values
                values = j;
            }

            applyInferredValues(data, values, reasoning);
            LOG_INFO("Atmosphere inference complete for {}", data.name);
        } catch (const std::exception& e) {
            LOG_WARN("Failed to parse atmosphere inference response: {}", e.what());
        }
    } else {
        LOG_WARN("Atmosphere inference failed for {}", data.name);
    }
}

void InferenceEngine::inferRenderHints(ExoplanetData& data) {
    if (!isAvailable()) {
        return;
    }

    LOG_INFO("Inferring render hints for {}", data.name);

    std::string systemPrompt = std::string(prompts::SYSTEM_PROMPT);
    std::string userPrompt = prompts::buildRenderHintsPrompt(data);

    auto response = queryBackend(systemPrompt, userPrompt);

    if (response.has_value()) {
        try {
            // Extract JSON from response (handles markdown code blocks)
            std::string jsonText = extractJsonFromResponse(*response);
            auto j = nlohmann::json::parse(jsonText);
            nlohmann::json values;
            std::string reasoning;

            if (j.contains("inferred_values")) {
                values = j["inferred_values"];
                if (j.contains("reasoning")) {
                    reasoning = j["reasoning"].get<std::string>();
                }
            } else {
                values = j;
            }

            applyInferredValues(data, values, reasoning);
            LOG_INFO("Render hints inference complete for {}", data.name);
        } catch (const std::exception& e) {
            LOG_WARN("Failed to parse render hints response: {}", e.what());
        }
    } else {
        LOG_WARN("Render hints inference failed for {}", data.name);
    }
}

ExoplanetData InferenceEngine::fillMissingParametersSync(ExoplanetData data) {
    if (!isAvailable()) {
        return data;
    }

    // Infer atmosphere if missing key parameters
    if (!data.surface_pressure_atm.hasValue() ||
        !data.albedo.hasValue() ||
        !data.atmosphere_composition.hasValue()) {
        inferAtmosphere(data);
    }

    // Infer render hints if missing
    if (!data.biome_classification.hasValue()) {
        inferRenderHints(data);
    }

    return data;
}

std::future<ExoplanetData> InferenceEngine::fillMissingParameters(ExoplanetData data) {
    return std::async(std::launch::async, [this, data = std::move(data)]() mutable {
        return fillMissingParametersSync(std::move(data));
    });
}

std::optional<PlanetParams> InferenceEngine::parseRenderParamsJson(const std::string& json) {
    try {
        std::string planetJson;

        // The raw response might be the full Bedrock API response with content[0].text
        // or it might be just the text content. Try to extract properly.
        try {
            auto outerJson = nlohmann::json::parse(json);

            // Check if this is a Bedrock API response wrapper
            if (outerJson.contains("content") && outerJson["content"].is_array() &&
                !outerJson["content"].empty() && outerJson["content"][0].contains("text")) {
                // Extract the text content from Bedrock response
                planetJson = outerJson["content"][0]["text"].get<std::string>();
                LOG_DEBUG("Extracted text from Bedrock response: {} chars", planetJson.size());
            } else {
                // Assume it's already the planet JSON or text containing it
                planetJson = json;
            }
        } catch (...) {
            // Not JSON, use as-is
            planetJson = json;
        }

        // Use helper to extract JSON (strips markdown, finds JSON boundaries)
        planetJson = extractJsonFromResponse(planetJson);

        LOG_DEBUG("Parsing planet JSON: {}", planetJson.substr(0, std::min(size_t(200), planetJson.size())));
        auto j = nlohmann::json::parse(planetJson);
        PlanetParams params;

        // Helper to safely extract values
        auto getFloat = [&j](const std::string& key, float defaultVal) -> float {
            if (j.contains(key) && j[key].is_number()) {
                return j[key].get<float>();
            }
            return defaultVal;
        };

        auto getInt = [&j](const std::string& key, int defaultVal) -> int {
            if (j.contains(key) && j[key].is_number()) {
                return j[key].get<int>();
            }
            return defaultVal;
        };

        auto getVec3 = [&j](const std::string& key, glm::vec3 defaultVal) -> glm::vec3 {
            if (j.contains(key) && j[key].is_array() && j[key].size() >= 3) {
                return glm::vec3(
                    j[key][0].get<float>(),
                    j[key][1].get<float>(),
                    j[key][2].get<float>()
                );
            }
            return defaultVal;
        };

        // Parse all parameters
        params.radius = getFloat("radius", 1.0f);
        params.rotationSpeed = getFloat("rotationSpeed", 0.1f);

        int noiseTypeInt = getInt("noiseType", 0);
        params.noiseType = static_cast<NoiseType>(std::clamp(noiseTypeInt, 0, 6));

        params.noiseStrength = getFloat("noiseStrength", 0.02f);
        params.terrainScale = getFloat("terrainScale", 0.8f);

        params.fbmOctaves = getInt("fbmOctaves", 6);
        params.fbmPersistence = getFloat("fbmPersistence", 0.5f);
        params.fbmLacunarity = getFloat("fbmLacunarity", 2.0f);
        params.fbmExponentiation = getFloat("fbmExponentiation", 5.0f);
        params.domainWarpStrength = getFloat("domainWarpStrength", 0.0f);

        params.ridgedStrength = getFloat("ridgedStrength", 0.0f);
        params.craterStrength = getFloat("craterStrength", 0.0f);
        params.continentScale = getFloat("continentScale", 0.0f);
        params.continentBlend = getFloat("continentBlend", 0.15f);

        params.waterLevel = getFloat("waterLevel", 0.0f);
        params.polarCapSize = getFloat("polarCapSize", 0.0f);
        params.bandingStrength = getFloat("bandingStrength", 0.0f);
        params.bandingFrequency = getFloat("bandingFrequency", 20.0f);

        // Gas giant storm system
        params.stormCount = getFloat("stormCount", 0.0f);
        params.stormSize = getFloat("stormSize", 0.15f);
        params.stormIntensity = getFloat("stormIntensity", 0.8f);
        params.stormSeed = getFloat("stormSeed", 0.0f);
        params.stormColor = getVec3("stormColor", {0.8f, 0.4f, 0.2f});
        params.flowSpeed = getFloat("flowSpeed", 1.0f);
        params.turbulenceScale = getFloat("turbulenceScale", 1.0f);
        params.vortexTightness = getFloat("vortexTightness", 3.0f);

        // Colors
        params.waterColorDeep = getVec3("waterColorDeep", {0.01f, 0.05f, 0.15f});
        params.waterColorSurface = getVec3("waterColorSurface", {0.02f, 0.12f, 0.27f});
        params.sandColor = getVec3("sandColor", {1.0f, 1.0f, 0.85f});
        params.treeColor = getVec3("treeColor", {0.02f, 0.1f, 0.06f});
        params.rockColor = getVec3("rockColor", {0.15f, 0.12f, 0.12f});
        params.iceColor = getVec3("iceColor", {0.8f, 0.9f, 0.9f});

        params.sandLevel = getFloat("sandLevel", 0.003f);
        params.treeLevel = getFloat("treeLevel", 0.004f);
        params.rockLevel = getFloat("rockLevel", 0.02f);
        params.iceLevel = getFloat("iceLevel", 0.04f);
        params.transition = getFloat("transition", 0.01f);

        params.cloudsDensity = std::min(0.02f, getFloat("cloudsDensity", 0.0f));  // Cap at 0.02 max!
        params.cloudsScale = getFloat("cloudsScale", 0.8f);
        params.cloudsSpeed = getFloat("cloudsSpeed", 1.5f);
        params.cloudAltitude = getFloat("cloudAltitude", 0.25f);
        params.cloudThickness = getFloat("cloudThickness", 0.2f);

        params.atmosphereColor = getVec3("atmosphereColor", {0.05f, 0.3f, 0.9f});
        params.atmosphereDensity = getFloat("atmosphereDensity", 0.3f);
        params.cloudColor = getVec3("cloudColor", {1.0f, 1.0f, 1.0f});

        params.sunDirection = getVec3("sunDirection", {1.0f, 1.0f, 0.5f});
        params.sunIntensity = getFloat("sunIntensity", 3.0f);
        params.ambientLight = getFloat("ambientLight", 0.01f);
        params.sunColor = getVec3("sunColor", {1.0f, 1.0f, 0.9f});
        params.deepSpaceColor = getVec3("deepSpaceColor", {0.0f, 0.0f, 0.001f});

        params.rotationOffset = getFloat("rotationOffset", 0.6f);
        params.quality = getFloat("quality", 1.0f);

        // Post-process: Boost color vibrancy to avoid bland/washed-out planets
        auto boostSaturation = [](glm::vec3& color, float factor) {
            float maxC = std::max({color.r, color.g, color.b});
            float minC = std::min({color.r, color.g, color.b});
            float delta = maxC - minC;

            if (delta > 0.01f && maxC > 0.01f) {
                float sat = delta / maxC;
                float newSat = std::min(1.0f, sat * factor);
                float satRatio = newSat / sat;
                color.r = maxC - (maxC - color.r) * satRatio;
                color.g = maxC - (maxC - color.g) * satRatio;
                color.b = maxC - (maxC - color.b) * satRatio;
            }
            color = glm::clamp(color, 0.0f, 1.0f);
        };

        float satBoost = 1.3f;
        boostSaturation(params.waterColorDeep, satBoost);
        boostSaturation(params.waterColorSurface, satBoost);
        boostSaturation(params.sandColor, satBoost);
        boostSaturation(params.treeColor, satBoost);
        boostSaturation(params.rockColor, satBoost);
        boostSaturation(params.atmosphereColor, satBoost * 1.2f);

        params.sunIntensity = std::max(params.sunIntensity, 2.5f);

        return params;

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse render params JSON: {}", e.what());
        return std::nullopt;
    }
}

std::optional<PlanetParams> InferenceEngine::generateRenderParams(const ExoplanetData& data) {
    if (!isAvailable()) {
        LOG_WARN("AI inference not available, cannot generate render params");
        return std::nullopt;
    }

    LOG_INFO("Generating AI render params for {} using {}", data.name, backendToString(m_currentBackend));

    std::string systemPrompt = std::string(prompts::RENDER_PARAMS_SYSTEM_PROMPT);
    std::string userPrompt = prompts::buildFullRenderParamsPrompt(data);

    auto response = queryBackend(systemPrompt, userPrompt);

    if (response.has_value() && !response->empty()) {
        LOG_DEBUG("Raw AI response (first 500 chars): {}",
                  response->substr(0, std::min(size_t(500), response->size())));

        auto params = parseRenderParamsJson(*response);
        if (params) {
            LOG_INFO("Successfully parsed AI render params for {}", data.name);
            LOG_INFO("  AI params: radius={:.2f}, noiseStrength={:.3f}, atmosphereDensity={:.2f}",
                     params->radius, params->noiseStrength, params->atmosphereDensity);
            LOG_INFO("  AI params: bandingStrength={:.2f}, waterLevel={:.2f}, craterStrength={:.2f}",
                     params->bandingStrength, params->waterLevel, params->craterStrength);
            LOG_INFO("  AI colors: atmosphere=({:.2f},{:.2f},{:.2f}), sand=({:.2f},{:.2f},{:.2f})",
                     params->atmosphereColor.r, params->atmosphereColor.g, params->atmosphereColor.b,
                     params->sandColor.r, params->sandColor.g, params->sandColor.b);
            return params;
        }

        LOG_ERROR("Could not parse AI response as render params for {}", data.name);
        return std::nullopt;
    } else {
        LOG_ERROR("AI render params generation failed for {}", data.name);
        return std::nullopt;
    }
}

std::future<std::optional<PlanetParams>> InferenceEngine::generateRenderParamsAsync(const ExoplanetData& data) {
    return std::async(std::launch::async, [this, data]() {
        return generateRenderParams(data);
    });
}

}  // namespace astrocore
