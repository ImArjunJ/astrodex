#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <functional>

namespace astrocore {

struct ExoplanetData;

struct GeneratedImage {
    std::vector<uint8_t> data;  // Raw RGBA pixel data
    int width = 0;
    int height = 0;
    bool success = false;
    std::string error;
};

struct ImageGenConfig {
    std::string region = "us-east-1";
    std::string model_id = "amazon.titan-image-generator-v2:0";
    int width = 1024;
    int height = 1024;
};

class ImageGenerator {
public:
    ImageGenerator();
    explicit ImageGenerator(const ImageGenConfig& config);
    ~ImageGenerator();

    // Check if image generation is available
    bool isAvailable() const;

    // Generate planet description using Claude
    std::string describePlanet(const ExoplanetData& data);

    // Generate image from prompt
    GeneratedImage generateFromPrompt(const std::string& prompt);

    // High-level: generate planet texture from exoplanet data
    GeneratedImage generatePlanetTexture(const ExoplanetData& data);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace astrocore
