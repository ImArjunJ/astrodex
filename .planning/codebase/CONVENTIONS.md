# Coding Conventions

**Analysis Date:** 2026-02-28

## Naming Patterns

**Files:**
- Implementation and header files use `.cpp` and `.hpp` extensions
- Names follow PascalCase with semantic suffixes: `Application.cpp`, `Logger.hpp`, `ShaderProgram.hpp`
- Organized in domain-based directories: `src/core/`, `src/render/`, `src/ai/`, `src/data/`, `src/ui/`

**Functions:**
- Member functions use camelCase: `init()`, `render()`, `getViewMatrix()`, `setPosition()`
- Static functions use camelCase: `Logger::init()`, `Window::pollEvents()`
- Private helper functions use camelCase with leading verb: `createQuad()`, `generateNoiseTexture()`, `readFile()`

**Variables:**
- Member variables prefixed with `m_` and use camelCase: `m_window`, `m_renderer`, `m_lastFrameTime`, `m_viewDirty`
- Static variables prefixed with `s_`: `Logger::s_coreLogger`
- Local variables use camelCase: `deltaTime`, `currentTime`, `width`, `height`
- Constants use UPPERCASE: `GL_LINEAR`, `GLFW_MOUSE_BUTTON_LEFT`
- Parameter variables use camelCase: `config`, `deltaTime`, `camera`

**Types:**
- Classes and structs use PascalCase: `Application`, `Renderer`, `Camera`, `ShaderProgram`, `Window`, `Logger`
- Struct config types use `Config` suffix: `WindowConfig`, `BedrockConfig`
- Struct data containers use singular names: `PlanetParams` (single struct containing all planet parameters)

**Namespace:**
- All code lives in `astrocore` namespace
- No nested namespaces used
- Namespace closing comment: `}  // namespace astrocore`

## Code Style

**Formatting:**
- No clang-format config detected; follows CMake standard C++23 conventions
- Indentation: 4 spaces (standard C++ practice)
- Brace style: Opening brace on same line for functions, new line for classes
- Line length: No strict limit observed, but generally concise

**Linting:**
- Enforced by CMake compiler options in `cmake/CompilerOptions.cmake`
- Enabled warnings: `-Wall -Wextra -Wpedantic -Wconversion -Wshadow`
- Disabled warning: `-Wno-unused-parameter` (common in callback handlers)
- Supports AddressSanitizer (`-fsanitize=address`) via CMake option `ASTROCORE_ENABLE_ASAN` (OFF by default)

**C++ Standard:**
- C++23 standard: `set(CMAKE_CXX_STANDARD 23)`
- Extensions disabled: `set(CMAKE_CXX_EXTENSIONS OFF)`
- Enforced: `set(CMAKE_CXX_STANDARD_REQUIRED ON)`

## Import Organization

**Order (observed pattern):**
1. Project headers using relative quotes: `#include "core/Application.hpp"`
2. External library headers with angle brackets: `#include <glad/gl.h>`, `#include <glm/glm.hpp>`, `#include <spdlog/spdlog.h>`
3. Standard library headers: `#include <memory>`, `#include <string>`, `#include <vector>`, `#include <exception>`

**Header Protection:**
- All header files use `#pragma once` (19 headers confirmed)
- No include guards (`#ifndef` style) found

**Path Aliases:**
- Relative includes from `src/` root: `#include "core/Logger.hpp"` (not `#include "../core/Logger.hpp"`)
- Configured in CMakeLists.txt: `target_include_directories(astrocore_lib PUBLIC ${CMAKE_SOURCE_DIR}/src)`

**Example (from `src/core/Application.cpp`):**
```cpp
#include "core/Application.hpp"
#include "core/Logger.hpp"
#include "render/Renderer.hpp"
#include "render/Camera.hpp"
#include <imgui.h>
```

## Error Handling

**Patterns:**
- Constructor errors throw `std::runtime_error` or `std::exception`:
  - `src/core/Window.cpp`: Throws on GLFW init, window creation, GLAD init failures
  - `src/main.cpp`: Catches `std::exception` in try-catch at top level
- File operations return false on error, log via `LOG_ERROR()`: `src/render/ShaderProgram.cpp` readFile pattern
- JSON parsing wraps in try-catch for `nlohmann::json::exception`: `src/ai/BedrockClient.cpp`, `src/data/NasaApiClient.cpp`
- Optional error recovery: Some failures logged but execution continues (e.g., shader load failure in `src/render/Renderer.cpp`)

**Pattern Example (from `src/core/Window.cpp`):**
```cpp
if (!glfwInit()) {
    throw std::runtime_error("Failed to initialize GLFW");
}
```

**Pattern Example (from `src/render/ShaderProgram.cpp`):**
```cpp
bool ShaderProgram::loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath) {
    std::string vertexSource = readFile(vertexPath);
    if (vertexSource.empty() || fragmentSource.empty()) {
        return false;
    }
    return loadFromSource(vertexSource, fragmentSource);
}
```

## Logging

**Framework:** spdlog (configured in `src/core/Logger.hpp` and `src/core/Logger.cpp`)

**Initialization:**
- Two sinks: stdout with colors, file sink (`astrocore.log`)
- Pattern for console: `"%^[%T] [%l] %n: %v%$"` (time, level, logger name, message with color)
- Pattern for file: `"[%Y-%m-%d %T.%e] [%l] %n: %v"` (timestamp, level, logger name, message)
- Global logger registered as "ASTRO"
- Trace level enabled: `s_coreLogger->set_level(spdlog::level::trace)`

**Usage (via convenience macros in `src/core/Logger.hpp`):**
- `LOG_TRACE(...)`: Lowest level detail
- `LOG_DEBUG(...)`: Debug information
- `LOG_INFO(...)`: General information (used in `Application::init()`)
- `LOG_WARN(...)`: Warning level
- `LOG_ERROR(...)`: Error level (used in shader/file failures)
- `LOG_CRITICAL(...)`: Critical errors

**Pattern Example:**
```cpp
LOG_INFO("OpenGL {}.{} loaded", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));
LOG_INFO("Window created: {}x{}", m_width, m_height);
LOG_ERROR("Failed to open shader file: {}", path);
```

## Comments

**When to Comment:**
- Inline comments explain complex logic or algorithms
- Comments appear frequently for shader parameters and rendering concepts
- Comments describe intent, not the obvious code

**Example (from `src/render/PlanetParams.hpp`):**
```cpp
float fbmPersistence = 0.5f;    // low = smooth, high = rough/noisy
float fbmLacunarity = 2.0f;     // frequency multiplier per octave
float fbmExponentiation = 5.0f; // low = flat plateaus, high = sharp peaks
```

**JSDoc/TSDoc:**
- No formal documentation comment style detected
- C++ doesn't have an enforced documentation standard in this codebase
- Comments are plain C++ style (`//` and `/* */`)

## Function Design

**Size:**
- Functions are generally 10-50 lines
- Core business logic separated into focused functions: `init()`, `render()`, `update()`, `shutdown()`

**Parameters:**
- Use const references for complex objects: `const Camera& camera`, `const glm::vec3& position`
- Use const references for structs: `const WindowConfig& config`
- Primitive types passed by value: `float deltaTime`, `int width`
- Out parameters via reference rare; getter methods preferred

**Return Values:**
- Boolean for success/failure: `loadFromFiles()`, `loadFromSource()`, `isValid()`
- Void for operations with side effects: `init()`, `render()`, `update()`
- getters return by value or const reference: `getViewMatrix() const` returns matrix, `getCoreLogger() const` returns shared_ptr reference

**Pattern (from `src/render/Renderer.hpp`):**
```cpp
// Success/failure
bool loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath);
bool loadFromSource(const std::string& vertexSource, const std::string& fragmentSource);

// State modification
void render(const Camera& camera);

// Query
bool isValid() const { return m_program != 0; }
glm::mat4 getViewMatrix() const;
```

## Module Design

**Exports:**
- Headers declare public interface in `.hpp` files
- Implementation in `.cpp` files
- No inline implementations in headers except trivial getters
- Classes use public/private sections clearly

**Barrel Files:**
- Not used; each module imports only what it needs directly

**Include Strategy:**
- Forward declarations used for pointers/references when possible
- Example (from `src/core/Application.hpp`):
```cpp
class Renderer;  // Forward declaration
class Camera;    // Forward declaration
class Application {
    std::unique_ptr<Renderer> m_renderer;  // Full definition in .cpp
    std::unique_ptr<Camera> m_camera;
};
```

## Move Semantics and Resource Management

**Smart Pointers:**
- `std::unique_ptr` for exclusive ownership: `m_window`, `m_renderer`, `m_camera`, `m_ui` in `Application`
- `std::shared_ptr` for shared ownership: `Logger::s_coreLogger`

**Move Operations:**
- Move constructors and assignment operators defined for resource types
- Copy constructors/assignment explicitly deleted: `= delete`

**Pattern (from `src/core/Application.hpp`):**
```cpp
Application(const Application&) = delete;
Application& operator=(const Application&) = delete;
```

**Pattern (from `src/render/ShaderProgram.hpp`):**
```cpp
ShaderProgram(ShaderProgram&& other) noexcept;
ShaderProgram& operator=(ShaderProgram&& other) noexcept;
```

---

*Convention analysis: 2026-02-28*
