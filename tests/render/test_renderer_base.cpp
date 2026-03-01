#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "render/RendererBase.hpp"
#include "render/Camera.hpp"
#include "render/VulkanTypes.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

using namespace astrocore;
using Catch::Matchers::WithinAbs;

// Concrete subclass for testing since RendererBase is abstract
// (init/resize/beginFrame/render/endFrame are pure virtual from IRenderer)
class TestRenderer : public RendererBase {
public:
    void init(int width, int height, void* /*nativeWindow*/ = nullptr) override {
        width_  = width;
        height_ = height;
    }
    void resize(int width, int height) override {
        width_  = width;
        height_ = height;
    }
    void beginFrame() override {}
    void render(const Camera& /*camera*/) override {}
    void endFrame() override {}
};

// ============================================================================
// TIME MANAGEMENT
// ============================================================================
TEST_CASE("RendererBase: time management", "[renderer_base][time]") {
    TestRenderer r;

    SECTION("initial state") {
        REQUIRE_THAT(r.currentTime(), WithinAbs(0.0f, 1e-6f));
        REQUIRE(r.isPaused() == false);
        REQUIRE_THAT(r.timeScale(), WithinAbs(1.0f, 1e-6f));
    }

    SECTION("advanceTime increments when not paused") {
        r.advanceTime(0.016f);
        REQUIRE_THAT(r.currentTime(), WithinAbs(0.016f, 1e-6f));
    }

    SECTION("advanceTime respects timeScale") {
        r.setTimeScale(2.0f);
        r.advanceTime(0.016f);
        REQUIRE_THAT(r.currentTime(), WithinAbs(0.032f, 1e-6f));
    }

    SECTION("advanceTime does nothing when paused") {
        r.setPaused(true);
        r.advanceTime(0.016f);
        REQUIRE_THAT(r.currentTime(), WithinAbs(0.0f, 1e-6f));
    }

    SECTION("paused -> unpaused resumes advancement") {
        r.setPaused(true);
        r.advanceTime(0.016f);
        REQUIRE_THAT(r.currentTime(), WithinAbs(0.0f, 1e-6f));

        r.setPaused(false);
        r.advanceTime(0.016f);
        REQUIRE_THAT(r.currentTime(), WithinAbs(0.016f, 1e-6f));
    }

    SECTION("zero timeScale freezes time without pausing") {
        r.setTimeScale(0.0f);
        r.advanceTime(0.016f);
        REQUIRE_THAT(r.currentTime(), WithinAbs(0.0f, 1e-6f));
    }
}

// ============================================================================
// PLANET POSITION & EMISSIVE STATE
// ============================================================================
TEST_CASE("RendererBase: planet position and emissive", "[renderer_base][state]") {
    TestRenderer r;

    SECTION("default planet position") {
        auto pos = r.planetPosition();
        REQUIRE_THAT(pos.x, WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(pos.y, WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(pos.z, WithinAbs(-10.0f, 1e-6f));
    }

    SECTION("setPlanetPosition updates position") {
        r.setPlanetPosition(glm::vec3(1.0f, 2.0f, 3.0f));
        auto pos = r.planetPosition();
        REQUIRE_THAT(pos.x, WithinAbs(1.0f, 1e-6f));
        REQUIRE_THAT(pos.y, WithinAbs(2.0f, 1e-6f));
        REQUIRE_THAT(pos.z, WithinAbs(3.0f, 1e-6f));
    }

    SECTION("emissive default is false") {
        REQUIRE(r.isEmissive() == false);
    }

    SECTION("setEmissive toggles flag") {
        r.setEmissive(true);
        REQUIRE(r.isEmissive() == true);
        r.setEmissive(false);
        REQUIRE(r.isEmissive() == false);
    }
}

// ============================================================================
// GENERATE NOISE DATA
// ============================================================================
TEST_CASE("RendererBase::generateNoiseData returns deterministic 512^3 data", "[renderer_base][noise]") {
    auto data = RendererBase::generateNoiseData();

    SECTION("correct size: 512^3 = 134,217,728 bytes") {
        REQUIRE(data.size() == 512ULL * 512 * 512);
    }

    SECTION("deterministic: first few bytes match LCG with seed 0x12345678") {
        uint32_t seed = 0x12345678;
        for (int i = 0; i < 16; ++i) {
            seed = seed * 1664525u + 1013904223u;
            uint8_t expected = static_cast<uint8_t>(seed >> 24);
            REQUIRE(data[i] == expected);
        }
    }

    SECTION("two calls produce identical data") {
        auto data2 = RendererBase::generateNoiseData();
        // Check first and last 256 bytes for equality
        REQUIRE(std::equal(data.begin(), data.begin() + 256, data2.begin()));
        REQUIRE(std::equal(data.end() - 256, data.end(), data2.end() - 256));
    }
}

// ============================================================================
// GET QUAD VERTICES
// ============================================================================
TEST_CASE("RendererBase::getQuadVertices returns 24 floats", "[renderer_base][quad]") {
    auto verts = RendererBase::getQuadVertices();

    SECTION("correct element count: 6 vertices * 4 components = 24") {
        REQUIRE(verts.size() == 24);
    }

    SECTION("first vertex is (-1, -1, 0, 0)") {
        REQUIRE_THAT(verts[0], WithinAbs(-1.0f, 1e-6f));
        REQUIRE_THAT(verts[1], WithinAbs(-1.0f, 1e-6f));
        REQUIRE_THAT(verts[2], WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(verts[3], WithinAbs(0.0f, 1e-6f));
    }

    SECTION("covers full NDC range [-1, 1]") {
        float minX = 1.0f, maxX = -1.0f, minY = 1.0f, maxY = -1.0f;
        for (size_t i = 0; i < verts.size(); i += 4) {
            minX = std::min(minX, verts[i]);
            maxX = std::max(maxX, verts[i]);
            minY = std::min(minY, verts[i + 1]);
            maxY = std::max(maxY, verts[i + 1]);
        }
        REQUIRE_THAT(minX, WithinAbs(-1.0f, 1e-6f));
        REQUIRE_THAT(maxX, WithinAbs(1.0f, 1e-6f));
        REQUIRE_THAT(minY, WithinAbs(-1.0f, 1e-6f));
        REQUIRE_THAT(maxY, WithinAbs(1.0f, 1e-6f));
    }
}

// ============================================================================
// FILL UNIFORMS
// ============================================================================
TEST_CASE("RendererBase::fillUniforms populates UBO from PlanetParams", "[renderer_base][uniforms]") {
    TestRenderer r;
    r.init(1920, 1080);

    Camera camera;
    camera.setPosition(glm::vec3(0.0f, 0.0f, 8.0f));

    SECTION("resolution matches renderer dimensions") {
        PlanetUniformsVk u = r.fillUniforms(camera);
        REQUIRE_THAT(u.resX, WithinAbs(1920.0f, 1e-3f));
        REQUIRE_THAT(u.resY, WithinAbs(1080.0f, 1e-3f));
    }

    SECTION("planet position is written correctly") {
        r.setPlanetPosition(glm::vec3(5.0f, -3.0f, 1.0f));
        PlanetUniformsVk u = r.fillUniforms(camera);
        REQUIRE_THAT(u.planetX, WithinAbs(5.0f, 1e-6f));
        REQUIRE_THAT(u.planetY, WithinAbs(-3.0f, 1e-6f));
        REQUIRE_THAT(u.planetZ, WithinAbs(1.0f, 1e-6f));
    }

    SECTION("radius from params is used") {
        r.params().radius = 4.5f;
        PlanetUniformsVk u = r.fillUniforms(camera);
        REQUIRE_THAT(u.radius, WithinAbs(4.5f, 1e-6f));
    }

    SECTION("emissive flag is reflected") {
        r.setEmissive(false);
        PlanetUniformsVk u1 = r.fillUniforms(camera);
        REQUIRE_THAT(u1.isEmissive, WithinAbs(0.0f, 1e-6f));

        r.setEmissive(true);
        PlanetUniformsVk u2 = r.fillUniforms(camera);
        REQUIRE_THAT(u2.isEmissive, WithinAbs(1.0f, 1e-6f));
    }

    SECTION("time value is copied to UBO") {
        r.advanceTime(0.5f);
        PlanetUniformsVk u = r.fillUniforms(camera);
        REQUIRE_THAT(u.time, WithinAbs(0.5f, 1e-6f));
    }

    SECTION("camera position is copied to UBO") {
        camera.setPosition(glm::vec3(1.0f, 2.0f, 3.0f));
        camera.update(0.0f);
        PlanetUniformsVk u = r.fillUniforms(camera);
        REQUIRE_THAT(u.camPosX, WithinAbs(1.0f, 1e-3f));
        REQUIRE_THAT(u.camPosY, WithinAbs(2.0f, 1e-3f));
        REQUIRE_THAT(u.camPosZ, WithinAbs(3.0f, 1e-3f));
    }

    SECTION("water level from params is used") {
        r.params().waterLevel = 0.42f;
        PlanetUniformsVk u = r.fillUniforms(camera);
        REQUIRE_THAT(u.waterLevel, WithinAbs(0.42f, 1e-6f));
    }

    SECTION("black hole flag maps to float") {
        r.params().isBlackHole = true;
        PlanetUniformsVk u = r.fillUniforms(camera);
        REQUIRE_THAT(u.isBlackHole, WithinAbs(1.0f, 1e-6f));

        r.params().isBlackHole = false;
        PlanetUniformsVk u2 = r.fillUniforms(camera);
        REQUIRE_THAT(u2.isBlackHole, WithinAbs(0.0f, 1e-6f));
    }

    SECTION("noise type enum maps to float") {
        r.params().noiseType = NoiseType::Ridged;
        PlanetUniformsVk u = r.fillUniforms(camera);
        REQUIRE_THAT(u.noiseType, WithinAbs(1.0f, 1e-6f));

        r.params().noiseType = NoiseType::Voronoi;
        PlanetUniformsVk u2 = r.fillUniforms(camera);
        REQUIRE_THAT(u2.noiseType, WithinAbs(4.0f, 1e-6f));
    }

    SECTION("UBO struct size is 544 bytes") {
        PlanetUniformsVk u = r.fillUniforms(camera);
        REQUIRE(sizeof(u) == 544);
    }
}

// ============================================================================
// RESOLVE ASSET PATH
// ============================================================================
TEST_CASE("RendererBase::resolveAssetPath fallback", "[renderer_base][path]") {
    SECTION("non-existent path falls back to relative") {
        std::string result = RendererBase::resolveAssetPath("nonexistent/file.txt");
        REQUIRE(result == "nonexistent/file.txt");
    }
}
