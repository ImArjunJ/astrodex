# Architecture

**Analysis Date:** 2026-02-28

## Pattern Overview

**Overall:** Layered Desktop Application with Real-time Graphics Rendering

**Key Characteristics:**
- Single-threaded main loop driving rendering and UI updates
- Separation of concerns across functional layers (core, render, UI, data, AI)
- Immediate-mode GUI (ImGui) for interactive parameter control
- Procedural shader-based planet generation in real-time
- Optional async data integration with external APIs and AI services

## Layers

**Core Layer:**
- Purpose: Application lifecycle management, window/input handling, and cross-cutting concerns (logging)
- Location: `src/core/`
- Contains: Application entry point, GLFW window wrapper, logging infrastructure
- Depends on: GLFW, spdlog
- Used by: All other layers depend on core components

**Render Layer:**
- Purpose: Real-time 3D graphics rendering, camera manipulation, shader management
- Location: `src/render/`
- Contains: Renderer (OpenGL state), Camera (orbit controls), ShaderProgram (GPU code management), procedural noise textures, planet parameters
- Depends on: OpenGL 4.5, GLM (math library), GLAD (OpenGL loader)
- Used by: Application (main render loop), UIManager (parameter binding)

**UI Layer:**
- Purpose: Interactive parameter editing and user feedback
- Location: `src/ui/`
- Contains: UIManager with ImGui-based planet editor, preset system for quick configuration
- Depends on: ImGui, GLFW callbacks
- Used by: Application (frame-based rendering)

**Data Layer (Optional):**
- Purpose: Integration with external data sources for real exoplanet data
- Location: `src/data/`
- Contains: NasaApiClient (NASA TAP endpoint queries), ExoplanetData model
- Depends on: CURL or similar HTTP client, JSON parsing, caching infrastructure
- Used by: Not actively used in core application loop; available for future features

**AI Layer (Optional):**
- Purpose: Machine learning inference for parameter inference and image generation
- Location: `src/ai/`
- Contains: InferenceEngine (orchestrates AI tasks), BedrockClient (AWS Bedrock integration), PromptTemplates (structured prompts)
- Depends on: AWS SDK, JSON parsing, optional image generation
- Used by: Not actively used in core application loop; available for batch operations

## Data Flow

**Primary Render Loop:**

1. **Input**: Application polls GLFW events (keyboard, mouse, window resize)
2. **Update**: Application processes mouse drag for camera orbit, zoom via scroll wheel
3. **Render**: Renderer executes GPU shader to generate procedural planet surface
4. **UI Overlay**: UIManager renders ImGui panel for parameter adjustment
5. **Output**: Window swaps buffers and presents to screen

**Parameter Update Flow:**

1. User adjusts slider in UIManager
2. ImGui modifies `PlanetParams` struct in Renderer
3. Next frame, Renderer passes updated params to GPU via shader uniforms
4. Shader immediately applies new parameters to procedural generation

**Optional Async Data Flow (not in main loop):**

1. User triggers external query (exoplanet search, AI inference)
2. NasaApiClient or InferenceEngine runs query asynchronously
3. Results are cached and can be applied to planet parameters
4. Main render loop continues unblocked

**State Management:**
- Central: `PlanetParams` struct in Renderer holds all procedural generation parameters
- Input State: Mouse/keyboard state queried directly from Window on demand
- Camera State: Camera stores orbit parameters (yaw, pitch, distance) updated by user input
- UI State: ImGui maintains preset index and widget states; non-persistent (lost on exit)
- Render State: OpenGL resources (VAO, VBO, textures, shaders) managed by Renderer

## Key Abstractions

**Renderer:**
- Purpose: Encapsulates OpenGL rendering pipeline and procedural planet generation
- Examples: `src/render/Renderer.hpp`, `src/render/ShaderProgram.hpp`
- Pattern: RAII (Resource Acquisition Is Initialization) for GPU resources; deferred rendering via quad mesh and full-screen shader

**Camera:**
- Purpose: Manages view and projection matrices; implements orbit control (rotate/zoom/pan)
- Examples: `src/render/Camera.hpp`
- Pattern: Cached matrix generation; only recalculates on state change

**Window:**
- Purpose: GLFW wrapper providing callback-based input handling and framebuffer operations
- Examples: `src/core/Window.hpp`
- Pattern: Callback function pointers; static methods marshaling to instance methods

**PlanetParams:**
- Purpose: Single struct holding all ~100 procedural generation parameters (terrain, colors, lighting, atmosphere, black hole physics)
- Examples: `src/render/PlanetParams.hpp`
- Pattern: Direct struct modification; no validation or clamping in core (UI layer responsible for safe ranges)

**UIManager:**
- Purpose: ImGui-based editor providing real-time parameter sliders and presets
- Examples: `src/ui/UIManager.hpp`
- Pattern: Immediate-mode rendering; no retained state except preset index; directly modifies external PlanetParams reference

## Entry Points

**main() → Application::run():**
- Location: `src/main.cpp`, orchestrated in `src/core/Application.cpp`
- Triggers: Program startup
- Responsibilities:
  1. Initializes core components (Window, Renderer, Camera, UIManager)
  2. Enters main loop (poll events → update → render)
  3. Shutdown and resource cleanup on exit

**Application::run():**
- Location: `src/core/Application.cpp` lines 56-66
- Triggers: Called once from main()
- Responsibilities:
  1. Frame loop: query time, poll events, update camera, render, swap buffers
  2. Driven by Window::shouldClose() sentinel

**Input Callbacks:**
- Location: `src/core/Window.cpp`, `src/core/Application.cpp` lines 34-42, 74-93
- Triggers: GLFW events (mouse move, scroll, key press)
- Responsibilities:
  1. Window resize → camera aspect ratio update, renderer resize
  2. Scroll wheel → camera zoom
  3. Left mouse drag → camera orbit (yaw/pitch)
  4. ImGui interception → skip camera control when UI has focus

## Error Handling

**Strategy:** Exceptions for fatal initialization errors; graceful degradation for runtime issues

**Patterns:**
- **Constructor failures**: Window, Application constructors throw `std::runtime_error` if GLFW/GLAD/GL context fails (lines 12-46 in `src/core/Window.cpp`)
- **Shader compilation**: ShaderProgram logs errors via `LOG_ERROR` but returns `false`; Application continues with broken shader visualization
- **Main exception handler**: `main()` catches and logs fatal exceptions, returns exit code 1 (`src/main.cpp` lines 10-12)
- **Resource cleanup**: RAII destructors ensure GPU resources deleted (e.g., `src/render/Renderer.cpp` lines 12-16); `Application::shutdown()` calls reset() on all owned pointers in reverse initialization order

## Cross-Cutting Concerns

**Logging:**
- Framework: spdlog
- Approach: Macro-based (`LOG_INFO`, `LOG_ERROR`, etc.) initialized via `Logger::init()` in Application constructor
- Used for: Initialization status, OpenGL version/vendor reporting, shader compilation feedback

**Validation:**
- Input ranges: UIManager enforces slider bounds (presets use safe value ranges)
- Shader uniforms: ShaderProgram caches uniform locations; bad names silently use -1 (no validation)
- API responses: NasaApiClient has basic try-catch for JSON parse errors

**Camera Control:**
- ImGui integration: Application checks `ImGui::GetIO().WantCaptureMouse` before processing camera input (line 74-75 in Application.cpp)
- Constraints: Camera min/max distance, pitch clamping prevents gimbal lock

**Memory Management:**
- Ownership: All major components owned by Application via `std::unique_ptr` (Window, Renderer, Camera, UIManager)
- Initialization order: Application::init() is called in constructor to ensure proper setup before use
- Deletion order: Application::shutdown() reverses initialization order to respect dependency tree

---

*Architecture analysis: 2026-02-28*
