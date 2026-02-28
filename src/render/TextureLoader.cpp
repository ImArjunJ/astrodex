#include "render/TextureLoader.hpp"
#include "core/Logger.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "external/stb_image.h"

namespace astrocore {

GLuint TextureLoader::loadFromFile(const std::string& path) {
    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);

    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (!data) {
        LOG_ERROR("Failed to load texture: {}", path);
        return 0;
    }

    GLuint texture = loadFromPixels(data, width, height);
    stbi_image_free(data);

    LOG_INFO("Loaded texture from file: {}x{}", width, height);
    return texture;
}

GLuint TextureLoader::loadFromMemory(const std::vector<uint8_t>& data) {
    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);

    unsigned char* pixels = stbi_load_from_memory(
        data.data(), static_cast<int>(data.size()),
        &width, &height, &channels, 4);

    if (!pixels) {
        LOG_ERROR("Failed to load texture from memory");
        return 0;
    }

    GLuint texture = loadFromPixels(pixels, width, height);
    stbi_image_free(pixels);

    LOG_INFO("Loaded texture from memory: {}x{}", width, height);
    return texture;
}

GLuint TextureLoader::loadFromPixels(const uint8_t* pixels, int width, int height) {
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenerateMipmap(GL_TEXTURE_2D);

    return texture;
}

GLuint TextureLoader::createSphereTexture(GLuint texture2D) {
    // For now, just return the 2D texture
    // Could convert to cubemap if needed
    return texture2D;
}

}  // namespace astrocore
