# Metal Backend — Change Log

## Branch: `Tej`

### What changed and why

---

### `CMakeLists.txt`

- Removed `OBJCXX` from the top-level `project()` declaration so the build stays valid on Linux/Windows where Objective-C++ is not available.
- Added `USE_METAL` CMake flag (default `OFF`). On Apple Silicon (arm64 macOS) it is automatically forced `ON` via `execute_process(COMMAND uname -m)`. Can be overridden with `-DUSE_METAL=OFF`.
- When `USE_METAL` is active:
  - `enable_language(OBJC)` is called **before** `enable_language(OBJCXX)`. Without `OBJC` being declared first, CMake had no registered handler for `.m` files and was falling back to the C++ compiler, which applied C++ strict type rules to GLFW's plain Objective-C sources and broke the build.
  - `src/render/MetalRenderer.mm` is added to the source list instead of `Renderer.cpp` / `ShaderProgram.cpp`.
  - Apple frameworks `Metal`, `MetalKit`, `Foundation`, `QuartzCore`, and `AppKit` are linked.
  - `ASTRO_METAL` preprocessor definition is set project-wide.
  - `src/ui/UIManager.cpp` is explicitly compiled as `OBJCXX` and both it and `MetalRenderer.mm` get `-fobjc-arc`.

---

### `cmake/Dependencies.cmake`

- **GLFW downgraded from 3.4 → 3.3.9.** GLFW 3.4 introduced a `UCKeyTranslate` call that passes `const void *` where the Xcode 16 / macOS SDK 15 `CarbonCore` header now requires `const UCKeyboardLayout *`. This caused a hard compile error on any Mac with a recent Xcode install.
- On macOS, `find_package(glfw3)` is attempted first (checks the Homebrew prefix `/opt/homebrew`). If GLFW is already installed via `brew install glfw` the source is never compiled, making the build faster and more robust.
- A `glfw ALIAS glfw3::glfw` target is created when the Homebrew package is found, so all downstream `target_link_libraries(... glfw)` calls work regardless of how GLFW was obtained.
- `glad` is only fetched when `USE_METAL` is `OFF` — it is a Python-generated OpenGL loader and requires `jinja2`; the Metal path has no use for it.
- `imgui_impl_metal.mm` is included instead of `imgui_impl_opengl3.cpp` when `USE_METAL` is active. The Metal ImGui backend is linked against `Metal` and `QuartzCore`.

---

### New files

| File | Purpose |
|------|---------|
| `src/render/IRenderer.hpp` | Pure-virtual `IRenderer` interface. Allows `Application` to hold a `std::unique_ptr<IRenderer>` and swap backends at compile time. Also defines `MetalFrameContext` for passing Metal command buffer / encoder / render pass between subsystems. |
| `src/render/MetalRenderer.hpp` | Header for the Metal backend using the Pimpl idiom to keep all Objective-C types out of headers included by plain C++ translation units. |
| `src/render/MetalRenderer.mm` | Full Objective-C++ Metal backend: device + command queue creation, `CAMetalLayer` attachment to the GLFW `NSWindow`, runtime MSL shader compilation, render pipeline state, fullscreen-quad vertex buffer, 3D noise texture upload, and per-frame command buffer / drawable management. |
| `shaders/planet.metal` | MSL translation of the original `planet.frag` GLSL shader. Contains all procedural planet logic: FBM / ridged noise, raymarching, craters, volumetric clouds, atmosphere scattering, and star field. Resource bindings use `[[buffer(n)]]` / `[[texture(n)]]` / `[[sampler(n)]]` instead of GLSL uniforms. |

---

### Modified files

| File | Change summary |
|------|---------------|
| `src/render/Renderer.hpp` | Inherits `IRenderer`; `PlanetParams` moved to `IRenderer.hpp`; wrapped in `#ifndef ASTRO_METAL`. |
| `src/render/Renderer.cpp` | `init()` signature updated to match `IRenderer::init(int, int, void*)`. |
| `src/core/Window.hpp` | `#include <glad/gl.h>` guarded by `#ifndef ASTRO_METAL`. |
| `src/core/Window.cpp` | GLFW window hints (`GLFW_CLIENT_API`, `GLFW_NO_API`), GLAD init, `glViewport`, and `glEnable` calls all guarded by `#ifndef ASTRO_METAL`. `swapBuffers()` is a no-op on the Metal path (presentation is handled by `MetalRenderer`). |
| `src/core/Application.hpp` | `m_renderer` changed from `std::unique_ptr<Renderer>` to `std::unique_ptr<IRenderer>`. |
| `src/core/Application.cpp` | Conditionally instantiates `MetalRenderer` or `Renderer`. Passes `glfwWindow` handle to `renderer->init`. Calls overloaded Metal-specific `UIManager::beginFrame` / `endFrame` with the frame context. |
| `src/ui/UIManager.hpp` | Overloaded `init`, `beginFrame`, and `endFrame` added for the Metal path (accept Metal device pointer and `MetalFrameContext`). |
| `src/ui/UIManager.cpp` | Conditional includes for `imgui_impl_metal.h` vs `imgui_impl_opengl3.h`. Separate init / frame / shutdown code paths for each backend using `__bridge` casts for Objective-C type interop. |

---

### Build instructions (macOS arm64)

```bash
# Optional but recommended — skips compiling GLFW from source
brew install glfw

mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(sysctl -n hw.logicalcpu)
./astrodex
```

CMake will print `Apple Silicon (arm64) detected — Metal backend ENABLED` to confirm the right path is active.

### Build instructions (Linux / Intel Mac / Windows)

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)   # Linux; use -j%NUMBER_OF_PROCESSORS% on Windows
./astrodex
```

The OpenGL path is used automatically. No Metal files are compiled.
