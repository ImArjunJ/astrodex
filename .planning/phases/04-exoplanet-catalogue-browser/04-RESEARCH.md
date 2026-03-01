# Phase 4: Exoplanet Catalogue Browser - Research

**Researched:** 2026-03-01
**Domain:** Vulkan offscreen rendering, ImGui texture integration, card-based UI, thumbnail caching
**Confidence:** HIGH

## Summary

Phase 4 builds a browseable Pokedex-style catalogue of 500+ exoplanets with mini-render previews. The project already has all foundational infrastructure: `DataFusionEngine::prefetchNotable(500)` for bulk data fetching, `CacheManager` for persistent JSON storage, `VulkanRenderer` with full Vulkan 1.2 + VMA rendering pipeline, `ImGui 1.91.6-docking` for UI, and proven scrollable list patterns from `GalaxyView`. The core technical challenge is implementing offscreen framebuffer rendering for thumbnails and wiring those rendered textures into ImGui cards via `ImGui_ImplVulkan_AddTexture()`.

The architecture follows a lazy deferred rendering model: show placeholder cards immediately, render thumbnails progressively as they scroll into view, cache rendered PNGs to disk for instant reload across sessions. User decision: card grid layout (not list), catalogue replaces Galaxy view as landing screen, hover-to-animate (only one planet rendering at a time), click-to-load with existing fade transition. Technical constraints: single offscreen FBO reused for all thumbnails (memory efficient), 128-256px resolution (user discretion), stb_image_write for PNG serialization.

**Primary recommendation:** Build in layers: (1) catalogue UI with placeholder cards and filtering/sorting, (2) offscreen FBO rendering infrastructure, (3) progressive thumbnail generation, (4) disk caching, (5) hover animation. Each layer can be tested independently.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **Catalogue Layout:** Card grid layout (Pokedex-style) — visual and scannable. Catalogue replaces Galaxy view as browse/landing screen. Click card → loads planet into main renderer with existing fade transition. Back button returns to catalogue. Claude's discretion on card info density (name, type, discovery year, key stat) based on grid space.
- **Mini-Render Previews:** Deferred rendering — show colored placeholders initially, render real previews lazily as user scrolls into view. Resolution: Claude's discretion (128-256px based on layout needs and VRAM budget). Static snapshots by default, animate (slowly rotate) on hover — only 1 planet rendering at a time. Disk cache thumbnails as PNGs in `.cache/thumbnails/` for instant reload across sessions.
- **Sorting & Filtering:** Default sort: discovery date (newest first). Claude's discretion on sort direction toggle and additional sort options. Filters: planet type + habitable zone toggle at minimum. Include search bar with autocomplete within catalogue (reuse existing pattern). Claude's discretion on filter UI complexity.
- **Data Prefetch Strategy:** Prefetch 500+ records at app startup in background using existing `prefetchNotable(500)` API. Show subtle count in catalogue header during load ("Loading... 142/500 planets"). Fully offline-capable: cache all fetched records, show cached data immediately on subsequent launches while refreshing in background. AI inference: on-demand only (when planet selected for full rendering). Catalogue shows raw data with 'data incomplete' indicator for missing fields. Use physics-based defaults / SolarSystemDatabase analog matching for catalogue thumbnail rendering.

### Claude's Discretion
- Exact card dimensions and spacing
- Loading skeleton / placeholder design
- Error state handling (failed fetches, missing data)
- Thumbnail VRAM budget management
- Sort options beyond discovery date (mass, name, etc.)
- Filter UI layout (dropdown vs sidebar vs chips)

### Deferred Ideas (OUT OF SCOPE)
None — discussion stayed within phase scope.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| R4.1 | Local Exoplanet Database: Pre-fetch and cache top 500 exoplanets, store fused records locally, background refresh on app start | Existing `DataFusionEngine::prefetchNotable(500)` async API returns `std::future<std::vector<ExoplanetData>>`, `CacheManager` JSON persistence with TTL, `Application::init()` startup hook |
| R4.2 | Catalogue UI: Scrollable list panel, default sort by discovery date, columns for key data, click to render | ImGui card grid via `ImGui::BeginChild` scrollable region + manual layout (tiles), `ImGui::Selectable` or `ImGui::InvisibleButton` for click detection, existing `GalaxyView::renderUI()` patterns for search/filter sidebar |
| R4.3 | Mini Render Previews: Small thumbnail renders via low-res offscreen FBO, cached as textures, progressive rendering | Vulkan offscreen FBO (render pass + framebuffer + color attachment), `vkCmdCopyImageToBuffer` to read pixels, `stb_image_write` for PNG serialization, `ImGui_ImplVulkan_AddTexture()` for ImGui display, existing `VulkanRenderer::Impl` patterns for image/view/sampler creation |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Vulkan | 1.2+ | GPU rendering API | Already core renderer backend; VMA 3.1.0 for memory management |
| vk-bootstrap | 1.3.290 | Vulkan initialization | Already integrated; simplifies instance/device/swapchain setup |
| Dear ImGui | 1.91.6-docking | Immediate-mode UI | Already integrated with `imgui_impl_vulkan`; all UI built with it |
| VulkanMemoryAllocator (VMA) | 3.1.0 | Vulkan memory management | Already integrated; handles all VkImage/VkBuffer allocations |
| stb_image_write | (header-only) | PNG encoding | Already in project (`external/stb_image.h` present); lightweight single-header library |
| nlohmann/json | (bundled) | JSON serialization | Already used for ExoplanetData caching; catalogue metadata persistence |
| glm | 1.0.1 | Math library | Already used for vectors/matrices; layout calculations |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| std::async/std::future | C++23 | Async prefetch and progressive thumbnail rendering | Background work without blocking main thread |
| std::filesystem | C++17 | Cache directory management | Already used in `CacheManager`; thumbnail file I/O |
| std::atomic | C++23 | Thread-safe prefetch progress counter | Async worker → main thread status updates |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Single reusable FBO | Per-thumbnail FBO pool | Reusable FBO saves VRAM (1-2MB each); progressive rendering is serial anyway |
| PNG caching | In-memory texture atlas | PNG files survive app restarts; atlas requires warm-up rebuild |
| ImGui card grid | ImGui::Table | Table is simpler but less flexible for custom card layouts with images |
| Offscreen render | Render to swapchain + readback | Offscreen FBO allows rendering without flickering main viewport |

**Installation:**
```bash
# All dependencies already in cmake/Dependencies.cmake
# stb_image_write.h needs to be added to external/ (or use existing stb_image.h which includes write)
```

## Architecture Patterns

### Recommended Project Structure
```
src/
├── ui/
│   ├── CatalogueView.hpp       # New: card grid UI, search/filter, click handler
│   └── CatalogueView.cpp       # Card layout, scrolling, prefetch progress display
├── render/
│   ├── ThumbnailRenderer.hpp   # New: offscreen FBO manager, progressive render queue
│   └── ThumbnailRenderer.cpp   # FBO creation, render-to-texture, PNG caching
├── data/
│   └── (existing files)        # DataFusionEngine, CacheManager already support bulk operations
└── core/
    └── Application.cpp         # Wire up prefetch on init, catalogue view lifecycle

.cache/
└── thumbnails/
    ├── kepler-186f.png        # 128x128 or 256x256 PNGs, named by planet slug
    └── trappist-1e.png
```

### Pattern 1: Vulkan Offscreen FBO Rendering
**What:** Render planet to an offscreen framebuffer, then read pixels back to CPU for PNG encoding.
**When to use:** Generate thumbnail images without affecting main viewport.
**Implementation approach:**

```cpp
// ThumbnailRenderer.hpp
class ThumbnailRenderer {
public:
    ThumbnailRenderer(VkDevice device, VmaAllocator allocator, uint32_t size = 256);
    ~ThumbnailRenderer();

    // Render planet to FBO and return ImTextureID for ImGui
    ImTextureID renderThumbnail(const PlanetParams& params, const Camera& camera);

    // Save FBO contents to PNG file
    bool saveToPNG(const std::string& filepath);

private:
    void createOffscreenResources();
    void recordRenderCommands(const PlanetParams& params);
    void readbackPixels(std::vector<uint8_t>& pixels);

    VkDevice m_device;
    VmaAllocator m_allocator;
    uint32_t m_size;

    VkImage m_colorImage;
    VmaAllocation m_colorAlloc;
    VkImageView m_colorView;
    VkImage m_depthImage;
    VmaAllocation m_depthAlloc;
    VkImageView m_depthView;
    VkRenderPass m_renderPass;
    VkFramebuffer m_framebuffer;
    VkSampler m_sampler;
    VkDescriptorSet m_descriptorSet; // For ImGui texture
};
```

**Key steps:**
1. Create offscreen color image: `VK_FORMAT_R8G8B8A8_UNORM`, `VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT`
2. Create depth image: `VK_FORMAT_D32_SFLOAT`, `VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT`
3. Create render pass: color + depth attachments, clear values
4. Create framebuffer: attach color + depth views
5. Record render commands: bind pipeline, set viewport/scissor, draw sphere mesh
6. Transition color image to `VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL`, copy to staging buffer via `vkCmdCopyImageToBuffer`
7. Map staging buffer, write PNG via `stb_image_write_png`
8. Create sampler + descriptor set, register with ImGui via `ImGui_ImplVulkan_AddTexture()`

**Example (offscreen FBO creation):**
```cpp
void ThumbnailRenderer::createOffscreenResources() {
    // Color attachment
    VkImageCreateInfo colorCI{};
    colorCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    colorCI.imageType = VK_IMAGE_TYPE_2D;
    colorCI.format = VK_FORMAT_R8G8B8A8_UNORM;
    colorCI.extent = {m_size, m_size, 1};
    colorCI.mipLevels = 1;
    colorCI.arrayLayers = 1;
    colorCI.samples = VK_SAMPLE_COUNT_1_BIT;
    colorCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    colorCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                    VK_IMAGE_USAGE_SAMPLED_BIT |
                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    colorCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocCI{};
    allocCI.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    vmaCreateImage(m_allocator, &colorCI, &allocCI, &m_colorImage, &m_colorAlloc, nullptr);

    // Depth attachment (similar pattern)
    // Create VkImageView for both
    // Create VkRenderPass with color + depth attachments
    // Create VkFramebuffer attaching views
}
```

### Pattern 2: ImGui Card Grid Layout
**What:** Manual grid layout in a scrollable ImGui child window, with cards showing planet data + thumbnail.
**When to use:** Display catalogue as visual grid (not ImGui::Table).
**Example:**
```cpp
// CatalogueView::render()
ImGui::BeginChild("##catalogue", {0, 0}, false, ImGuiWindowFlags_HorizontalScrollbar);

const float cardW = 200.f;
const float cardH = 280.f;
const float spacing = 16.f;
float availW = ImGui::GetContentRegionAvail().x;
int cols = std::max(1, int((availW + spacing) / (cardW + spacing)));

for (int i = 0; i < m_planets.size(); ++i) {
    ImGui::PushID(i);

    // Calculate grid position
    int row = i / cols;
    int col = i % cols;
    ImVec2 pos = ImGui::GetCursorScreenPos();
    pos.x = col * (cardW + spacing);
    pos.y = row * (cardH + spacing);
    ImGui::SetCursorScreenPos(pos);

    // Card background
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(pos, {pos.x + cardW, pos.y + cardH}, IM_COL32(30, 30, 35, 255), 8.f);

    // Thumbnail (if available)
    if (m_planets[i].thumbnailID) {
        ImGui::SetCursorScreenPos({pos.x + 8, pos.y + 8});
        ImGui::Image(m_planets[i].thumbnailID, {cardW - 16, 180});
    } else {
        // Placeholder
        dl->AddRectFilled({pos.x + 8, pos.y + 8}, {pos.x + cardW - 8, pos.y + 188},
                          IM_COL32(50, 50, 60, 255), 4.f);
    }

    // Planet name
    ImGui::SetCursorScreenPos({pos.x + 8, pos.y + 200});
    ImGui::Text("%s", m_planets[i].name.c_str());

    // Click detection
    ImGui::SetCursorScreenPos(pos);
    if (ImGui::InvisibleButton("##card", {cardW, cardH})) {
        onPlanetClicked(i);
    }

    ImGui::PopID();
}

ImGui::EndChild();
```

### Pattern 3: Progressive Thumbnail Rendering
**What:** Render thumbnails in background, one per frame, prioritizing visible cards.
**When to use:** Avoid blocking UI while generating 500+ thumbnails.
**Example:**
```cpp
// Application::update()
if (!m_thumbnailQueue.empty() && !m_isRenderingThumbnail) {
    // Get next planet from queue (prioritize visible cards)
    int planetIdx = m_thumbnailQueue.front();
    m_thumbnailQueue.pop();

    // Launch async thumbnail render
    m_thumbnailFuture = std::async(std::launch::async, [this, planetIdx]() {
        auto& planet = m_catalogue[planetIdx];

        // Check disk cache first
        std::string cachePath = fmt::format(".cache/thumbnails/{}.png", planet.slug);
        if (std::filesystem::exists(cachePath)) {
            // Load from PNG, upload to GPU, return ImTextureID
            return loadThumbnailFromPNG(cachePath);
        }

        // Render offscreen
        PlanetParams params = m_mapper->mapToParams(planet.data);
        ImTextureID texID = m_thumbnailRenderer->renderThumbnail(params, m_thumbnailCamera);

        // Save to PNG cache
        m_thumbnailRenderer->saveToPNG(cachePath);

        return texID;
    });

    m_isRenderingThumbnail = true;
}

// Poll async result
if (m_isRenderingThumbnail && m_thumbnailFuture.wait_for(0ms) == std::future_status::ready) {
    ImTextureID texID = m_thumbnailFuture.get();
    m_catalogue[m_currentThumbnailIdx].thumbnailID = texID;
    m_isRenderingThumbnail = false;
}
```

### Pattern 4: Thumbnail Disk Caching with stb_image_write
**What:** Serialize rendered thumbnails as PNGs for instant reload across sessions.
**When to use:** After successfully rendering an offscreen FBO.
**Example:**
```cpp
bool ThumbnailRenderer::saveToPNG(const std::string& filepath) {
    // Read pixels from FBO via staging buffer
    std::vector<uint8_t> pixels(m_size * m_size * 4);
    readbackPixels(pixels); // vkCmdCopyImageToBuffer + map staging

    // Flip Y (Vulkan origin is top-left, PNG is bottom-left)
    std::vector<uint8_t> flipped(pixels.size());
    for (uint32_t y = 0; y < m_size; ++y) {
        std::memcpy(flipped.data() + y * m_size * 4,
                    pixels.data() + (m_size - 1 - y) * m_size * 4,
                    m_size * 4);
    }

    // Write PNG (stb_image_write.h)
    std::filesystem::create_directories(std::filesystem::path(filepath).parent_path());
    return stbi_write_png(filepath.c_str(), m_size, m_size, 4, flipped.data(), m_size * 4) != 0;
}

// Load cached PNG
ImTextureID loadThumbnailFromPNG(const std::string& filepath) {
    int w, h, ch;
    uint8_t* data = stbi_load(filepath.c_str(), &w, &h, &ch, 4);
    if (!data) return nullptr;

    // Upload to GPU as VkImage
    VkImage img = createImageFromPixels(data, w, h);
    free(data);

    // Create sampler + descriptor set for ImGui
    return ImGui_ImplVulkan_AddTexture(sampler, imgView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}
```

### Anti-Patterns to Avoid
- **Per-thumbnail FBO allocation:** Creates VRAM fragmentation and cleanup complexity. Use a single reusable FBO.
- **Rendering all thumbnails on startup:** Blocks UI for 10+ seconds. Use progressive lazy rendering.
- **Synchronous rendering in main thread:** Causes stuttering. Use `std::async` for offscreen work.
- **Forgetting image layout transitions:** Vulkan images must transition layouts (UNDEFINED → COLOR_ATTACHMENT → TRANSFER_SRC → SHADER_READ).
- **Not caching to disk:** Regenerating thumbnails on every launch wastes CPU/GPU. PNGs survive restarts.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Vulkan memory allocation | Manual VkDeviceMemory suballocator | VulkanMemoryAllocator (VMA) | Already integrated; handles fragmentation, memory types, aliasing edge cases |
| PNG encoding | Custom BMP or uncompressed format | stb_image_write.h | Already in project; battle-tested, single-header, 4-5 lines of code |
| Texture atlas packing | Rectangle bin-packing algorithm | Individual PNG files | Simpler cache invalidation, no repacking on adds/removes, survives restarts |
| Thumbnail render queue | Custom priority queue with visibility culling | `std::deque` + sort by scroll position | Sufficient for 500 items; premature optimization avoided |
| Async thumbnail loading | Custom thread pool | `std::async` | C++23 standard; sufficient for serial thumbnail generation |

**Key insight:** Vulkan offscreen rendering has many subtle pitfalls (image layout transitions, synchronization, memory barriers). Follow proven patterns from existing `VulkanRenderer::Impl` code (see noise texture upload, starmap loading). Don't improvise Vulkan commands.

## Common Pitfalls

### Pitfall 1: Forgetting Image Layout Transitions
**What goes wrong:** Rendering to offscreen FBO produces garbage or crashes with validation errors.
**Why it happens:** Vulkan images must explicitly transition layouts (UNDEFINED → COLOR_ATTACHMENT, then COLOR_ATTACHMENT → TRANSFER_SRC for readback, then TRANSFER_SRC → SHADER_READ for ImGui display).
**How to avoid:** Use `VkImageMemoryBarrier` with `vkCmdPipelineBarrier` at every layout change. Copy-paste from existing noise texture upload code (lines 706-728 in VulkanRenderer.cpp).
**Warning signs:** Validation layer errors like "image layout mismatch" or "access mask conflict."

### Pitfall 2: Rendering Thumbnails on Main Thread
**What goes wrong:** UI freezes for seconds while generating thumbnails.
**Why it happens:** 500 renders × 10ms each = 5 seconds of blocking work.
**How to avoid:** Render one thumbnail per frame in a background task via `std::async`. Show placeholder cards immediately, swap in real thumbnails as they complete.
**Warning signs:** UI unresponsive during catalogue initialization.

### Pitfall 3: Not Reusing Offscreen FBO
**What goes wrong:** VRAM exhausted with 500+ FBOs (256×256×4 bytes = 256KB each → 128MB total).
**Why it happens:** Creating one FBO per planet instead of reusing a single FBO serially.
**How to avoid:** Allocate one FBO at ThumbnailRenderer construction, render planets sequentially, immediately copy pixels to staging buffer after each render.
**Warning signs:** VMA allocation failures, "out of device memory" errors.

### Pitfall 4: Y-Axis Flip Confusion
**What goes wrong:** Thumbnails appear upside-down in ImGui or saved PNGs.
**Why it happens:** Vulkan framebuffer origin is top-left, PNG/ImGui expects bottom-left. Viewport Y-flip doesn't affect readback.
**How to avoid:** Manually flip pixel rows when copying from staging buffer (see Pattern 4 example). Or set negative viewport height (Vulkan 1.1+ feature).
**Warning signs:** Thumbnails vertically mirrored.

### Pitfall 5: Thumbnail Cache Invalidation
**What goes wrong:** Stale thumbnails shown after changing planet data or rendering code.
**Why it happens:** PNG filename based only on planet name, not on data version or renderer hash.
**How to avoid:** Include a cache version number in filename (e.g., `kepler-186f-v2.png`) or store metadata JSON with ExoplanetData hash. Bump version when rendering code changes.
**Warning signs:** Thumbnails don't update after tweaking shader/params.

## Code Examples

Verified patterns from existing codebase:

### Vulkan Image Creation (from VulkanRenderer.cpp:653-668)
```cpp
// Create GPU-only image for rendering
VkImageCreateInfo imgCI{};
imgCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
imgCI.imageType = VK_IMAGE_TYPE_2D;
imgCI.format = VK_FORMAT_R8G8B8A8_UNORM;
imgCI.extent = {256, 256, 1};
imgCI.mipLevels = 1;
imgCI.arrayLayers = 1;
imgCI.samples = VK_SAMPLE_COUNT_1_BIT;
imgCI.tiling = VK_IMAGE_TILING_OPTIMAL;
imgCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
imgCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

VmaAllocationCreateInfo allocCI{};
allocCI.usage = VMA_MEMORY_USAGE_GPU_ONLY;
vmaCreateImage(allocator, &imgCI, &allocCI, &image, &allocation, nullptr);
```

### Image Layout Transition (from VulkanRenderer.cpp:706-728)
```cpp
// Transition UNDEFINED → TRANSFER_DST
VkImageMemoryBarrier barrier{};
barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
barrier.srcAccessMask = 0;
barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
barrier.image = image;
barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
vkCmdPipelineBarrier(cmd,
    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    VK_PIPELINE_STAGE_TRANSFER_BIT,
    0, 0, nullptr, 0, nullptr, 1, &barrier);

// Later: TRANSFER_DST → SHADER_READ
barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
vkCmdPipelineBarrier(cmd,
    VK_PIPELINE_STAGE_TRANSFER_BIT,
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    0, 0, nullptr, 0, nullptr, 1, &barrier);
```

### ImGui Scrollable Card Grid (adapted from GalaxyView.cpp:451-471)
```cpp
// Scrollable child window with card layout
if (ImGui::BeginChild("##catalogue", {-1.f, -1.f}, true, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
    const float cardW = 200.f;
    const float cardH = 280.f;
    const float spacing = 16.f;

    float availW = ImGui::GetContentRegionAvail().x;
    int cols = std::max(1, int((availW + spacing) / (cardW + spacing)));

    for (int i = 0; i < planets.size(); ++i) {
        int col = i % cols;

        // Start new row
        if (col == 0 && i > 0) {
            ImGui::Dummy({0, spacing}); // vertical spacing
        }

        // Card rendering (see Pattern 2)
        ImGui::PushID(i);
        // ... card background, thumbnail, text, click detection
        ImGui::PopID();

        // Same-line for grid
        if (col < cols - 1) {
            ImGui::SameLine(0, spacing);
        }
    }
}
ImGui::EndChild();
```

### Async Prefetch with Progress (adapted from existing DataFusionEngine)
```cpp
// Application::init()
m_prefetchFuture = m_dataFusion->prefetchNotable(500);
m_prefetchProgress = std::make_shared<std::atomic<int>>(0);

// In DataFusionEngine::prefetchNotable()
std::future<std::vector<ExoplanetData>> DataFusionEngine::prefetchNotable(int count) {
    return std::async(std::launch::async, [this, count]() {
        std::vector<ExoplanetData> results;
        results.reserve(count);

        // Fetch from NASA query (top N by discovery date)
        auto records = m_nasa->queryTopN(count, "disc_year DESC");

        for (size_t i = 0; i < records.size(); ++i) {
            auto data = fetchAndFuseSync(records[i].name);
            results.push_back(data);

            // Update progress atomically
            if (m_progressCounter) {
                m_progressCounter->store(i + 1);
            }
        }

        return results;
    });
}

// Application::update()
if (m_prefetchFuture.valid() && m_prefetchFuture.wait_for(0ms) == std::future_status::ready) {
    m_catalogueData = m_prefetchFuture.get();
    m_prefetchComplete = true;
}

// UIManager catalogue header
if (!m_prefetchComplete) {
    ImGui::Text("Loading... %d/%d planets", m_prefetchProgress->load(), 500);
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| ImGui::ListBox for catalogues | Card grid with ImGui::Image thumbnails | ImGui 1.80+ (2021) | More visual, scalable to large datasets, better UX for browsing |
| CPU-side thumbnail rendering | GPU offscreen FBO rendering | Vulkan 1.0 (2016) | 10-100× faster, leverages existing shader pipeline |
| Texture atlas packing | Individual textures with caching | Vulkan 1.2 dynamic rendering (2020) | Simpler management, no repacking overhead, better cache locality |
| Synchronous rendering | Async progressive rendering | C++11 std::async (2011) | Non-blocking UI, perceived performance improvement |

**Deprecated/outdated:**
- **OpenGL glReadPixels for thumbnails:** Project uses Vulkan; VkCmdCopyImageToBuffer is the Vulkan equivalent.
- **ImGui::Columns for grids:** Replaced by ImGui::Table in 1.80+, but manual layout is more flexible for cards.
- **Manual Vulkan memory management:** VMA (VulkanMemoryAllocator) is industry standard since 2017; project already uses it.

## Open Questions

1. **Thumbnail resolution: 128px or 256px?**
   - What we know: 128px = 64KB/PNG, 256px = 256KB/PNG. 500 planets = 32MB vs 128MB disk cache.
   - What's unclear: Visual quality threshold for planet detail at card size. VRAM budget for in-flight textures.
   - Recommendation: Start with 128px, add a config setting. User can increase if quality insufficient. Most modern monitors can display 128px cards clearly at typical UI scales.

2. **Hover animation: rotate planet or pulse scale?**
   - What we know: User decision: animate on hover, only 1 at a time. Rotation requires continuous re-rendering.
   - What's unclear: Performance cost of re-rendering one thumbnail per frame (likely 1-2ms, acceptable).
   - Recommendation: Slow rotation (10 sec/rev) via updated PlanetParams::rotationOffset. Reuse existing offscreen FBO, render to same texture, ImGui automatically updates display. Stop rotation on unhover.

3. **Filter UI: dropdown or sidebar chips?**
   - What we know: Minimum filters: planet type, habitable zone toggle. Search bar with autocomplete.
   - What's unclear: Number of filter options grows complex with sidebar (types: terrestrial, gas giant, ice giant, super-Earth; zones: hot, temperate, cold).
   - Recommendation: Top-bar chip layout (ImGui::Selectable in horizontal row) for common filters, dropdown (ImGui::Combo) for advanced. Matches modern web catalogue UX (e.g., Steam, Netflix).

4. **Thumbnail cache invalidation strategy?**
   - What we know: Rendering code or planet data changes invalidate thumbnails.
   - What's unclear: How to detect data changes (ExoplanetData has no version field).
   - Recommendation: Store cache metadata JSON per thumbnail with planet data hash + renderer version string. On load, compare hashes, regenerate if mismatch. Simple versioning scheme: `CACHE_VERSION=2` constant in code, bump when shaders change.

## Sources

### Primary (HIGH confidence)
- Existing codebase: `src/render/VulkanRenderer.cpp` (lines 653-759 for image creation, layout transitions, staging buffers)
- Existing codebase: `src/ui/GalaxyView.cpp` (lines 451-523 for scrollable list + selectable items)
- Existing codebase: `src/data/DataFusionEngine.hpp` (line 24 for `prefetchNotable(500)` API)
- Existing codebase: `cmake/Dependencies.cmake` (line 79 for ImGui 1.91.6-docking version)
- VMA documentation: `build/_deps/vulkanmemoryallocator-src/README.md` (usage patterns, allocation flags)
- Vulkan 1.2 specification (image layout transitions, framebuffer attachment rules, transfer operations)

### Secondary (MEDIUM confidence)
- stb_image_write.h documentation: Single-header PNG/BMP/TGA encoding, trivial API (`stbi_write_png`)
- ImGui backends documentation: `imgui_impl_vulkan.h` comments for `ImGui_ImplVulkan_AddTexture()` usage
- Dear ImGui demo code: Card layout patterns from `imgui_demo.cpp` (not in project, but standard reference)

### Tertiary (LOW confidence)
- General Vulkan best practices: Reuse command buffers, minimize pipeline barriers, batch uploads (not specific to this project)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - All libraries already integrated and versions confirmed from Dependencies.cmake
- Architecture: HIGH - Patterns verified from existing VulkanRenderer.cpp and GalaxyView.cpp code
- Pitfalls: MEDIUM-HIGH - Based on common Vulkan issues and project-specific patterns; not all encountered in this codebase yet

**Research date:** 2026-03-01
**Valid until:** 2026-04-30 (60 days for stable libraries; Vulkan API and ImGui stable)
