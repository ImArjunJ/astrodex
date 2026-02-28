# Codebase Structure

**Analysis Date:** 2026-02-28

## Directory Layout

```
cooking/
├── CMakeLists.txt           # Build configuration (C++23, dependencies, target setup)
├── README.md                # Project overview (procedural planet generator)
├── .planning/               # GSD planning artifacts (generated, not source)
├── src/                     # Source code root
│   ├── main.cpp             # Entry point: creates Application, catches exceptions
│   ├── core/                # Application lifecycle, window, logging
│   │   ├── Application.hpp/cpp   # Main app class, event loop, component lifecycle
│   │   ├── Window.hpp/cpp        # GLFW wrapper, input callbacks, OpenGL context
│   │   └── Logger.hpp/cpp        # spdlog integration, logging macros
│   ├── render/              # 3D graphics, camera, procedural generation
│   │   ├── Renderer.hpp/cpp      # OpenGL render loop, shader uniforms, noise texture
│   │   ├── Camera.hpp/cpp        # Orbit camera, view/projection matrices
│   │   ├── ShaderProgram.hpp/cpp # GPU shader compilation, uniform setter API
│   │   ├── PlanetParams.hpp/cpp  # ~100 procedural generation parameters
│   │   ├── PlanetTypes.hpp       # Enum definitions for planet presets
│   │   ├── PlanetProfiles.hpp    # Preset planet configurations (unused in render loop)
│   │   ├── SphereMesh.hpp/cpp    # (Optional) Geometric sphere mesh (not used in quad render)
│   │   ├── TextureLoader.hpp/cpp # (Optional) Texture file I/O
│   │   └── CubemapGenerator.hpp/cpp # (Optional) Cubemap generation utilities
│   ├── ui/                  # Interactive parameter editing
│   │   ├── UIManager.hpp/cpp     # ImGui integration, preset panel, parameter sliders
│   ├── data/                # External data integration (not in main loop)
│   │   ├── NasaApiClient.hpp/cpp # NASA exoplanet TAP queries, ADQL builder, caching
│   │   └── ExoplanetData.hpp/cpp # Exoplanet data model, JSON serialization
│   └── ai/                  # AI inference (not in main loop)
│       ├── InferenceEngine.hpp/cpp      # Orchestrates AWS Bedrock inference
│       ├── BedrockClient.hpp/cpp        # AWS Bedrock API wrapper
│       └── PromptTemplates.hpp          # Structured prompts for AI inference
├── shaders/                 # GLSL shader sources
│   ├── planet.vert          # Vertex shader (quad with UVs)
│   ├── planet.frag          # Fragment shader (procedural planet generation)
│   ├── planet_unified.vert  # Unified planet shader variant
│   ├── planet_unified.frag  # Unified fragment implementation
│   ├── atmosphere.vert/frag # Atmospheric effects (optional)
│   ├── planet_pbr.vert/frag # PBR rendering (optional)
│   └── planet_ai.frag       # AI-guided generation (optional)
├── assets/                  # Static resources (copied to build output)
│   └── [image/text files]
├── cmake/                   # CMake modules
│   ├── CompilerOptions.cmake # C++ flags (C++23, warnings, optimization)
│   └── Dependencies.cmake    # External library setup (GLFW, GLAD, GLM, ImGui, spdlog)
├── external/                # External dependency sources (submodules or vendored)
├── build/                   # Build artifacts (generated, not committed)
└── ref/                     # Reference documentation or assets
```

## Directory Purposes

**src/**
- Purpose: All application source code
- Contains: C++ headers (.hpp), implementations (.cpp)
- Key files: `main.cpp` (entry), `core/Application.hpp` (orchestrator)

**src/core/**
- Purpose: Application framework and windowing
- Contains: Application lifecycle management, GLFW window wrapping, logging infrastructure
- Key files: `Application.hpp` (orchestrator), `Window.hpp` (input/rendering context)

**src/render/**
- Purpose: 3D graphics rendering and procedural generation
- Contains: OpenGL API usage, camera control, shader management, procedural planet parameters
- Key files: `Renderer.hpp` (render loop), `Camera.hpp` (view control), `PlanetParams.hpp` (generation parameters)

**src/ui/**
- Purpose: Interactive user interface for parameter editing
- Contains: ImGui integration, preset selection, real-time parameter adjustment
- Key files: `UIManager.hpp` (ImGui wrapper, preset panel, sliders)

**src/data/**
- Purpose: External data integration (not actively used in main render loop)
- Contains: NASA exoplanet database queries, data model, caching
- Key files: `NasaApiClient.hpp` (TAP endpoint queries), `ExoplanetData.hpp` (data model)

**src/ai/**
- Purpose: Machine learning inference (not actively used in main render loop)
- Contains: AWS Bedrock integration, prompt templating, optional image generation
- Key files: `InferenceEngine.hpp` (orchestrates AI tasks), `PromptTemplates.hpp` (structured prompts)

**shaders/**
- Purpose: GLSL GPU code for procedural planet rendering
- Contains: Vertex and fragment shaders; primary is `planet.vert` + `planet.frag`
- Key files: `planet.frag` (implements procedural terrain, atmosphere, lighting)

**cmake/**
- Purpose: Build system configuration
- Contains: Compiler flags, external dependency setup
- Key files: `CompilerOptions.cmake` (C++23 standard, warnings), `Dependencies.cmake` (library linking)

## Key File Locations

**Entry Points:**
- `src/main.cpp`: Program entry; creates Application, catches exceptions
- `src/core/Application.cpp`: Main event loop; coordinates all components

**Core Logic:**
- `src/render/Renderer.cpp`: OpenGL render operations, shader setup, noise texture generation
- `src/render/Camera.cpp`: View matrix calculations, orbit control updates
- `src/core/Window.cpp`: GLFW initialization, event polling, callback dispatch

**Configuration:**
- `src/render/PlanetParams.hpp`: ~100 procedural generation parameters (terrain, colors, lighting, black hole physics)
- `CMakeLists.txt`: Build configuration (C++ standard, link libraries, shader/asset copying)
- `cmake/CompilerOptions.cmake`: Compiler flags, optimization levels

**Testing:**
- Not detected — no test files present in codebase

## Naming Conventions

**Files:**
- `.hpp`: C++ header files
- `.cpp`: C++ implementation files
- `*.vert`, `*.frag`: GLSL shader files
- `CMakeLists.txt`: CMake build files

**Directories:**
- Lowercase, descriptive names: `core`, `render`, `ui`, `data`, `ai`
- Grouped by functionality, not by type (e.g., `render/` contains both headers and implementations)

**Classes:**
- PascalCase: `Application`, `Renderer`, `Camera`, `Window`, `UIManager`, `ShaderProgram`
- One public class per file (e.g., `Renderer.hpp` defines class Renderer)

**Functions/Methods:**
- camelCase: `beginFrame()`, `pollEvents()`, `setPosition()`, `getViewMatrix()`
- Getters: `get*()` prefix, e.g., `getViewMatrix()`
- Setters: `set*()` prefix, e.g., `setPosition()`
- Predicates: `is*()` or `should*()`, e.g., `shouldClose()`, `isValid()`

**Variables:**
- Member variables: `m_` prefix, camelCase: `m_window`, `m_renderer`, `m_running`
- Static members: `s_` prefix: `s_coreLogger`
- Local variables: camelCase: `deltaTime`, `lastX`, `vertices`

**Types/Structs:**
- PascalCase: `PlanetParams`, `WindowConfig`, `NasaApiConfig`
- Configuration structs end in `Config`

## Where to Add New Code

**New Feature (e.g., terrain simulation, animation system):**
- Primary code: `src/render/` if graphics-related; `src/core/` if framework-related
- Tests: No test framework present; consider adding test files alongside implementations
- Example: New terrain algorithm → `src/render/TerrainSimulator.hpp/cpp`

**New Component/Module:**
- Implementation: Create new directory under `src/` if it represents a distinct layer (e.g., `src/physics/`)
- Or add to existing layer if it's an enhancement (e.g., new camera mode in `src/render/Camera.cpp`)
- Update `CMakeLists.txt` to add source files to `ASTROCORE_SOURCES` list

**New UI Parameter:**
- Add field to `PlanetParams` struct in `src/render/PlanetParams.hpp`
- Add slider/control in `UIManager::render()` method in `src/ui/UIManager.cpp`
- Apply uniform binding in `Renderer::render()` in `src/render/Renderer.cpp` (e.g., `m_shader.setFloat("paramName", m_params.fieldName)`)

**New Shader Effect:**
- Create new `.vert` and `.frag` files in `shaders/` directory
- Load in Renderer::init() by calling `m_shader.loadFromFiles("shaders/neweffect.vert", "shaders/neweffect.frag")`
- Reference new parameters via shader uniforms (must declare in PlanetParams first)

**Utilities/Helpers:**
- Shared utilities: `src/core/` (if generic) or layer-specific subdirectory
- No dedicated `utils/` directory currently; follow existing pattern of keeping helpers close to consumers

**External Integration:**
- API clients: `src/data/` (for public data APIs like NASA) or `src/ai/` (for ML services)
- Follow pattern in `NasaApiClient`: impl pattern for HTTP details, public interface for queries
- Add configuration struct (e.g., `NasaApiConfig`) for API endpoints and timeouts

## Special Directories

**build/:**
- Purpose: CMake output directory (object files, executables, linked libraries)
- Generated: Yes — created by `cmake . -B build && cmake --build build`
- Committed: No — in `.gitignore`
- Note: Contains subdirectory `_deps/glm-src/doc/api/` with GLM documentation

**external/:**
- Purpose: Vendored or submoduled external dependencies
- Generated: Depends on setup (may be populated by `git submodule update` or FetchContent)
- Committed: Conditionally (submodules are referenced, not copied)

**cmake/**
- Purpose: CMake module files extending build system
- Generated: No
- Committed: Yes
- Note: Imported by `CMakeLists.txt` via `list(APPEND CMAKE_MODULE_PATH ...)`

**.planning/**
- Purpose: GSD (goal-state-driven) planning artifacts
- Generated: Yes — created by `/gsd:map-codebase` and `/gsd:plan-phase` commands
- Committed: Yes — tracks planning history
- Contains: `codebase/` subdirectory with ARCHITECTURE.md, STRUCTURE.md, CONVENTIONS.md, TESTING.md, STACK.md, INTEGRATIONS.md, CONCERNS.md

---

*Structure analysis: 2026-02-28*
