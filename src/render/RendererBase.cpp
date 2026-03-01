#include "render/RendererBase.hpp"
#include "render/Camera.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstring>
#include <fstream>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#ifdef __linux__
#include <unistd.h>
#endif

namespace astrocore {

// ── resolveAssetPath ─────────────────────────────────────────────────────────

std::string RendererBase::resolveAssetPath(const std::string& relative) {
#ifdef __EMSCRIPTEN__
    return relative;
#elif defined(__APPLE__)
    char exePath[4096];
    uint32_t size = sizeof(exePath);
    if (_NSGetExecutablePath(exePath, &size) == 0) {
        std::string dir(exePath);
        auto pos = dir.find_last_of('/');
        if (pos != std::string::npos) {
            std::string candidate = dir.substr(0, pos + 1) + relative;
            if (std::ifstream(candidate).good()) return candidate;
        }
    }
    return relative;
#elif defined(__linux__)
    char exePath[4096];
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len > 0) {
        exePath[len] = '\0';
        std::string dir(exePath);
        auto pos = dir.find_last_of('/');
        if (pos != std::string::npos) {
            std::string candidate = dir.substr(0, pos + 1) + relative;
            if (std::ifstream(candidate).good()) return candidate;
        }
    }
    return relative;
#else
    return relative;
#endif
}

// ── advanceTime ──────────────────────────────────────────────────────────────

void RendererBase::advanceTime(float dt) {
    if (!paused_) {
        time_ += dt * timeScale_;
    }
}

// ── fillUniforms ─────────────────────────────────────────────────────────────

PlanetUniformsVk RendererBase::fillUniforms(const Camera& camera) {
    PlanetUniformsVk u{};

    glm::mat4 inv = glm::inverse(camera.getViewMatrix());
    std::memcpy(u.invView, &inv[0][0], sizeof(u.invView));

    // Planet rotation matrix
    float angle = time_ * params_.rotationSpeed + params_.rotationOffset;
    float c = glm::cos(angle), s = glm::sin(angle);
    glm::mat3 rot(glm::vec3(c, 0, s), glm::vec3(0, 1, 0), glm::vec3(-s, 0, c));
    u.planetRot_col0[0] = rot[0][0]; u.planetRot_col0[1] = rot[0][1]; u.planetRot_col0[2] = rot[0][2]; u.planetRot_col0[3] = 0.f;
    u.planetRot_col1[0] = rot[1][0]; u.planetRot_col1[1] = rot[1][1]; u.planetRot_col1[2] = rot[1][2]; u.planetRot_col1[3] = 0.f;
    u.planetRot_col2[0] = rot[2][0]; u.planetRot_col2[1] = rot[2][1]; u.planetRot_col2[2] = rot[2][2]; u.planetRot_col2[3] = 0.f;

    glm::vec3 cam = camera.getPosition();
    u.camPosX = cam.x; u.camPosY = cam.y; u.camPosZ = cam.z;
    u.time = time_;

    u.planetX = planetPosition_.x;
    u.planetY = planetPosition_.y;
    u.planetZ = planetPosition_.z;
    u.radius  = params_.radius;

    u.resX = float(width_); u.resY = float(height_);
    u.rotOffset = params_.rotationOffset;
    u.rotSpeed  = params_.rotationSpeed;

    u.noiseStr     = params_.noiseStrength;
    u.quality      = params_.quality;
    u.terrainScale = params_.terrainScale;
    u.domainWarp   = params_.domainWarpStrength;

    u.fbmPersist = params_.fbmPersistence;
    u.fbmLac     = params_.fbmLacunarity;
    u.fbmExp     = params_.fbmExponentiation;
    u.fbmOct     = float(params_.fbmOctaves);

    u.ridged     = params_.ridgedStrength;
    u.crater     = params_.craterStrength;
    u.continent  = params_.continentScale;
    u.waterLevel = params_.waterLevel;

    u.bandStr  = params_.bandingStrength;
    u.bandFreq = params_.bandingFrequency;
    u.polarCap = params_.polarCapSize;

    u.cloudDensity = params_.cloudsDensity;
    u.cloudScale   = params_.cloudsScale;
    u.cloudSpeed   = params_.cloudsSpeed;
    u.cloudAlt     = params_.cloudAltitude;
    u.cloudThick   = params_.cloudThickness;
    u.sunInt       = params_.sunIntensity;
    u.ambLight     = params_.ambientLight;
    u.atmoDensity  = params_.atmosphereDensity;

    auto v3 = [](float* f, const glm::vec3& v) { f[0]=v.x; f[1]=v.y; f[2]=v.z; };
    v3(&u.atmoR,      params_.atmosphereColor);
    v3(&u.sunDirX,    glm::normalize(params_.sunDirection));
    v3(&u.sunColR,    params_.sunColor);
    v3(&u.deepSpR,    params_.deepSpaceColor);
    v3(&u.waterDeepR, params_.waterColorDeep);
    v3(&u.waterSurfR, params_.waterColorSurface);
    v3(&u.sandR,      params_.sandColor);
    v3(&u.treeR,      params_.treeColor);
    v3(&u.rockR,      params_.rockColor);
    v3(&u.iceR,       params_.iceColor);
    v3(&u.cloudColR,  params_.cloudColor);

    u.sandLev    = params_.sandLevel;
    u.treeLev    = params_.treeLevel;
    u.rockLev    = params_.rockLevel;
    u.iceLev     = params_.iceLevel;
    u.transition = params_.transition;

    // Black hole
    u.isBlackHole       = params_.isBlackHole ? 1.0f : 0.0f;
    u.bhMass            = params_.bhMass;
    u.bhAccretionInner  = params_.bhAccretionInner;
    u.bhAccretionOuter  = params_.bhAccretionOuter;
    u.bhDiskSpeed       = params_.bhDiskSpeed;
    u.bhDiskTurbulence  = params_.bhDiskTurbulence;
    u.bhDiskBrightness  = params_.bhDiskBrightness;
    u.bhTempInner       = params_.bhDiskTemperatureInner;
    u.bhTempOuter       = params_.bhDiskTemperatureOuter;
    u.bhDopplerStrength = params_.bhDopplerStrength;
    u.bhRaySteps        = float(params_.bhRaySteps);
    v3(&u.bhDiskTintR, params_.bhDiskTint);

    // Extra params (blackhole branch additions)
    u.noiseType      = float(static_cast<int>(params_.noiseType));
    u.continentBlend = params_.continentBlend;
    u.isEmissive     = emissive_ ? 1.0f : 0.0f;

    return u;
}

// ── generateNoiseData ────────────────────────────────────────────────────────

std::vector<uint8_t> RendererBase::generateNoiseData() {
    constexpr int sz = 512;
    size_t totalSize = size_t(sz) * sz * sz;
    std::vector<uint8_t> data(totalSize);
    uint32_t seed = 0x12345678;
    for (size_t i = 0; i < totalSize; ++i) {
        seed = seed * 1664525u + 1013904223u;
        data[i] = static_cast<uint8_t>(seed >> 24);
    }
    return data;
}

// ── getQuadVertices ──────────────────────────────────────────────────────────

std::vector<float> RendererBase::getQuadVertices() {
    return {
        -1.f, -1.f, 0.f, 0.f,
         1.f, -1.f, 1.f, 0.f,
         1.f,  1.f, 1.f, 1.f,
        -1.f, -1.f, 0.f, 0.f,
         1.f,  1.f, 1.f, 1.f,
        -1.f,  1.f, 0.f, 1.f,
    };
}

} // namespace astrocore
