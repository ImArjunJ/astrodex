#include "render/CubemapGenerator.hpp"
#include "core/Logger.hpp"
#include <cmath>
#include <random>
#include <algorithm>

namespace astrocore {

CubemapGenerator::CubemapGenerator() {
    // Initialize permutation table for noise
    m_perm.resize(512);
    std::vector<int> p(256);
    for (int i = 0; i < 256; i++) p[i] = i;

    std::mt19937 rng(42);
    std::shuffle(p.begin(), p.end(), rng);

    for (int i = 0; i < 256; i++) {
        m_perm[i] = m_perm[i + 256] = p[i];
    }
}

CubemapGenerator::~CubemapGenerator() = default;

// Improved 3D noise
float CubemapGenerator::noise3D(glm::vec3 p) const {
    auto fade = [](float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); };
    auto lerp = [](float a, float b, float t) { return a + t * (b - a); };
    auto grad = [](int hash, float x, float y, float z) {
        int h = hash & 15;
        float u = h < 8 ? x : y;
        float v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
        return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
    };

    int X = static_cast<int>(std::floor(p.x)) & 255;
    int Y = static_cast<int>(std::floor(p.y)) & 255;
    int Z = static_cast<int>(std::floor(p.z)) & 255;

    p.x -= std::floor(p.x);
    p.y -= std::floor(p.y);
    p.z -= std::floor(p.z);

    float u = fade(p.x), v = fade(p.y), w = fade(p.z);

    int A = m_perm[X] + Y, AA = m_perm[A] + Z, AB = m_perm[A + 1] + Z;
    int B = m_perm[X + 1] + Y, BA = m_perm[B] + Z, BB = m_perm[B + 1] + Z;

    return lerp(
        lerp(lerp(grad(m_perm[AA], p.x, p.y, p.z), grad(m_perm[BA], p.x - 1, p.y, p.z), u),
             lerp(grad(m_perm[AB], p.x, p.y - 1, p.z), grad(m_perm[BB], p.x - 1, p.y - 1, p.z), u), v),
        lerp(lerp(grad(m_perm[AA + 1], p.x, p.y, p.z - 1), grad(m_perm[BA + 1], p.x - 1, p.y, p.z - 1), u),
             lerp(grad(m_perm[AB + 1], p.x, p.y - 1, p.z - 1), grad(m_perm[BB + 1], p.x - 1, p.y - 1, p.z - 1), u), v),
        w);
}

float CubemapGenerator::fbm(glm::vec3 p, int octaves) const {
    float value = 0.0f;
    float amplitude = 0.5f;
    float frequency = 1.0f;
    float maxValue = 0.0f;

    for (int i = 0; i < octaves; i++) {
        value += amplitude * (noise3D(p * frequency) * 0.5f + 0.5f);
        maxValue += amplitude;
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }

    return value / maxValue;
}

float CubemapGenerator::voronoi(glm::vec3 p) const {
    glm::vec3 i = glm::floor(p);
    glm::vec3 f = glm::fract(p);

    float minDist = 1.0f;

    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            for (int z = -1; z <= 1; z++) {
                glm::vec3 neighbor(x, y, z);
                glm::vec3 cellPos = i + neighbor;

                // Random point in cell
                glm::vec3 randOffset(
                    noise3D(cellPos * 1.17f) * 0.5f + 0.5f,
                    noise3D(cellPos * 2.31f + glm::vec3(100)) * 0.5f + 0.5f,
                    noise3D(cellPos * 3.47f + glm::vec3(200)) * 0.5f + 0.5f
                );

                glm::vec3 point = neighbor + randOffset - f;
                float dist = glm::length(point);
                minDist = std::min(minDist, dist);
            }
        }
    }

    return minDist;
}

float CubemapGenerator::ridgedNoise(glm::vec3 p, int octaves) const {
    float value = 0.0f;
    float amplitude = 0.5f;
    float frequency = 1.0f;
    float maxValue = 0.0f;

    for (int i = 0; i < octaves; i++) {
        float n = noise3D(p * frequency);
        n = 1.0f - std::abs(n);  // Ridged
        n = n * n;  // Sharpen
        value += amplitude * n;
        maxValue += amplitude;
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }

    return value / maxValue;
}

glm::vec3 CubemapGenerator::domainWarp(glm::vec3 p, float strength) const {
    return p + glm::vec3(
        noise3D(p + glm::vec3(0, 0, 0)),
        noise3D(p + glm::vec3(5.2f, 1.3f, 2.8f)),
        noise3D(p + glm::vec3(1.7f, 9.2f, 3.1f))
    ) * strength;
}

glm::vec3 CubemapGenerator::faceUVToDir(int face, float u, float v) const {
    // Convert UV [0,1] to [-1,1]
    float uc = 2.0f * u - 1.0f;
    float vc = 2.0f * v - 1.0f;

    glm::vec3 dir(0.0f);
    switch (face) {
        case 0: dir = glm::vec3( 1, -vc, -uc); break;  // +X
        case 1: dir = glm::vec3(-1, -vc,  uc); break;  // -X
        case 2: dir = glm::vec3( uc,  1,  vc); break;  // +Y
        case 3: dir = glm::vec3( uc, -1, -vc); break;  // -Y
        case 4: dir = glm::vec3( uc, -vc,  1); break;  // +Z
        default: dir = glm::vec3(-uc, -vc, -1); break;  // -Z
    }
    return glm::normalize(dir);
}

float CubemapGenerator::getHeight(glm::vec3 dir, PlanetType type, uint32_t seed) const {
    // Seed offset
    glm::vec3 offset(seed * 0.1f, seed * 0.2f, seed * 0.3f);
    dir += offset;

    switch (type) {
        case PlanetType::GasGiant:
        case PlanetType::HotJupiter:
        case PlanetType::MiniNeptune:
            // Banded structure for gas giants
            return std::sin(dir.y * 8.0f) * 0.3f + fbm(dir * 4.0f, 4) * 0.2f;

        case PlanetType::IceWorld:
            // Smooth with some craters
            return fbm(dir * 3.0f, 5) * 0.4f + ridgedNoise(dir * 2.0f, 3) * 0.1f;

        case PlanetType::LavaWorld:
            // Volcanic with cracks
            return voronoi(dir * 4.0f) * 0.5f + ridgedNoise(dir * 6.0f, 4) * 0.3f;

        case PlanetType::DesertWorld:
            // Dunes and canyons
            return fbm(domainWarp(dir * 3.0f, 0.5f), 6) * 0.6f +
                   ridgedNoise(dir * 5.0f, 3) * 0.2f;

        case PlanetType::OceanWorld:
            // Mostly flat with few islands
            return fbm(dir * 2.0f, 5) * 0.3f;

        default:  // Earth-like, SuperEarth
            // Continental shapes using domain warping
            glm::vec3 warped = domainWarp(dir * 2.0f, 0.8f);
            float continents = fbm(warped, 6);

            // Mountain ridges at plate boundaries
            float plates = 1.0f - voronoi(dir * 3.0f);
            float mountains = ridgedNoise(dir * 8.0f, 4) * plates * 0.3f;

            return continents * 0.7f + mountains;
    }
}

void CubemapGenerator::generateAlbedo(std::vector<uint8_t>& data, int face,
                                       PlanetType type, const PlanetProfile& profile,
                                       uint32_t seed) {
    data.resize(CUBEMAP_RESOLUTION * CUBEMAP_RESOLUTION * 4);

    for (int y = 0; y < CUBEMAP_RESOLUTION; y++) {
        for (int x = 0; x < CUBEMAP_RESOLUTION; x++) {
            float u = (x + 0.5f) / CUBEMAP_RESOLUTION;
            float v = (y + 0.5f) / CUBEMAP_RESOLUTION;
            glm::vec3 dir = faceUVToDir(face, u, v);

            float height = getHeight(dir, type, seed);
            float latitude = std::abs(dir.y);

            glm::vec3 color;

            // Gas giants - banded
            if (type == PlanetType::GasGiant || type == PlanetType::HotJupiter ||
                type == PlanetType::MiniNeptune) {
                float band = std::sin(dir.y * 12.0f + fbm(dir * 3.0f, 3) * 2.0f) * 0.5f + 0.5f;
                color = glm::mix(profile.oceanColor, profile.highlandColor, band);
                // Storm spots
                float storm = 1.0f - voronoi(dir * 5.0f + glm::vec3(seed));
                storm = std::pow(std::max(storm - 0.7f, 0.0f) * 3.0f, 2.0f);
                color = glm::mix(color, profile.lowlandColor, storm);
            }
            // Rocky planets
            else {
                float oceanLevel = profile.oceanLevel;

                // Ocean
                if (height < oceanLevel) {
                    float depth = (oceanLevel - height) / oceanLevel;
                    glm::vec3 deepOcean = profile.oceanColor * 0.4f;
                    glm::vec3 shallowOcean = profile.oceanColor * 1.3f + glm::vec3(0.0f, 0.05f, 0.1f);
                    color = glm::mix(shallowOcean, deepOcean, std::pow(depth, 0.7f));
                }
                // Ice caps
                else if (latitude > 0.7f) {
                    float ice = glm::smoothstep(0.7f, 0.85f, latitude);
                    color = glm::mix(profile.mountainColor, profile.snowColor, ice);
                }
                // Land
                else {
                    float landHeight = (height - oceanLevel) / (1.0f - oceanLevel);

                    // Beach
                    if (landHeight < 0.05f) {
                        glm::vec3 beach(0.85f, 0.78f, 0.55f);
                        color = glm::mix(beach, profile.lowlandColor, landHeight / 0.05f);
                    }
                    // Lowlands (vegetation)
                    else if (landHeight < 0.25f) {
                        float t = (landHeight - 0.05f) / 0.2f;
                        // Add vegetation variation
                        float vegVar = fbm(dir * 20.0f, 3);
                        glm::vec3 veg = glm::mix(profile.lowlandColor * 0.8f,
                                                  profile.lowlandColor * 1.2f, vegVar);
                        color = glm::mix(veg, profile.highlandColor, t * 0.5f);
                    }
                    // Highlands
                    else if (landHeight < 0.5f) {
                        float t = (landHeight - 0.25f) / 0.25f;
                        color = glm::mix(profile.highlandColor, profile.mountainColor, t);
                    }
                    // Mountains
                    else if (landHeight < 0.75f) {
                        float t = (landHeight - 0.5f) / 0.25f;
                        color = glm::mix(profile.mountainColor, profile.snowColor * 0.9f, t * 0.5f);
                    }
                    // Snow caps
                    else {
                        color = profile.snowColor;
                    }

                    // Snow at high altitudes regardless of latitude
                    float snowLine = 0.6f - latitude * 0.3f;
                    if (landHeight > snowLine) {
                        float snow = glm::smoothstep(snowLine, snowLine + 0.15f, landHeight);
                        color = glm::mix(color, profile.snowColor, snow);
                    }
                }
            }

            // Boost saturation and brightness
            float luminance = 0.299f * color.r + 0.587f * color.g + 0.114f * color.b;
            color = glm::mix(glm::vec3(luminance), color, 1.3f);  // Increase saturation
            color *= 1.2f;  // Boost brightness

            // Clamp and convert to bytes
            color = glm::clamp(color, glm::vec3(0.0f), glm::vec3(1.0f));
            size_t idx = static_cast<size_t>((y * CUBEMAP_RESOLUTION + x) * 4);
            data[idx + 0] = static_cast<uint8_t>(color.r * 255);
            data[idx + 1] = static_cast<uint8_t>(color.g * 255);
            data[idx + 2] = static_cast<uint8_t>(color.b * 255);
            data[idx + 3] = 255;
        }
    }
}

void CubemapGenerator::generateNormal(std::vector<uint8_t>& data, int face,
                                       const std::vector<float>& heightmap) {
    data.resize(CUBEMAP_RESOLUTION * CUBEMAP_RESOLUTION * 4);

    auto getHeightAt = [&](int x, int y) {
        x = std::clamp(x, 0, CUBEMAP_RESOLUTION - 1);
        y = std::clamp(y, 0, CUBEMAP_RESOLUTION - 1);
        return heightmap[y * CUBEMAP_RESOLUTION + x];
    };

    for (int y = 0; y < CUBEMAP_RESOLUTION; y++) {
        for (int x = 0; x < CUBEMAP_RESOLUTION; x++) {
            // Sobel filter for normals
            float tl = getHeightAt(x - 1, y - 1);
            float t  = getHeightAt(x, y - 1);
            float tr = getHeightAt(x + 1, y - 1);
            float l  = getHeightAt(x - 1, y);
            float r  = getHeightAt(x + 1, y);
            float bl = getHeightAt(x - 1, y + 1);
            float b  = getHeightAt(x, y + 1);
            float br = getHeightAt(x + 1, y + 1);

            float dx = (tr + 2.0f * r + br) - (tl + 2.0f * l + bl);
            float dy = (bl + 2.0f * b + br) - (tl + 2.0f * t + tr);

            glm::vec3 normal = glm::normalize(glm::vec3(-dx * 2.0f, -dy * 2.0f, 1.0f));
            normal = normal * 0.5f + 0.5f;  // Pack to [0,1]

            int idx = (y * CUBEMAP_RESOLUTION + x) * 4;
            data[idx + 0] = static_cast<uint8_t>(normal.x * 255);
            data[idx + 1] = static_cast<uint8_t>(normal.y * 255);
            data[idx + 2] = static_cast<uint8_t>(normal.z * 255);
            data[idx + 3] = 255;
        }
    }
}

void CubemapGenerator::generateRoughness(std::vector<uint8_t>& data, int face,
                                          const std::vector<float>& heightmap,
                                          float oceanLevel) {
    data.resize(CUBEMAP_RESOLUTION * CUBEMAP_RESOLUTION);

    for (int y = 0; y < CUBEMAP_RESOLUTION; y++) {
        for (int x = 0; x < CUBEMAP_RESOLUTION; x++) {
            float height = heightmap[y * CUBEMAP_RESOLUTION + x];

            float roughness;
            if (height < oceanLevel) {
                roughness = 0.1f;  // Ocean is smooth/reflective
            } else {
                roughness = 0.7f + fbm(glm::vec3(x * 0.1f, y * 0.1f, 0), 3) * 0.2f;
            }

            data[y * CUBEMAP_RESOLUTION + x] = static_cast<uint8_t>(roughness * 255);
        }
    }
}

void CubemapGenerator::generateClouds(std::vector<uint8_t>& data, int face,
                                       PlanetType type, uint32_t seed) {
    data.resize(CUBEMAP_RESOLUTION * CUBEMAP_RESOLUTION);

    // No clouds for some planet types
    if (type == PlanetType::IceWorld || type == PlanetType::LavaWorld) {
        std::fill(data.begin(), data.end(), 0);
        return;
    }

    for (int y = 0; y < CUBEMAP_RESOLUTION; y++) {
        for (int x = 0; x < CUBEMAP_RESOLUTION; x++) {
            float u = (x + 0.5f) / static_cast<float>(CUBEMAP_RESOLUTION);
            float v = (y + 0.5f) / static_cast<float>(CUBEMAP_RESOLUTION);
            glm::vec3 dir = faceUVToDir(face, u, v);

            // Different cloud patterns based on planet type
            float clouds;
            if (type == PlanetType::GasGiant || type == PlanetType::HotJupiter) {
                // Banded storm clouds
                clouds = std::sin(dir.y * 10.0f + fbm(dir * 5.0f, 3)) * 0.5f + 0.5f;
                clouds *= fbm(dir * 8.0f + glm::vec3(static_cast<float>(seed)), 4);
            } else {
                // Earth-like cumulus patterns - sparse, patchy clouds
                glm::vec3 warped = domainWarp(dir * 3.0f + glm::vec3(static_cast<float>(seed) * 0.1f), 0.3f);
                float large = fbm(warped, 4);
                float detail = fbm(dir * 12.0f + glm::vec3(static_cast<float>(seed)), 3) * 0.2f;
                clouds = large + detail;

                // Higher threshold = less cloud coverage (aim for ~30-40% coverage)
                clouds = glm::smoothstep(0.55f, 0.75f, clouds);

                // Less clouds at poles and equator
                float latitude = std::abs(dir.y);
                clouds *= glm::smoothstep(0.95f, 0.6f, latitude);
                clouds *= glm::smoothstep(0.0f, 0.2f, latitude);  // Less at equator too
            }

            data[static_cast<size_t>(y * CUBEMAP_RESOLUTION + x)] = static_cast<uint8_t>(std::clamp(clouds, 0.0f, 1.0f) * 255);
        }
    }
}

GLuint CubemapGenerator::createCubemap(const std::array<std::vector<uint8_t>, 6>& faces,
                                        int channels, bool srgb) {
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture);

    GLenum internalFormat, format;
    if (channels == 4) {
        internalFormat = srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;
        format = GL_RGBA;
    } else if (channels == 3) {
        internalFormat = srgb ? GL_SRGB8 : GL_RGB8;
        format = GL_RGB;
    } else {
        internalFormat = GL_R8;
        format = GL_RED;
    }

    for (int i = 0; i < 6; i++) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, internalFormat,
                     CUBEMAP_RESOLUTION, CUBEMAP_RESOLUTION, 0, format,
                     GL_UNSIGNED_BYTE, faces[i].data());
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    return texture;
}

PlanetTextures CubemapGenerator::generate(PlanetType type, const PlanetProfile& profile,
                                           uint32_t seed) {
    LOG_INFO("Generating planet textures (type={}, seed={})...", static_cast<int>(type), seed);

    PlanetTextures textures;
    std::array<std::vector<uint8_t>, 6> albedoFaces;
    std::array<std::vector<uint8_t>, 6> normalFaces;
    std::array<std::vector<uint8_t>, 6> roughnessFaces;
    std::array<std::vector<uint8_t>, 6> cloudFaces;

    // Generate heightmaps for normal calculation
    std::array<std::vector<float>, 6> heightmaps;

    for (int face = 0; face < 6; face++) {
        // Generate heightmap
        heightmaps[face].resize(CUBEMAP_RESOLUTION * CUBEMAP_RESOLUTION);
        for (int y = 0; y < CUBEMAP_RESOLUTION; y++) {
            for (int x = 0; x < CUBEMAP_RESOLUTION; x++) {
                float u = (x + 0.5f) / CUBEMAP_RESOLUTION;
                float v = (y + 0.5f) / CUBEMAP_RESOLUTION;
                glm::vec3 dir = faceUVToDir(face, u, v);
                heightmaps[face][y * CUBEMAP_RESOLUTION + x] = getHeight(dir, type, seed);
            }
        }

        // Generate textures
        generateAlbedo(albedoFaces[face], face, type, profile, seed);
        generateNormal(normalFaces[face], face, heightmaps[face]);
        generateRoughness(roughnessFaces[face], face, heightmaps[face], profile.oceanLevel);
        generateClouds(cloudFaces[face], face, type, seed);
    }

    // Create OpenGL textures
    textures.albedo = createCubemap(albedoFaces, 4, true);
    textures.normal = createCubemap(normalFaces, 4, false);
    textures.roughness = createCubemap(roughnessFaces, 1, false);
    textures.clouds = createCubemap(cloudFaces, 1, false);

    LOG_INFO("Planet textures generated successfully");
    return textures;
}

void CubemapGenerator::destroy(PlanetTextures& textures) {
    if (textures.albedo) glDeleteTextures(1, &textures.albedo);
    if (textures.normal) glDeleteTextures(1, &textures.normal);
    if (textures.roughness) glDeleteTextures(1, &textures.roughness);
    if (textures.clouds) glDeleteTextures(1, &textures.clouds);
    if (textures.night) glDeleteTextures(1, &textures.night);
    textures = {};
}

}  // namespace astrocore
