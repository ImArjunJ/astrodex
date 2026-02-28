# Testing Patterns

**Analysis Date:** 2026-02-28

## Test Framework

**Runner:**
- Not detected
- No test framework configured (no jest.config.*, vitest.config.*, or gtest integration)

**Status:**
- No test files found in codebase (`*.test.cpp`, `*.spec.cpp`, `*_test.cpp`)
- No test directory structure (e.g., `tests/`, `test/`, `spec/`)
- No test runner commands in CMakeLists.txt

**Build System:**
- CMake 3.25+ required
- Compilation uses C++23 standard with strict warnings
- AddressSanitizer available via `ASTROCORE_ENABLE_ASAN` CMake option (disabled by default)

## Testing Currently

**Approach:**
- Testing framework: **Not implemented**
- Manual/integration testing likely used during development
- Compiled with `-Wall -Wextra -Wpedantic -Wconversion -Wshadow` for compile-time error detection

**Run Commands:**
```bash
mkdir -p build && cd build
cmake ..
cmake --build .
./astrosplat    # Run application manually
```

**AddressSanitizer (for memory debugging):**
```bash
mkdir -p build && cd build
cmake -DASTROCORE_ENABLE_ASAN=ON ..
cmake --build .
./astrosplat    # Memory issues logged to stderr
```

## Code Coverage

**Requirements:** Not enforced; no coverage tools detected

**Recommendations for Future Implementation:**
- Add gcov/lcov for C++ coverage reporting
- Integrate into CMake with target like `add_custom_target(coverage ...)`

## Test Structure Recommendations

**Suggested Layout (not yet implemented):**
```
/cooking/
├── CMakeLists.txt
├── src/                    # Application code
│   ├── core/
│   ├── render/
│   └── ...
├── tests/                  # New: test directory
│   ├── CMakeLists.txt      # Test target configuration
│   ├── core/               # Core module tests
│   │   └── test_logger.cpp
│   │   └── test_window.cpp
│   ├── render/             # Render module tests
│   │   └── test_shader_program.cpp
│   │   └── test_camera.cpp
│   └── fixtures/           # Shared test utilities
│       └── test_helpers.hpp
```

## Recommended Testing Framework for C++

**Google Test (gtest):**
Suggested framework for this C++ project:
- Industry standard for C++ testing
- Header-only and easy to integrate via CMakeLists.txt
- Supports fixtures, mocking (via googlemock), assertions

**Example Setup (future):**

`tests/CMakeLists.txt`:
```cmake
find_package(GTest REQUIRED)

add_executable(astrocore_tests
    core/test_logger.cpp
    render/test_shader_program.cpp
)

target_link_libraries(astrocore_tests
    PRIVATE
        astrocore_lib
        GTest::gtest
        GTest::gtest_main
)

add_test(NAME AstroCoreTests COMMAND astrocore_tests)
```

## Testing Patterns (Future Implementation)

**Suggested Unit Test Structure:**

```cpp
// tests/core/test_logger.cpp
#include <gtest/gtest.h>
#include "core/Logger.hpp"

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        astrocore::Logger::init();
    }
};

TEST_F(LoggerTest, CoreLoggerInitialized) {
    auto logger = astrocore::Logger::getCoreLogger();
    EXPECT_NE(logger, nullptr);
}

TEST_F(LoggerTest, LogMessageSucceeds) {
    EXPECT_NO_THROW(LOG_INFO("Test message"));
}
```

**Suggested Component Test (Shader):**

```cpp
// tests/render/test_shader_program.cpp
#include <gtest/gtest.h>
#include "render/ShaderProgram.hpp"

class ShaderProgramTest : public ::testing::Test {
protected:
    astrocore::ShaderProgram shader;
};

TEST_F(ShaderProgramTest, InvalidShaderFileReturnssFalse) {
    bool result = shader.loadFromFiles("invalid/path.vert", "invalid/path.frag");
    EXPECT_FALSE(result);
}

TEST_F(ShaderProgramTest, ValidShaderIsValid) {
    bool result = shader.loadFromFiles("shaders/planet.vert", "shaders/planet.frag");
    if (result) {
        EXPECT_TRUE(shader.isValid());
    }
}
```

## Error Cases to Test

**Based on current code patterns:**

1. **Resource Initialization Failures:**
   - GLFW initialization failure (currently throws in `Window::Window()`)
   - Window creation failure
   - GLAD loading failure
   - Shader compilation failure

2. **File I/O:**
   - Missing shader files (returns false in `ShaderProgram::readFile()`)
   - Invalid file paths

3. **JSON Parsing:**
   - Invalid JSON from external APIs (caught in try-catch, `src/ai/BedrockClient.cpp`)
   - Missing expected fields in JSON response

4. **Parameter Validation:**
   - Camera zoom limits (`m_minDistance`, `m_maxDistance`)
   - Pitch angle limits (`m_minPitch`, `m_maxPitch`)

## Mocking Strategy (Future)

**Framework:** Google Mock (googlemock)

**Things to Mock:**
- External API calls (BedrockClient, NasaApiClient)
- File I/O operations
- GLFW callbacks (window size, key press, mouse events)

**Things NOT to Mock:**
- Core rendering logic (test with actual OpenGL if possible, or render-to-texture)
- Math utilities (GLM matrix operations)
- Logging (use real logger in tests for debugging)

**Example Mock (future):**

```cpp
// tests/fixtures/mock_window.hpp
#include <gmock/gmock.h>
#include "core/Window.hpp"

class MockWindow : public astrocore::Window {
public:
    MOCK_METHOD(bool, shouldClose, (), (const, override));
    MOCK_METHOD(void, pollEvents, (), (override));
    MOCK_METHOD(void, swapBuffers, (), (override));
};
```

## Testable Code Patterns (Current Observations)

**Already Testable:**
- Math components (`Camera`, `ShaderProgram`)
- Parameter management (`PlanetParams`)
- Utility functions (Logger, file operations)

**Needs Refactoring for Testability:**
- `Application` tightly couples window/renderer/UI initialization
- External API clients hardcode credentials/config; suggest dependency injection
- No interfaces/abstract base classes for mocking

**Refactoring Suggestion:**
```cpp
// Current (hard to test)
Application::Application() {
    m_window = std::make_unique<Window>(config);
}

// Better (testable)
class Application {
    Application(std::unique_ptr<Window> window, std::unique_ptr<Renderer> renderer)
        : m_window(std::move(window)), m_renderer(std::move(renderer)) {}
};
```

## Current Quality Measures

**Compiler Warnings:**
- Strict compilation flags enabled: `-Wall -Wextra -Wpedantic -Wconversion -Wshadow`
- `-Wno-unused-parameter` disabled (for callback handlers with unused params)

**Runtime Safety:**
- AddressSanitizer support available via CMake option
- Smart pointers (`unique_ptr`, `shared_ptr`) prevent memory leaks

**No Issues Detected:**
- No TODO/FIXME markers in source files
- No stub returns or unimplemented functions

---

*Testing analysis: 2026-02-28*

## Recommendations for Testing Implementation

1. **Add Google Test framework** to CMakeLists.txt
2. **Start with unit tests** for math components (Camera, ShaderProgram)
3. **Mock external dependencies** (NASA API, Bedrock client)
4. **Set up CI pipeline** to run tests on commit
5. **Aim for 70%+ coverage** of core logic, skip rendering layer if needed
