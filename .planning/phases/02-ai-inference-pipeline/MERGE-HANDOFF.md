# Merge Handoff — starmap + intro branches

## What was done
- Merged `origin/intro` (particle constellation intro animation) into master
- Merged `origin/starmap` (Vulkan renderer, star explorer, cubemap) into master
- Transitioned from OpenGL/Metal multi-backend to **Vulkan-only**
- Removed: MetalRenderer, Renderer (OpenGL), ShaderProgram, GLAD
- Added: VulkanRenderer, vk-bootstrap, VMA, ShaderCompilation.cmake
- Kept: All data layer, AI inference pipeline, tests, pugixml, Catch2, curl

## Merge commit
`c0678a5` on master

## Conflict resolution summary
| File | Resolution |
|------|-----------|
| .gitignore | Merged both (ML experiments + starmap data) |
| CMakeLists.txt | Starmap's Vulkan structure + master's data/AI/tests/curl/intro |
| Dependencies.cmake | Vulkan deps + pugixml + Catch2, GLFW 3.3.9 |
| IRenderer.hpp | Kept black hole fields, removed MetalFrameContext |
| Application.cpp | VulkanRenderer init + ML pipeline + intro + exoplanet loading |
| Application.hpp | Kept ML pipeline members |
| UIManager.cpp/hpp | Vulkan init/shutdown + exoplanet callback/status |
| Window.cpp/hpp | Took starmap's Vulkan version (GLFW_NO_API) |
| MetalRenderer.*, Renderer.*, ShaderProgram.* | Deleted (Vulkan-only) |

## NOT verified in container
- Container lacks X11/Vulkan so cmake configure fails — **full build must be tested on local machine**
- `rm -rf build && cmake -B build && cmake --build build`
- Tests should still compile if cmake can find Vulkan (tests don't use Vulkan directly)

## Remaining: Phase 2 verification
User wanted to verify phase 2 was successfully implemented before moving to phase 3.
Phase 2 = AI inference pipeline. All data tests (409 assertions/46 cases) and render tests (114/15) passed before the Vulkan transition. The test code itself was not changed by the merges.

## Compiler warnings from user's build (not blockers)
- ImGui `-Wconversion` warnings (upstream, not our code)
- GaiaClient.cpp: sign-compare, unused variable `idx`
- OecClient.cpp: size_t to int conversion in urlEncode
