#pragma once

#include <glad/gl.h>
#include <string>
#include <vector>
#include <cstdint>

namespace astrocore {

class TextureLoader {
public:
    // Load texture from file path
    static GLuint loadFromFile(const std::string& path);

    // Load texture from memory (PNG data)
    static GLuint loadFromMemory(const std::vector<uint8_t>& data);

    // Load texture from raw RGBA pixels
    static GLuint loadFromPixels(const uint8_t* pixels, int width, int height);

    // Create a simple 2D texture as a sphere map (for planet rendering)
    static GLuint createSphereTexture(GLuint texture2D);
};

}  // namespace astrocore
