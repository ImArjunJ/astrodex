# WebGPU + WASM Port Design

**Date:** 2026-03-01
**Branch:** refactor/ai
**Goal:** Port AstroSplat's procedural planet renderer to the browser via WebGPU + Emscripten WASM

## Decisions

- **Target:** Interactive demo first, feature parity path
- **Textures:** Pre-baked 3D noise (128^3 RGBA) + cubemap starfield as static assets
- **UI:** ImGui via Emscripten (reuse existing UIManager.cpp)
- **Build:** Same CMakeLists.txt with Emscripten toolchain target
- **Shader:** Manual GLSL 450 -> WGSL translation (not auto-transpiled)

## Architecture

### Shared Base Class: RendererBase

Extract common logic from VulkanRenderer into RendererBase (~250 lines):

```
RendererBase : public IRenderer
  State:
    PlanetParams params_
    glm::vec3 planetPosition_
    float time_, timeScale_
    bool paused_, emissive_

  Shared Methods:
    fillUniforms(Camera&) -> PlanetUniformsVk    // 90-line UBO conversion
    resolveAssetPath(string) -> string            // with __EMSCRIPTEN__ variant
    readFile(string) -> vector<char>              // binary file reader
    getQuadVertices() -> span<float>              // fullscreen quad (6 verts)
    loadNoiseTexture() -> {pixels, w, h, d}       // stb_image to CPU buffer
    loadCubemap() -> {6 faces, w, h}              // stb_image to CPU buffer
    params() -> PlanetParams&
    setPlanetPosition(), setPaused(), setTimeScale(), setEmissive()

  Pure Virtual (API-specific):
    init(), resize(), beginFrame(), render(), endFrame()
```

### VulkanRenderer : public RendererBase

Existing renderer refactored to inherit from RendererBase. All Vulkan-specific code
(swapchain, pipeline, descriptors, VMA, command buffers) stays in VulkanRenderer.

### WebGPURenderer : public RendererBase

New renderer (~600 lines) implementing WebGPU pipeline:

- `init()`: Request adapter/device (async), create surface, pipeline, bind group layout, load WGSL shader, upload textures and UBO
- `resize()`: Reconfigure surface
- `beginFrame()`: Get current surface texture, create command encoder, begin render pass
- `render()`: Call fillUniforms(), write UBO buffer, set bind group, draw 6 vertices
- `endFrame()`: End render pass, submit command buffer, present

### Conditional Compilation

```cpp
// Application.cpp
#ifdef __EMSCRIPTEN__
    auto renderer = std::make_unique<WebGPURenderer>();
#else
    auto renderer = std::make_unique<VulkanRenderer>();
#endif
```

## WGSL Shader Translation

### Syntax Mapping

| GLSL 450 | WGSL |
|-----------|------|
| vec2/vec3/vec4 | vec2f/vec3f/vec4f |
| mat3/mat4 | mat3x3<f32>/mat4x4<f32> |
| layout(set=0, binding=0) uniform | @group(0) @binding(0) var<uniform> |
| texture(sampler3D, coord) | textureSample(t, s, coord) |
| texture(samplerCube, dir) | textureSample(t, s, dir) |
| mix/clamp/smoothstep | same |
| atan(y, x) | atan2(y, x) |
| mod(x, y) | x - y * floor(x / y) |
| #define macros | direct struct member access |

### Binding Layout (5 bindings, texture/sampler split)

- @group(0) @binding(0): uniform buffer (544 bytes)
- @group(0) @binding(1): texture_3d<f32> (noise)
- @group(0) @binding(2): sampler (noise)
- @group(0) @binding(3): texture_cube<f32> (starmap)
- @group(0) @binding(4): sampler (starmap)

### UBO Struct

Same 544-byte layout as Vulkan std140. mat3 stored as 3x vec4 with explicit padding.
Use `@size` and `@align` WGSL annotations to match.

## Emscripten Build Integration

### CMakeLists.txt

```cmake
if(EMSCRIPTEN)
    set(ASTRO_WEB_SOURCES
        src/render/RendererBase.cpp
        src/render/WebGPURenderer.cpp
        # ... shared sources (Camera, PlanetParams, UIManager, etc.)
    )
    target_compile_options(astrodex PRIVATE -sUSE_WEBGPU=1 -sUSE_GLFW=3)
    target_link_options(astrodex PRIVATE
        -sUSE_WEBGPU=1 -sUSE_GLFW=3 -sALLOW_MEMORY_GROWTH=1
        --preload-file assets/
    )
    # WGSL shader embedded or preloaded, no SPIR-V compilation
else()
    find_package(Vulkan REQUIRED)
    include(ShaderCompilation)
    # ... existing Vulkan build
endif()
```

### Main Loop

```cpp
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(mainLoopCallback, &app, 0, true);
#else
    while (!window.shouldClose()) { app.tick(); }
#endif
```

### Data APIs

- Demo mode: no network calls, ship preset planets as embedded JSON
- Feature parity: emscripten_fetch replaces libcurl for NASA/OEC APIs

## ImGui Integration

ImGui ships with `imgui_impl_wgpu.cpp` + `imgui_impl_glfw.cpp` backends.
UIManager.cpp compiles unchanged — it's pure ImGui API calls.

Initialization in WebGPURenderer::init() or Application.cpp with #ifdef:
- ImGui_ImplGlfw_InitForOther(window, true)
- ImGui_ImplWGPU_InitInfo with WebGPU device, format, depth format

## Phased Implementation

1. **RendererBase extraction** — Refactor VulkanRenderer, extract shared logic, verify desktop still works
2. **WGSL shader port** — Translate planet_vk.frag -> planet_web.wgsl (1011 lines)
3. **WebGPURenderer** — Implement WebGPU pipeline with fullscreen quad
4. **Emscripten CMake** — Build system integration, main loop adaptation
5. **ImGui overlay** — WebGPU backend init, verify UIManager works
6. **Asset pipeline** — Pre-baked textures, embedded presets, static file serving
7. **Feature parity** — Catalogue, data APIs, presets (future milestone)

## Risks and Mitigations

| Risk | Mitigation |
|------|-----------|
| WebGPU not available | Feature-detect at startup, show fallback message |
| 3D texture unsupported on device | Check maxTextureDimension3D, fallback to 2D atlas |
| WGSL loop uniformity analysis | @diagnostic(off, derivative_uniformity) |
| Large WASM binary | Strip debug, -Oz, lazy-load data features |
| Mobile GPU perf | Quality slider (already in shader), default low on mobile |
| Emscripten WebGPU maturity | Pin emscripten version, test on Chrome stable |

## Files to Create/Modify

### New Files
- `src/render/RendererBase.hpp` — shared base class header
- `src/render/RendererBase.cpp` — shared base class implementation
- `src/render/WebGPURenderer.hpp` — WebGPU renderer header
- `src/render/WebGPURenderer.cpp` — WebGPU renderer implementation
- `shaders/planet_web.wgsl` — WGSL fragment + vertex shader
- `web/index.html` — minimal HTML host page
- `web/shell.html` — Emscripten shell template (optional)

### Modified Files
- `src/render/VulkanRenderer.cpp` — refactor to inherit RendererBase
- `src/render/VulkanRenderer.hpp` — change base class
- `src/render/IRenderer.hpp` — may add virtual methods for ImGui hooks
- `src/core/Application.cpp` — conditional renderer creation, main loop
- `CMakeLists.txt` — Emscripten target, conditional dependencies
- `cmake/Dependencies.cmake` — skip Vulkan deps for Emscripten
- `Makefile` — add `make web` target
