#include "ai/ImageGenerator.hpp"
#include "data/ExoplanetData.hpp"
#include "core/Logger.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdlib>
#include <array>
#include <sstream>
#include <filesystem>

// For base64 decoding
#include <openssl/bio.h>
#include <openssl/evp.h>

namespace astrocore {

struct ImageGenerator::Impl {
    ImageGenConfig config;
    bool available = false;

    std::string execCommand(const std::string& cmd) {
        std::array<char, 4096> buffer;
        std::string result;
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) return "";
        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            result += buffer.data();
        }
        pclose(pipe);
        return result;
    }

    std::vector<uint8_t> base64Decode(const std::string& encoded) {
        BIO* bio = BIO_new_mem_buf(encoded.data(), static_cast<int>(encoded.size()));
        BIO* b64 = BIO_new(BIO_f_base64());
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
        bio = BIO_push(b64, bio);

        std::vector<uint8_t> decoded(encoded.size());
        int len = BIO_read(bio, decoded.data(), static_cast<int>(decoded.size()));
        BIO_free_all(bio);

        if (len > 0) {
            decoded.resize(static_cast<size_t>(len));
        } else {
            decoded.clear();
        }
        return decoded;
    }
};

ImageGenerator::ImageGenerator() : ImageGenerator(ImageGenConfig{}) {}

ImageGenerator::ImageGenerator(const ImageGenConfig& config)
    : m_impl(std::make_unique<Impl>()) {
    m_impl->config = config;

    // Check if AWS CLI is available
    std::string check = m_impl->execCommand("aws --version 2>&1");
    m_impl->available = !check.empty() && check.find("aws-cli") != std::string::npos;

    if (m_impl->available) {
        LOG_INFO("Image generator initialized (model: {})", config.model_id);
    } else {
        LOG_WARN("AWS CLI not available, image generation disabled");
    }
}

ImageGenerator::~ImageGenerator() = default;

bool ImageGenerator::isAvailable() const {
    return m_impl->available;
}

std::string ImageGenerator::describePlanet(const ExoplanetData& data) {
    // Build a prompt for Claude to describe the planet's surface for a texture map
    std::stringstream prompt;
    prompt << "Describe this exoplanet's SURFACE in 2 sentences for a texture artist. ";
    prompt << "Focus on: ocean color, land colors, ice coverage, cloud patterns. ";
    prompt << "Example: 'Deep blue oceans with brown-green continents and white polar ice caps. Swirling white clouds over tropical regions.' ";
    prompt << "No headers, no formatting, just colors and features.\n\n";

    prompt << "Planet: " << data.name << "\n";

    if (data.radius_earth.hasValue()) {
        prompt << "Radius: " << data.radius_earth.value << " Earth radii\n";
    }
    if (data.mass_earth.hasValue()) {
        prompt << "Mass: " << data.mass_earth.value << " Earth masses\n";
    }
    if (data.equilibrium_temp_k.hasValue()) {
        prompt << "Temperature: " << data.equilibrium_temp_k.value << " K\n";
    }
    if (data.planet_type.hasValue()) {
        prompt << "Type: " << data.planet_type.value << "\n";
    }
    if (!data.host_star.spectral_type.empty()) {
        prompt << "Host star type: " << data.host_star.spectral_type << "\n";
    }

    // Add inferred atmosphere if available
    if (data.surface_pressure_atm.hasValue()) {
        prompt << "Surface pressure: " << data.surface_pressure_atm.value << " atm\n";
    }

    prompt << "\nDescribe this planet's visual appearance for a photorealistic rendering:";

    // Call Claude via Bedrock
    nlohmann::json request = {
        {"anthropic_version", "bedrock-2023-05-31"},
        {"max_tokens", 150},
        {"messages", {{
            {"role", "user"},
            {"content", prompt.str()}
        }}}
    };

    std::string tempFile = "/tmp/claude_img_request.json";
    std::string outputFile = "/tmp/claude_img_response.json";

    std::ofstream out(tempFile);
    out << request.dump();
    out.close();

    std::string cmd = "aws bedrock-runtime invoke-model "
                      "--model-id 'us.anthropic.claude-sonnet-4-5-20250929-v1:0' "
                      "--region " + m_impl->config.region + " "
                      "--body fileb://" + tempFile + " "
                      "--content-type application/json "
                      "--accept application/json "
                      + outputFile + " 2>&1";

    std::string cmdOutput = m_impl->execCommand(cmd);

    // Read response
    std::ifstream in(outputFile);
    if (!in.good()) {
        LOG_ERROR("Failed to read Claude response");
        return "";
    }

    std::string responseStr((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());
    in.close();

    try {
        auto json = nlohmann::json::parse(responseStr);
        if (json.contains("content") && json["content"].is_array() && !json["content"].empty()) {
            return json["content"][0].value("text", "");
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse Claude response: {}", e.what());
    }

    return "";
}

GeneratedImage ImageGenerator::generateFromPrompt(const std::string& prompt) {
    GeneratedImage result;

    if (!m_impl->available) {
        result.error = "Image generation not available";
        return result;
    }

    LOG_INFO("Generating image from prompt...");

    // Build request for Titan Image Generator
    nlohmann::json request;

    if (m_impl->config.model_id.find("titan") != std::string::npos) {
        // Amazon Titan Image Generator format
        request = {
            {"taskType", "TEXT_IMAGE"},
            {"textToImageParams", {
                {"text", prompt}
            }},
            {"imageGenerationConfig", {
                {"numberOfImages", 1},
                {"width", m_impl->config.width},
                {"height", m_impl->config.height},
                {"cfgScale", 8.0}
            }}
        };
    } else if (m_impl->config.model_id.find("stability") != std::string::npos) {
        // Stability AI format
        request = {
            {"text_prompts", {{
                {"text", prompt},
                {"weight", 1.0}
            }}},
            {"cfg_scale", 10},
            {"steps", 50},
            {"width", m_impl->config.width},
            {"height", m_impl->config.height}
        };
    } else {
        result.error = "Unknown image model: " + m_impl->config.model_id;
        return result;
    }

    std::string tempFile = "/tmp/imggen_request.json";
    std::string outputFile = "/tmp/imggen_response.json";

    std::ofstream out(tempFile);
    out << request.dump();
    out.close();

    std::string cmd = "aws bedrock-runtime invoke-model "
                      "--model-id '" + m_impl->config.model_id + "' "
                      "--region " + m_impl->config.region + " "
                      "--body fileb://" + tempFile + " "
                      "--content-type application/json "
                      "--accept application/json "
                      + outputFile + " 2>&1";

    LOG_DEBUG("Image generation command: {}", cmd.substr(0, 100) + "...");
    std::string cmdOutput = m_impl->execCommand(cmd);

    if (cmdOutput.find("error") != std::string::npos ||
        cmdOutput.find("Error") != std::string::npos) {
        result.error = "AWS CLI error: " + cmdOutput;
        LOG_ERROR("Image generation failed: {}", cmdOutput);
        return result;
    }

    // Read response
    std::ifstream in(outputFile);
    if (!in.good()) {
        result.error = "Failed to read response file";
        return result;
    }

    std::string responseStr((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());
    in.close();

    try {
        auto json = nlohmann::json::parse(responseStr);

        std::string base64Image;

        // Extract base64 image based on model type
        if (m_impl->config.model_id.find("titan") != std::string::npos) {
            if (json.contains("images") && json["images"].is_array() && !json["images"].empty()) {
                base64Image = json["images"][0].get<std::string>();
            }
        } else if (m_impl->config.model_id.find("stability") != std::string::npos) {
            if (json.contains("artifacts") && json["artifacts"].is_array() && !json["artifacts"].empty()) {
                base64Image = json["artifacts"][0].value("base64", "");
            }
        }

        if (base64Image.empty()) {
            result.error = "No image in response";
            LOG_ERROR("No image in response: {}", responseStr.substr(0, 200));
            return result;
        }

        // Decode base64 to PNG bytes
        std::vector<uint8_t> pngData = m_impl->base64Decode(base64Image);

        if (pngData.empty()) {
            result.error = "Failed to decode base64 image";
            return result;
        }

        // Save PNG temporarily and use stb_image to decode to RGBA
        // For now, just save to file and let the texture loader handle it
        std::string pngPath = "/tmp/planet_texture.png";
        std::ofstream pngOut(pngPath, std::ios::binary);
        pngOut.write(reinterpret_cast<const char*>(pngData.data()),
                     static_cast<std::streamsize>(pngData.size()));
        pngOut.close();

        // Store PNG data and mark success
        result.data = pngData;
        result.width = m_impl->config.width;
        result.height = m_impl->config.height;
        result.success = true;

        LOG_INFO("Image generated successfully: {}x{}, {} bytes",
                 result.width, result.height, result.data.size());

    } catch (const std::exception& e) {
        result.error = std::string("JSON parse error: ") + e.what();
        LOG_ERROR("Failed to parse image response: {}", e.what());
    }

    return result;
}

GeneratedImage ImageGenerator::generatePlanetTexture(const ExoplanetData& data) {
    // Step 1: Get Claude to describe the planet
    LOG_INFO("Asking Claude to describe planet appearance...");
    std::string description = describePlanet(data);

    if (description.empty()) {
        LOG_WARN("Failed to get planet description from Claude");
        // Fall back to a basic prompt
        description = "A photorealistic exoplanet similar to Earth, with blue oceans, "
                     "brown and green continents, white cloud formations, and a thin "
                     "blue atmospheric haze at the edges. Viewed from space.";
    }

    LOG_INFO("Claude's description: {}", description.substr(0, 100) + "...");

    // Step 2: Build an optimized prompt for image generation (max 512 chars for Titan v2)
    // Request a seamless equirectangular texture map, not a planet view
    std::string imagePrompt =
        "Seamless equirectangular planet surface texture map, top-down satellite view, "
        "tileable terrain with oceans continents clouds, no horizon no stars no space. ";

    // Truncate description to fit within limit
    size_t maxDescLen = 380 - imagePrompt.length();
    if (description.length() > maxDescLen) {
        description = description.substr(0, maxDescLen);
        size_t lastSpace = description.rfind(' ');
        if (lastSpace != std::string::npos) {
            description = description.substr(0, lastSpace);
        }
    }
    imagePrompt += description;

    // Step 3: Generate the image
    return generateFromPrompt(imagePrompt);
}

}  // namespace astrocore
