# WebGPU + WASM Port Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Port AstroSplat's procedural planet renderer to run in the browser via WebGPU + Emscripten WASM, with ImGui controls and visual parity to the Vulkan desktop version.

**Architecture:** Extract shared rendering logic (UBO filling, noise generation, quad vertices, texture loading) into a `RendererBase` class. Create `WebGPURenderer` implementing the same `IRenderer` interface. Manually translate `planet_vk.frag` (GLSL 450) to `planet_web.wgsl`. Build via Emscripten toolchain integrated into the existing CMakeLists.txt.

**Tech Stack:** C++23, WebGPU (via Emscripten's Dawn), WGSL, Emscripten 3.x, GLFW (Emscripten backend), Dear ImGui (WebGPU + GLFW backends), GLM, stb_image

---

## Task 1: Extract RendererBase from VulkanRenderer

Extract shared logic into a base class that both VulkanRenderer and WebGPURenderer will inherit.

**Files:**
- Create: `src/render/RendererBase.hpp`
- Create: `src/render/RendererBase.cpp`
- Modify: `src/render/IRenderer.hpp` (add setPlanetPosition, setPaused, setTimeScale, setEmissive to interface)
- Modify: `src/render/VulkanRenderer.hpp` (inherit from RendererBase instead of IRenderer)
- Modify: `src/render/VulkanRenderer.cpp` (delegate shared logic to base)
- Create: `tests/render/test_renderer_base.cpp`
- Modify: `tests/CMakeLists.txt` (add test_renderer_base.cpp)

**Step 1: Write the failing test**

```cpp
// tests/render/test_renderer_base.cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "render/RendererBase.hpp"
#include "render/VulkanTypes.hpp"
#include "render/Camera.hpp"

using namespace astrocore;
using Catch::Matchers::WithinAbs;

TEST_CASE("RendererBase::fillUniforms populates UBO from PlanetParams", "[render]") {
    // RendererBase is abstract, so we need a concrete subclass for testing
    struct TestRenderer : RendererBase {
        void init(int, int, void*) override {}
        void resize(int, int) override {}
        void beginFrame() override {}
        void render(const Camera&) override {}
        void endFrame() override {}
    };

    TestRenderer r;
    r.params().radius = 3.5f;
    r.params().noiseStrength = 0.05f;
    r.params().sunIntensity = 4.0f;
    r.setPlanetPosition({1.0f, 2.0f, -10.0f});

    Camera cam;
    cam.setPosition({0.f, 0.f, 6.f});
    cam.setTarget({0.f, 0.f, -10.f});

    PlanetUniformsVk u = r.fillUniforms(cam);

    REQUIRE_THAT(u.radius, WithinAbs(3.5, 0.001));
    REQUIRE_THAT(u.noiseStr, WithinAbs(0.05, 0.001));
    REQUIRE_THAT(u.sunInt, WithinAbs(4.0, 0.001));
    REQUIRE_THAT(u.planetX, WithinAbs(1.0, 0.001));
    REQUIRE_THAT(u.planetY, WithinAbs(2.0, 0.001));
    REQUIRE_THAT(u.planetZ, WithinAbs(-10.0, 0.001));
}

TEST_CASE("RendererBase::generateNoiseData produces deterministic 512^3 noise", "[render]") {
    auto data = RendererBase::generateNoiseData();
    REQUIRE(data.size() == 512 * 512 * 512);
    // Verify determinism: first few bytes should be consistent
    REQUIRE(data[0] == static_cast<uint8_t>(0x12345678u * 1664525u + 1013904223u >> 24));
}

TEST_CASE("RendererBase::getQuadVertices returns 6 vertices with position and UV", "[render]") {
    auto verts = RendererBase::getQuadVertices();
    REQUIRE(verts.size() == 24); // 6 vertices * 4 floats (x, y, u, v)
    // First vertex: (-1, -1, 0, 0)
    REQUIRE_THAT(verts[0], WithinAbs(-1.0, 0.001));
    REQUIRE_THAT(verts[1], WithinAbs(-1.0, 0.001));
}

TEST_CASE("RendererBase time management", "[render]") {
    struct TestRenderer : RendererBase {
        void init(int, int, void*) override {}
        void resize(int, int) override {}
        void beginFrame() override {}
        void render(const Camera&) override {}
        void endFrame() override {}
    };

    TestRenderer r;
    REQUIRE(r.isPaused() == false);
    REQUIRE_THAT(r.timeScale(), WithinAbs(1.0, 0.001));

    r.setPaused(true);
    REQUIRE(r.isPaused() == true);

    r.setTimeScale(2.5f);
    REQUIRE_THAT(r.timeScale(), WithinAbs(2.5, 0.001));
}
```

**Step 2: Run test to verify it fails**

Run: `cd /cooking && cmake --build build --target render_tests 2>&1 | tail -20`
Expected: FAIL — `RendererBase.hpp` doesn't exist.

**Step 3: Create RendererBase header**

```cpp
// src/render/RendererBase.hpp
#pragma once

#include "render/IRenderer.hpp"
#include "render/VulkanTypes.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <cstdint>
#include <span>

namespace astrocore {

class Camera;

class RendererBase : public IRenderer {
public:
    virtual ~RendererBase() = default;

    // IRenderer interface (params)
    PlanetParams& params() override { return params_; }

    // State management
    void setPlanetPosition(const glm::vec3& pos) { planetPosition_ = pos; }
    const glm::vec3& planetPosition() const { return planetPosition_; }

    void setPaused(bool paused) { paused_ = paused; }
    bool isPaused() const { return paused_; }

    void setTimeScale(float scale) { timeScale_ = scale; }
    float timeScale() const { return timeScale_; }

    void setEmissive(bool emissive) { emissive_ = emissive; }
    bool isEmissive() const { return emissive_; }

    // Shared UBO filling: PlanetParams + Camera -> PlanetUniformsVk
    PlanetUniformsVk fillUniforms(const Camera& camera);

    // Advance time (call once per frame from render())
    void advanceTime(float dt);
    float currentTime() const { return time_; }

    // Static utilities shared by both renderers
    static std::vector<uint8_t> generateNoiseData();   // 512^3 R8 deterministic noise
    static std::vector<float> getQuadVertices();        // 6 verts, 4 floats each (x,y,u,v)
    static std::string resolveAssetPath(const std::string& relative);

protected:
    PlanetParams params_;
    glm::vec3 planetPosition_{0.0f, 0.0f, -10.0f};
    float time_ = 0.0f;
    float timeScale_ = 1.0f;
    bool paused_ = false;
    bool emissive_ = false;
    int width_ = 0;
    int height_ = 0;
};

}  // namespace astrocore
```

**Step 4: Create RendererBase implementation**

```cpp
// src/render/RendererBase.cpp
#include "render/RendererBase.hpp"
#include "render/Camera.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cstring>
#include <fstream>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace astrocore {

void RendererBase::advanceTime(float dt) {
    if (!paused_) {
        time_ += dt * timeScale_;
    }
}

PlanetUniformsVk RendererBase::fillUniforms(const Camera& camera) {
    PlanetUniformsVk u{};

    // Inverse view matrix
    glm::mat4 inv = glm::inverse(camera.getViewMatrix());
    std::memcpy(u.invView, &inv[0][0], sizeof(u.invView));

    // Planet rotation matrix
    float angle = time_ * params_.rotationSpeed + params_.rotationOffset;
    float c = glm::cos(angle), s = glm::sin(angle);
    glm::mat3 rot(glm::vec3(c, 0, s), glm::vec3(0, 1, 0), glm::vec3(-s, 0, c));
    u.planetRot_col0[0] = rot[0][0]; u.planetRot_col0[1] = rot[0][1]; u.planetRot_col0[2] = rot[0][2]; u.planetRot_col0[3] = 0.f;
    u.planetRot_col1[0] = rot[1][0]; u.planetRot_col1[1] = rot[1][1]; u.planetRot_col1[2] = rot[1][2]; u.planetRot_col1[3] = 0.f;
    u.planetRot_col2[0] = rot[2][0]; u.planetRot_col2[1] = rot[2][1]; u.planetRot_col2[2] = rot[2][2]; u.planetRot_col2[3] = 0.f;

    // Camera
    glm::vec3 cam = camera.getPosition();
    u.camPosX = cam.x; u.camPosY = cam.y; u.camPosZ = cam.z;
    u.time = time_;

    // Planet
    u.planetX = planetPosition_.x;
    u.planetY = planetPosition_.y;
    u.planetZ = planetPosition_.z;
    u.radius  = params_.radius;

    u.resX = float(width_); u.resY = float(height_);
    u.rotOffset = params_.rotationOffset;
    u.rotSpeed  = params_.rotationSpeed;

    // Noise
    u.noiseStr    = params_.noiseStrength;
    u.quality     = params_.quality;
    u.terrainScale = params_.terrainScale;
    u.domainWarp  = params_.domainWarpStrength;

    // FBM
    u.fbmPersist = params_.fbmPersistence;
    u.fbmLac     = params_.fbmLacunarity;
    u.fbmExp     = params_.fbmExponentiation;
    u.fbmOct     = float(params_.fbmOctaves);

    // Terrain features
    u.ridged     = params_.ridgedStrength;
    u.crater     = params_.craterStrength;
    u.continent  = params_.continentScale;
    u.waterLevel = params_.waterLevel;

    // Banding / polar
    u.bandStr  = params_.bandingStrength;
    u.bandFreq = params_.bandingFrequency;
    u.polarCap = params_.polarCapSize;

    // Clouds
    u.cloudDensity = params_.cloudsDensity;
    u.cloudScale   = params_.cloudsScale;
    u.cloudSpeed   = params_.cloudsSpeed;
    u.cloudAlt     = params_.cloudAltitude;
    u.cloudThick   = params_.cloudThickness;
    u.sunInt       = params_.sunIntensity;
    u.ambLight     = params_.ambientLight;
    u.atmoDensity  = params_.atmosphereDensity;

    // Colors (helper lambda)
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

    // Biome levels
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

    // Extra params
    u.noiseType     = float(static_cast<int>(params_.noiseType));
    u.continentBlend = params_.continentBlend;
    u.isEmissive    = emissive_ ? 1.0f : 0.0f;

    return u;
}

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

std::string RendererBase::resolveAssetPath(const std::string& relative) {
#ifdef __EMSCRIPTEN__
    return relative;  // Emscripten uses virtual filesystem
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

}  // namespace astrocore
```

**Step 5: Update IRenderer to include state methods**

Add these pure virtual methods to `IRenderer` in `src/render/IRenderer.hpp`:

```cpp
    virtual void setPlanetPosition(const glm::vec3& pos) = 0;
    virtual void setPaused(bool paused) = 0;
    virtual bool isPaused() const = 0;
    virtual void setTimeScale(float scale) = 0;
    virtual float timeScale() const = 0;
    virtual void setEmissive(bool emissive) = 0;
```

**Step 6: Refactor VulkanRenderer to inherit RendererBase**

In `VulkanRenderer.hpp`, change `class VulkanRenderer : public IRenderer` to `class VulkanRenderer : public RendererBase`.

Remove from VulkanRenderer: `params()`, `setPlanetPosition()`, `setPaused()`, `isPaused()`, `setTimeScale()`, `timeScale()`, `setEmissive()`.

In `VulkanRenderer::Impl`, remove: `params`, `planetPosition`, `paused`, `timeScale`, `emissive`, `time`.

In `VulkanRenderer::render()`, replace the 90-line UBO fill block (lines 1071-1162) with:

```cpp
    advanceTime(0.016f);
    PlanetUniformsVk u = fillUniforms(camera);
    std::memcpy(m_impl->uniformMapped[f], &u, sizeof(u));
```

In `VulkanRenderer::init()`, set `width_` and `height_` from RendererBase. In `VulkanRenderer::resize()`, update `width_` and `height_`.

Remove `resolveAssetPath` and `readFile` static functions from VulkanRenderer.cpp — use `RendererBase::resolveAssetPath()` instead.

Replace the noise generation block (lines 647-656) with `auto noiseData = generateNoiseData();` and use `noiseData.data()` for the staging upload.

Replace the vertex data literal (lines 560-568) with `auto verts = getQuadVertices();` and use `verts.data()`, `verts.size() * sizeof(float)`.

**Step 7: Update CMakeLists.txt to include RendererBase.cpp**

Add `src/render/RendererBase.cpp` to the `ASTROCORE_SOURCES` list and `tests/render/test_renderer_base.cpp` to `render_tests`.

**Step 8: Build and run tests**

Run: `cd /cooking && cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build --target render_tests && cd build && ctest -R render_tests -V`
Expected: All tests pass including new RendererBase tests.

**Step 9: Build and run the full desktop app**

Run: `cd /cooking && cmake --build build --target astrodex && ./build/astrodex`
Expected: Desktop Vulkan app works identically to before refactoring.

**Step 10: Commit**

```bash
git add src/render/RendererBase.hpp src/render/RendererBase.cpp \
        src/render/IRenderer.hpp src/render/VulkanRenderer.hpp \
        src/render/VulkanRenderer.cpp CMakeLists.txt \
        tests/render/test_renderer_base.cpp tests/CMakeLists.txt
git commit -m "refactor: extract RendererBase from VulkanRenderer

Shared base class with UBO filling, noise generation, quad vertices,
asset path resolution, and state management. VulkanRenderer now
inherits RendererBase. No functional changes to desktop rendering."
```

---

## Task 2: Port planet_vk.frag to WGSL (planet_web.wgsl)

Translate the 1011-line GLSL 450 fragment shader to WGSL, plus the vertex shader.

**Files:**
- Create: `shaders/planet_web.wgsl`

**Step 1: Write the WGSL shader**

The translation is mechanical. Key changes from GLSL 450 to WGSL:

1. Replace all 53 `#define` accessor macros with direct `u.field.component` access
2. `vec3` -> `vec3f`, `mat4` -> `mat4x4<f32>`, `mat3` -> `mat3x3<f32>`
3. Separate texture and sampler bindings (5 bindings total)
4. `texture()` -> `textureSample()`
5. `atan(y, x)` -> `atan2(y, x)`
6. `mod(a, b)` -> `(a - b * floor(a / b))` (WGSL `%` is truncated, GLSL `mod` is floored)
7. Struct and function syntax differences
8. WGSL requires explicit return types, parameter types
9. `in/out` parameters -> return values or pointers
10. Entry points use `@vertex` / `@fragment` attributes

The UBO struct must match the 544-byte std140 layout. In WGSL, the mat3 stored as 3 x vec4 becomes:

```wgsl
struct PlanetUniforms {
    invView: mat4x4<f32>,           // 64 bytes
    planetRot_col0: vec4f,          // 16 bytes (xyz used, w padding)
    planetRot_col1: vec4f,          // 16 bytes
    planetRot_col2: vec4f,          // 16 bytes
    cameraPos_time: vec4f,          // ...
    // ... all remaining vec4 fields identical ...
    extraParams: vec4f,
};
```

The mat3 reconstruction in WGSL: `mat3x3<f32>(u.planetRot_col0.xyz, u.planetRot_col1.xyz, u.planetRot_col2.xyz)`.

The full shader file is ~1050 lines of WGSL. Write the complete translation of all functions:
- `noise()`, `fbm()`, `ridgedFBM()`, `craterNoise()`, `worleyNoise()`, `billowyFBM()`, `swissFBM()`, `voronoiTerrain()`, `hybridFBM()`
- `cloudBaseFBM()`, `cloudDetailFBM()`, `cloudNoise()`, `cloudNoiseCheap()`
- `planetNoise()`, `planetDist()`, `planetNormal()`
- `spaceColor()`, `stellarNoise()`, `renderStar()`
- `blackbodyColor()`, `traceBlackHole()`
- `simpleReinhardToneMapping()`, `atmosphereColor()`
- `marchCloudSegment()`, `volumetricClouds()`
- `intersectPlanet()`, `radiance()`
- Vertex and fragment entry points

**Step 2: Validate WGSL syntax**

Run: `npx @aspect-build/rules_ts//ts/wgsl-validate shaders/planet_web.wgsl` (or use Tint validator if available)

If no validator is available, syntax will be validated when WebGPURenderer tries to create the shader module (Task 3).

**Step 3: Commit**

```bash
git add shaders/planet_web.wgsl
git commit -m "feat: add WGSL shader port of planet_vk.frag

Complete manual translation of 1011-line GLSL 450 fragment shader to
WGSL. Includes all 7 noise types, volumetric clouds, atmosphere,
black hole geodesics, and star rendering. Same UBO layout (544 bytes)."
```

---

## Task 3: Implement WebGPURenderer

Create the WebGPU rendering backend.

**Files:**
- Create: `src/render/WebGPURenderer.hpp`
- Create: `src/render/WebGPURenderer.cpp`

**Step 1: Create WebGPURenderer header**

```cpp
// src/render/WebGPURenderer.hpp
#pragma once

#include "render/RendererBase.hpp"
#include <webgpu/webgpu.h>
#include <memory>

namespace astrocore {

class Camera;

class WebGPURenderer : public RendererBase {
public:
    WebGPURenderer();
    ~WebGPURenderer();

    void init(int width, int height, void* glfwWindow = nullptr) override;
    void resize(int width, int height) override;

    void beginFrame() override;
    void render(const Camera& camera) override;
    void endFrame() override;

    // WebGPU accessors for ImGui integration
    WGPUDevice getDevice();
    WGPUTextureFormat getSurfaceFormat();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace astrocore
```

**Step 2: Create WebGPURenderer implementation**

The implementation covers:
1. `init()`: Request adapter (async via emscripten), request device, configure surface, create shader module from embedded WGSL, create bind group layout (5 bindings), create pipeline layout, create render pipeline, create vertex buffer, create uniform buffer, generate + upload 3D noise texture, load + upload cubemap, create samplers, create bind group
2. `resize()`: Reconfigure surface with new width/height
3. `beginFrame()`: Get current surface texture, create texture view, create command encoder, begin render pass with the texture view as color attachment
4. `render()`: Call `advanceTime(0.016f)`, call `fillUniforms(camera)`, write UBO via `wgpuQueueWriteBuffer`, set pipeline, set bind group, draw 6 vertices
5. `endFrame()`: End render pass, finish command encoder, submit command buffer

Key WebGPU specifics:
- `WGPUTextureFormat_BGRA8Unorm` for surface
- `WGPUTextureFormat_R8Unorm` for 3D noise texture
- `WGPUTextureFormat_RGBA8Unorm` for cubemap faces
- Vertex buffer layout: 2 attributes (position vec2f, uv vec2f), stride 16
- Bind group layout: uniform buffer + texture_3d + sampler + texture_cube + sampler

**Step 3: Commit**

```bash
git add src/render/WebGPURenderer.hpp src/render/WebGPURenderer.cpp
git commit -m "feat: add WebGPURenderer backend

WebGPU rendering pipeline implementing RendererBase. Fullscreen quad
with WGSL shader, 3D noise texture, cubemap starfield, and 544-byte
UBO. Designed for Emscripten browser deployment."
```

---

## Task 4: Emscripten Build System Integration

Add Emscripten toolchain support to CMakeLists.txt.

**Files:**
- Modify: `CMakeLists.txt` (conditional Emscripten vs Vulkan)
- Modify: `cmake/Dependencies.cmake` (skip Vulkan deps for Emscripten)
- Create: `web/shell.html` (minimal HTML host page)
- Modify: `Makefile` (add `make web` target)

**Step 1: Modify CMakeLists.txt**

Wrap the `find_package(Vulkan)` and Vulkan-specific dependencies in `if(NOT EMSCRIPTEN)`. Add Emscripten-specific source list that includes WebGPURenderer instead of VulkanRenderer. Add Emscripten link flags: `-sUSE_WEBGPU=1 -sUSE_GLFW=3 -sALLOW_MEMORY_GROWTH=1 --preload-file assets/`.

Key structure:

```cmake
if(EMSCRIPTEN)
    # WebGPU sources (no VulkanRenderer, no VMA, no vk-bootstrap)
    list(APPEND ASTROCORE_SOURCES src/render/WebGPURenderer.cpp)
    # Link flags for Emscripten
    target_link_options(astrodex PRIVATE
        -sUSE_WEBGPU=1 -sUSE_GLFW=3 -sALLOW_MEMORY_GROWTH=1
        -sEXPORTED_RUNTIME_METHODS=['ccall'] --preload-file assets/)
    # ImGui WebGPU backend instead of Vulkan
else()
    find_package(Vulkan REQUIRED)
    list(APPEND ASTROCORE_SOURCES src/render/VulkanRenderer.cpp)
    # existing Vulkan linking
endif()
```

**Step 2: Create web/shell.html**

Minimal HTML page that loads the WASM module and provides a canvas:

```html
<!DOCTYPE html>
<html><head>
    <meta charset="utf-8">
    <title>AstroSplat</title>
    <style>
        body { margin: 0; background: #000; overflow: hidden; }
        canvas { width: 100vw; height: 100vh; display: block; }
    </style>
</head><body>
    <canvas id="canvas" oncontextmenu="event.preventDefault()"></canvas>
    <script>
        var Module = {
            canvas: document.getElementById('canvas'),
            onRuntimeInitialized: function() { console.log('AstroSplat loaded'); }
        };
    </script>
    <script src="astrodex.js"></script>
</body></html>
```

**Step 3: Add Makefile target**

```makefile
web:
	emcmake cmake -B build_web -DCMAKE_BUILD_TYPE=Release
	cmake --build build_web
	@echo "Open build_web/astrodex.html in Chrome"
```

**Step 4: Verify desktop build still works**

Run: `cd /cooking && cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build --target astrodex`
Expected: Desktop build succeeds with no regressions.

**Step 5: Commit**

```bash
git add CMakeLists.txt cmake/Dependencies.cmake web/shell.html Makefile
git commit -m "build: add Emscripten WebGPU build target

Conditional CMake: Vulkan for desktop, WebGPU for Emscripten.
Adds web/shell.html host page and 'make web' build target."
```

---

## Task 5: Adapt Application Main Loop for Emscripten

Make the application's run loop compatible with Emscripten's single-threaded event model.

**Files:**
- Modify: `src/core/Application.hpp` (use IRenderer/RendererBase instead of VulkanRenderer)
- Modify: `src/core/Application.cpp` (conditional renderer creation, emscripten main loop)
- Modify: `src/ui/UIManager.hpp` (accept RendererBase* instead of VulkanRenderer*)
- Modify: `src/ui/UIManager.cpp` (conditional ImGui backend init: Vulkan vs WebGPU)

**Step 1: Update Application.hpp**

Change `std::unique_ptr<VulkanRenderer> m_renderer` to `std::unique_ptr<RendererBase> m_renderer`. Add forward declaration for `RendererBase` instead of `VulkanRenderer`.

**Step 2: Update Application.cpp renderer creation**

```cpp
#ifdef __EMSCRIPTEN__
    #include "render/WebGPURenderer.hpp"
    m_renderer = std::make_unique<WebGPURenderer>();
    LOG_INFO("Using WebGPU rendering backend");
#else
    #include "render/VulkanRenderer.hpp"
    m_renderer = std::make_unique<VulkanRenderer>();
    LOG_INFO("Using Vulkan rendering backend");
#endif
```

**Step 3: Adapt run() for Emscripten**

Replace the `while (!shouldClose)` loop with Emscripten's callback:

```cpp
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
static Application* g_app = nullptr;
static void emMainLoop() { g_app->tick(); }
void Application::run() {
    g_app = this;
    emscripten_set_main_loop(emMainLoop, 0, true);
}
#else
void Application::run() {
    // existing while loop
}
#endif
```

Extract one iteration of the run loop into `Application::tick()` for Emscripten compatibility.

**Step 4: Update UIManager for backend-agnostic ImGui init**

```cpp
#ifdef __EMSCRIPTEN__
    #include <imgui_impl_wgpu.h>
    ImGui_ImplGlfw_InitForOther(window, true);
    ImGui_ImplWGPU_InitInfo wgpuInfo{};
    wgpuInfo.Device = static_cast<WebGPURenderer*>(renderer)->getDevice();
    wgpuInfo.RenderTargetFormat = static_cast<WGPUTextureFormat>(
        static_cast<WebGPURenderer*>(renderer)->getSurfaceFormat());
    ImGui_ImplWGPU_Init(&wgpuInfo);
#else
    ImGui_ImplGlfw_InitForVulkan(window, true);
    // existing Vulkan ImGui init
#endif
```

Similarly update `beginFrame()`, `endFrame()`, and `shutdown()` with `#ifdef __EMSCRIPTEN__` guards for the WebGPU ImGui backend calls.

**Step 5: Handle data API differences**

For the web build, disable or stub out libcurl-dependent code:
- `NasaApiClient`, `OecClient`, `ExoMastClient`, etc. — wrap their implementations in `#ifndef __EMSCRIPTEN__` or provide web-compatible stubs
- For demo mode, ship a few preset planet configurations as embedded JSON

**Step 6: Commit**

```bash
git add src/core/Application.hpp src/core/Application.cpp \
        src/ui/UIManager.hpp src/ui/UIManager.cpp
git commit -m "feat: adapt Application and UIManager for Emscripten

Conditional renderer creation (WebGPU vs Vulkan), Emscripten main
loop callback, and backend-agnostic ImGui initialization."
```

---

## Task 6: Asset Pipeline and First Browser Test

Generate pre-baked assets and test the web build end-to-end.

**Files:**
- Verify: `assets/` directory has cubemap face files
- Create: `web/serve.py` (simple HTTP server for testing)

**Step 1: Verify assets exist**

Check that cubemap face files exist in `assets/starmap/`:
Run: `ls -la /cooking/assets/starmap/`

**Step 2: Build the web target**

Run: `cd /cooking && emcmake cmake -B build_web -DCMAKE_BUILD_TYPE=Release && cmake --build build_web`
Expected: Produces `build_web/astrodex.html`, `build_web/astrodex.js`, `build_web/astrodex.wasm`, `build_web/astrodex.data`

**Step 3: Serve and test in Chrome**

```python
# web/serve.py
import http.server, ssl, sys
handler = http.server.SimpleHTTPRequestHandler
handler.extensions_map.update({'.wasm': 'application/wasm', '.data': 'application/octet-stream'})
server = http.server.HTTPServer(('localhost', 8080), handler)
print('Serving at http://localhost:8080')
server.serve_forever()
```

Run: `cd /cooking/build_web && python3 ../web/serve.py`
Open: `http://localhost:8080/astrodex.html` in Chrome (must have WebGPU enabled)

Expected: Spinning procedural planet with ImGui controls visible.

**Step 4: Debug any shader compilation errors**

Check browser console for WGSL compilation errors. Common issues:
- Loop uniformity warnings -> add `@diagnostic(off, derivative_uniformity)`
- Type mismatches -> ensure all `f32` suffixes are correct
- Binding mismatches -> verify bind group layout matches WGSL declarations

**Step 5: Commit**

```bash
git add web/serve.py
git commit -m "feat: complete WebGPU browser deployment pipeline

Asset pipeline verified, web build produces working WASM+WebGPU
application. Tested in Chrome with planet rendering and ImGui overlay."
```

---

## Task 7: Polish and Verification

Final verification and cross-browser testing.

**Files:**
- Modify: `shaders/planet_web.wgsl` (any fixes from browser testing)
- Modify: `src/render/WebGPURenderer.cpp` (any fixes)
- Modify: `README.md` (add web build instructions)

**Step 1: Test all planet presets in browser**

Cycle through all 8 presets (Earth, Mars, Mercury, Jupiter, etc.) and verify visual parity with desktop.

**Step 2: Test black hole mode**

Enable black hole parameters and verify geodesic ray tracing works in WGSL.

**Step 3: Test star/emissive mode**

Verify emissive star rendering with limb darkening and granulation animation.

**Step 4: Performance check**

Compare frame times between desktop Vulkan and browser WebGPU. The quality slider should allow lowering quality for slower GPUs.

**Step 5: Add web build instructions to README**

```markdown
## Web Build (WebGPU)

Requires [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) 3.x+.

```bash
make web
cd build_web && python3 -m http.server 8080
# Open http://localhost:8080/astrodex.html in Chrome
```

Requires a browser with WebGPU support (Chrome 113+, Edge 113+, Firefox Nightly).
```

**Step 6: Final commit**

```bash
git add -A
git commit -m "feat: complete WebGPU + WASM port of planet renderer

Full browser deployment with visual parity to desktop Vulkan:
- Manual WGSL shader translation (1011 lines)
- WebGPURenderer with shared RendererBase
- Emscripten build integration
- ImGui controls via WebGPU backend
- Pre-baked 3D noise and cubemap assets"
```

---

## Summary

| Task | Description | Estimated Complexity |
|------|-------------|---------------------|
| 1 | Extract RendererBase | Medium (refactor, tests) |
| 2 | WGSL shader port | Large (1011 lines translation) |
| 3 | WebGPURenderer | Large (new renderer ~600 lines) |
| 4 | Emscripten build system | Medium (CMake conditionals) |
| 5 | Application/UIManager adaptation | Medium (ifdef guards, main loop) |
| 6 | Asset pipeline + browser test | Small (integration testing) |
| 7 | Polish and verification | Small (bug fixes, docs) |

**Critical path:** Task 1 -> Task 2 (parallel with 3) -> Task 4 -> Task 5 -> Task 6 -> Task 7

Tasks 2 and 3 can be developed in parallel since the shader and renderer are independent until integration.
