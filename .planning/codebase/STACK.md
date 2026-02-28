# Technology Stack

**Analysis Date:** 2026-02-28

## Languages

**Primary:**
- C++ 23 - Core application and rendering engine
- GLSL - Fragment/vertex shaders for GPU rendering (in `shaders/`)

**Secondary:**
- CMake - Build configuration and dependency management
- Python - AWS CLI integration (for AI/bedrock invocation)

## Runtime

**Environment:**
- C++ runtime (libstdc++ or libc++)
- OpenGL 4.5 Core profile

**Build System:**
- CMake 3.25+
- Compiler support: GCC, Clang, MSVC
- C++ Standard: C++23 with extensions disabled

## Frameworks

**Core Graphics:**
- OpenGL 4.5 Core - Graphics API
- GLAD 2.0.6 - OpenGL loader (in `cmake/Dependencies.cmake`)
- GLFW 3.4 - Window and input management
- GLM 1.0.1 - OpenGL Mathematics library

**UI:**
- Dear ImGui v1.91.6-docking - Immediate mode GUI with GLFW+OpenGL3 backends
  - Backend: `imgui_impl_glfw.cpp`, `imgui_impl_opengl3.cpp`
  - Created as static library `imgui_impl` in CMakeLists

**Logging:**
- spdlog v1.14.1 - Fast C++ logging library
- Macros: `LOG_TRACE`, `LOG_DEBUG`, `LOG_INFO`, `LOG_WARN`, `LOG_ERROR`, `LOG_CRITICAL`

## Key Dependencies

**Critical:**
- nlohmann/json v3.11.3 - JSON parsing and serialization
  - Used by: NASA API client (`src/data/NasaApiClient.hpp`), Bedrock client (`src/ai/BedrockClient.hpp`), ExoplanetData serialization
- libcurl - HTTP client library (implicit dependency via NasaApiClient and BedrockClient)
  - NASA TAP endpoint queries
  - AWS Bedrock inference requests

**Infrastructure:**
- OpenGL::GL - System OpenGL library
- spdlog::spdlog - Logging infrastructure
- glm::glm - Mathematics utilities
- glad_gl45_core - OpenGL 4.5 loader

## Configuration

**Build Flags:**
- `CMAKE_CXX_STANDARD 23`
- `CMAKE_CXX_STANDARD_REQUIRED ON`
- `CMAKE_EXPORT_COMPILE_COMMANDS ON` - Generate compile_commands.json

**Compiler Options:**
- GCC/Clang: `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wno-unused-parameter`
- Debug: `-g -O0` with optional AddressSanitizer (`-fsanitize=address`)
- Release: `-O3 -DNDEBUG`
- MSVC: `/W4 /permissive- /Zc:__cplusplus`

**Dependencies Resolution:**
- FetchContent (CMake built-in) - fetches from GitHub git repositories
- All dependencies shallow-cloned for speed (`GIT_SHALLOW TRUE`)
- Optional dependency: pybind11 (commented out in `cmake/Dependencies.cmake`, line 89)
- Optional dependency: Catch2 (commented out for unit testing, line 96)

**Environment Variables:**
- `AWS_ACCESS_KEY_ID` - AWS credentials for Bedrock (BedrockClient checks via AWS CLI)
- `AWS_SECRET_ACCESS_KEY` - AWS credentials for Bedrock
- `AWS_REGION` - Optional, defaults to `us-east-1` in BedrockConfig

**Asset Location:**
- Shaders: `shaders/` → copied to build directory post-build
- Assets: `assets/` → copied to build directory post-build

## Platform Requirements

**Development:**
- C++ compiler: GCC, Clang, or MSVC
- CMake 3.25 or higher
- GLFW dependencies (X11 on Linux, Cocoa on macOS, Win32 on Windows)
- OpenGL development headers
- AWS CLI installed for Bedrock integration (checked at runtime in `src/ai/BedrockClient.cpp`)

**Production:**
- OpenGL 4.5 Core capable GPU
- libcurl for network requests
- spdlog runtime
- AWS CLI for Bedrock inference (optional, falls back to disabled AI)

---

*Stack analysis: 2026-02-28*
