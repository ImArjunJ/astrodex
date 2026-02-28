#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <vector>
#include <array>
#include "render/PlanetTypes.hpp"

namespace astrocore {

// Texture resolution for cubemap faces
constexpr int CUBEMAP_RESOLUTION = 1024;

struct PlanetTextures {
    GLuint albedo = 0;      // Color/diffuse
    GLuint normal = 0;      // Surface normals
    GLuint roughness = 0;   // Roughness map (ocean = smooth, land = rough)
    GLuint clouds = 0;      // Cloud layer opacity
    GLuint night = 0;       // Night side lights (optional)
};

class CubemapGenerator {
public:
    CubemapGenerator();
    ~CubemapGenerator();

    // Generate all textures for a planet
    PlanetTextures generate(PlanetType type, const PlanetProfile& profile, uint32_t seed = 0);

    // Cleanup textures
    void destroy(PlanetTextures& textures);

private:
    // Noise functions
    float noise3D(glm::vec3 p) const;
    float fbm(glm::vec3 p, int octaves) const;
    float voronoi(glm::vec3 p) const;
    float ridgedNoise(glm::vec3 p, int octaves) const;
    glm::vec3 domainWarp(glm::vec3 p, float strength) const;

    // Texture generation
    void generateAlbedo(std::vector<uint8_t>& data, int face, PlanetType type,
                        const PlanetProfile& profile, uint32_t seed);
    void generateNormal(std::vector<uint8_t>& data, int face,
                        const std::vector<float>& heightmap);
    void generateRoughness(std::vector<uint8_t>& data, int face,
                          const std::vector<float>& heightmap, float oceanLevel);
    void generateClouds(std::vector<uint8_t>& data, int face, PlanetType type, uint32_t seed);

    // Height map for terrain
    float getHeight(glm::vec3 dir, PlanetType type, uint32_t seed) const;

    // Convert face + UV to direction
    glm::vec3 faceUVToDir(int face, float u, float v) const;

    // Create OpenGL cubemap from data
    GLuint createCubemap(const std::array<std::vector<uint8_t>, 6>& faces,
                         int channels, bool srgb = false);

    // Permutation table for noise
    std::vector<int> m_perm;
};

}  // namespace astrocore
