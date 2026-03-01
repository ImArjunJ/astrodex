---
phase: 04-exoplanet-catalogue-browser
plan: 02
subsystem: render
tags: [vulkan, offscreen-fbo, thumbnail, stb-image-write, imgui-texture, vma, png]

# Dependency graph
requires:
  - phase: 03-search-render-integration
    provides: VulkanRenderer with Vulkan 1.2/VMA pipeline, ImGui integration
provides:
  - ThumbnailRenderer class with reusable offscreen Vulkan FBO
  - GPU-to-CPU pixel readback via VMA staging buffer
  - PNG serialization via stb_image_write
  - ImGui texture registration for catalogue card display
affects: [04-03-progressive-thumbnail-generation, catalogue-hover-animation]

# Tech tracking
tech-stack:
  added: [stb_image_write.h]
  patterns: [offscreen-fbo-render, image-layout-transitions, gpu-cpu-readback, imgui-texture-registration]

key-files:
  created:
    - src/render/ThumbnailRenderer.hpp
    - src/render/ThumbnailRenderer.cpp
    - external/stb_image_write.h
    - tests/render/test_thumbnail_renderer.cpp
  modified:
    - CMakeLists.txt
    - tests/CMakeLists.txt

key-decisions:
  - "Single reusable 128px FBO instead of per-thumbnail allocation to save VRAM"
  - "Placeholder colored clear instead of full planet pipeline for thumbnails (deferred to optimization pass)"
  - "reinterpret_cast for VkDescriptorSet to ImTextureID conversion on non-pointer ImTextureID platforms"
  - "No Y-flip for PNG: both Vulkan and PNG use top-left origin with standard viewport"

patterns-established:
  - "Offscreen FBO pattern: create image -> view -> render pass -> framebuffer -> sampler -> ImGui register"
  - "GPU->CPU readback: staging buffer with VMA_MEMORY_USAGE_GPU_TO_CPU + MAPPED flag"
  - "Image layout transition helper via VkImageMemoryBarrier + vkCmdPipelineBarrier"
  - "One-shot command buffer pattern: allocate -> begin -> record -> end -> submit -> wait -> free"

requirements-completed: [R4.3]

# Metrics
duration: 5min
completed: 2026-03-01
---

# Phase 4 Plan 2: Offscreen Thumbnail Renderer Summary

**Vulkan offscreen FBO with reusable 128px framebuffer, GPU-to-CPU readback via staging buffer, PNG serialization via stb_image_write, and ImGui texture registration**

## Performance

- **Duration:** 5 min
- **Started:** 2026-03-01T02:53:53Z
- **Completed:** 2026-03-01T02:59:31Z
- **Tasks:** 2
- **Files modified:** 6

## Accomplishments
- ThumbnailRenderer class with complete offscreen Vulkan FBO lifecycle (573 lines)
- GPU-to-CPU pixel readback via VMA staging buffer with persistent mapping
- PNG file output via stb_image_write with automatic directory creation
- ImGui texture registration for catalogue card display
- 15 new test assertions (129 total in render_tests suite)

## Task Commits

Each task was committed atomically:

1. **Task 1: Create ThumbnailRenderer class with offscreen FBO infrastructure** - `a6e217a` (feat)
2. **Task 2: Add unit test for thumbnail rendering and PNG serialization** - `3448914` (test)

**Plan metadata:** (pending) (docs: complete plan)

## Files Created/Modified
- `src/render/ThumbnailRenderer.hpp` - ThumbnailRenderer class declaration with offscreen FBO management
- `src/render/ThumbnailRenderer.cpp` - Full Vulkan offscreen rendering: FBO creation, render-to-texture, pixel readback, PNG serialization, ImGui texture registration (573 lines)
- `external/stb_image_write.h` - Single-header PNG encoding library (1724 lines)
- `tests/render/test_thumbnail_renderer.cpp` - Unit tests for interface, color extraction, camera config, PNG paths, GPU smoke test
- `CMakeLists.txt` - Added ThumbnailRenderer.cpp to astrocore_lib sources
- `tests/CMakeLists.txt` - Added test_thumbnail_renderer.cpp to render_tests target

## Decisions Made
- **Single reusable FBO:** One 128px offscreen framebuffer reused serially for all thumbnails. Saves VRAM (each FBO is ~64KB vs 32MB for 500 individual FBOs).
- **Placeholder rendering for now:** Planet-type-colored clear instead of full procedural planet pipeline. Full rendering integration deferred to future optimization pass. Acceptable per user decision "show placeholders initially".
- **ImTextureID casting:** Used `reinterpret_cast<ImTextureID>(VkDescriptorSet)` since ImTextureID is `unsigned long long` on this platform (not `void*`). Clean separation via `m_imguiDescSet` for Vulkan-side and `m_imguiTexture` for ImGui-side.
- **No Y-flip for PNG:** Both Vulkan framebuffer and PNG use top-left origin with standard viewport, so no row flip needed during readback.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed ImTextureID type mismatch on Linux**
- **Found during:** Task 1 (ThumbnailRenderer build)
- **Issue:** `ImTextureID` is `unsigned long long` on this platform, not `void*`. Direct assignment from `VkDescriptorSet` (pointer) caused compilation error. `nullptr` also invalid for integer type.
- **Fix:** Added separate `m_imguiDescSet` (VkDescriptorSet) member for Vulkan operations, use `reinterpret_cast` for ImTextureID conversion, initialize with `0` instead of `nullptr`.
- **Files modified:** src/render/ThumbnailRenderer.hpp, src/render/ThumbnailRenderer.cpp
- **Verification:** Build succeeds, ImGui texture registration functional
- **Committed in:** a6e217a (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (1 bug)
**Impact on plan:** Platform-specific type fix necessary for compilation. No scope creep.

## Issues Encountered
- stb_image_write.h produces several `-Wconversion` and `-Wmissing-field-initializers` warnings. These are from the third-party header and do not affect correctness. Could be suppressed with pragma push/pop if desired.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- ThumbnailRenderer is ready for integration into CatalogueView (Plan 03)
- Progressive thumbnail generation can call renderThumbnail() per-frame for visible cards
- saveToPNG() enables disk caching in `.cache/thumbnails/` directory
- ImGui texture IDs can be passed directly to ImGui::Image() in catalogue cards
- Full procedural planet rendering in thumbnails is a future optimization (not blocking)

---
*Phase: 04-exoplanet-catalogue-browser*
*Completed: 2026-03-01*
