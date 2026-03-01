# Phase 4: Exoplanet Catalogue Browser - Planning Summary

**Date:** 2026-03-01
**Phase Goal:** Build a browseable catalogue of 500+ known exoplanets with mini-render previews, sorted by discovery date.
**Plans Created:** 3 plans (1286 lines)
**Total Tasks:** 6 tasks
**Wave Structure:** 2 waves (Plans 01-02 parallel in Wave 1, Plan 03 in Wave 2)

---

## Plan Decomposition Strategy

**Chosen approach:** Vertical slice by concern + dependency wave separation

- **Plan 01 (Wave 1):** Catalogue UI infrastructure — card grid, filtering, sorting, search, prefetch integration
- **Plan 02 (Wave 1):** Offscreen rendering infrastructure — ThumbnailRenderer with Vulkan FBO, PNG caching, ImGui texture registration
- **Plan 03 (Wave 2):** Integration layer — progressive thumbnail generation, cache loading, hover animation (depends on 01+02)

**Why this decomposition:**
- Plans 01 and 02 are fully independent and can execute in parallel (no file conflicts, no logical dependencies)
- Plan 01 provides the UI foundation with placeholder cards (immediate catalogue functionality)
- Plan 02 provides the rendering engine (reusable FBO infrastructure)
- Plan 03 wires them together with progressive rendering and caching logic
- Each plan is 2-3 tasks, targeting ~40-50% context budget
- Clear test boundaries: Plan 01 = UI verification, Plan 02 = rendering tests, Plan 03 = integration + checkpoint

---

## Requirements Coverage

| Requirement | Plans | Verification |
|-------------|-------|--------------|
| R4.1: Local Exoplanet Database (prefetch 500, cache locally, background refresh) | 01 | Prefetch triggered on init, cache integration, loading progress display |
| R4.2: Catalogue UI (scrollable grid, discovery date sort, filters, click to render) | 01 | Card grid with filters/sort/search, click handler wired to loadPlanet() |
| R4.3: Mini Render Previews (offscreen FBO, cached textures, progressive rendering) | 02, 03 | ThumbnailRenderer FBO (Plan 02), progressive generation + PNG cache (Plan 03) |

**Coverage complete:** All three requirements mapped to tasks across three plans.

---

## Locked User Decisions (from CONTEXT.md)

### Fully Honored
- ✅ **Card grid layout** (Pokedex-style, not list) — Plan 01 Task 1
- ✅ **Catalogue replaces Galaxy view** — Plan 01 Task 2 (Application.cpp integration)
- ✅ **Click card → loads planet with existing fade transition** — Plan 01 Task 2 (onCataloguePlanetClicked)
- ✅ **Deferred rendering: placeholders first, real thumbnails lazily** — Plan 01 (placeholder cards), Plan 03 (progressive rendering)
- ✅ **Resolution: 128-256px (Claude's discretion)** — Started with 128px, checkpoint in Plan 03 to decide if 256px needed
- ✅ **Static snapshots, animate on hover (only 1 at a time)** — Plan 03 Task 2 (hover animation with rotationOffset)
- ✅ **Disk cache thumbnails as PNGs in .cache/thumbnails/** — Plan 02 (saveToPNG), Plan 03 (cache loading)
- ✅ **Default sort: discovery date (newest first)** — Plan 01 Task 1 (sort controls)
- ✅ **Filters: planet type + habitable zone toggle at minimum** — Plan 01 Task 1 (chip layout)
- ✅ **Search bar with autocomplete (reuse existing pattern)** — Plan 01 Task 1 (prefix matching)
- ✅ **Prefetch 500+ records at startup using prefetchNotable(500)** — Plan 01 Task 2 (Application::init)
- ✅ **Show loading count: "Loading... 142/500 planets"** — Plan 01 Task 1, Task 2 (progress display)
- ✅ **Offline-capable: cache all, show cached data immediately** — Plan 01 Task 2 (CacheManager integration)
- ✅ **AI inference: on-demand only (when selected for full rendering)** — Plan 03 (catalogue uses physics-based defaults for thumbnails)
- ✅ **Use SolarSystemDatabase analog matching for catalogue thumbnails** — Plan 03 Task 2 (findClosestAnalog fallback)

### Claude's Discretion Exercised
- Card dimensions: 200×280px (based on typical UI sizing and grid layout math)
- Placeholder design: Colored rectangles with planet type colors (brown, orange, blue, green)
- Thumbnail resolution: Started with 128px (RESEARCH.md recommendation), checkpoint to increase if needed
- Filter UI: Horizontal chip layout (matches modern web catalogue UX like Steam, Netflix)
- Sort options: Discovery date + mass + radius + name (covers most common browse patterns)
- Loading skeleton: Simple text indicator ("Loading... X/500 planets") — no elaborate skeleton UI

### Deferred Ideas
- None — discussion stayed within phase scope per CONTEXT.md

---

## Parallel Execution Analysis

### Wave 1 (Plans 01, 02)
**Why parallel:**
- Plan 01 modifies: `src/ui/CatalogueView.*`, `src/core/Application.*`
- Plan 02 modifies: `src/render/ThumbnailRenderer.*`, `cmake/Dependencies.cmake`
- **No file overlap** → safe for parallel execution
- Plan 01 can be tested with placeholder cards (no rendering needed)
- Plan 02 can be tested independently (unit test + manual PNG generation)

**Estimated duration:**
- Plan 01: ~45 minutes (UI implementation, straightforward ImGui patterns)
- Plan 02: ~60 minutes (Vulkan FBO setup, image layout transitions, stb integration)
- **Parallel execution: ~60 minutes total** (limited by Plan 02)

### Wave 2 (Plan 03)
**Why sequential:**
- Depends on CatalogueView API (Plan 01: setThumbnail, getHoveredCardIndex)
- Depends on ThumbnailRenderer API (Plan 02: renderThumbnail, saveToPNG)
- Integrates both into Application update loop

**Estimated duration:**
- Plan 03: ~75 minutes (async rendering, cache loading, hover logic, checkpoint verification)

**Total phase estimate: ~135 minutes (~2.25 hours) of Claude execution time**

---

## Must-Haves Derivation (Goal-Backward)

### Phase Goal (from ROADMAP.md)
"Build a browseable catalogue of known exoplanets with mini-render previews, sorted by discovery date."

### Observable Truths (What must be TRUE for goal to be achieved)
1. User can browse a grid of 500+ planet cards on app startup ✓
2. User can filter planets by type and habitable zone status ✓
3. User can sort planets by discovery date (newest first by default) ✓
4. User can search planets by name with autocomplete ✓
5. Catalogue shows loading progress during prefetch ✓
6. Catalogue cards show real planet thumbnails (after progressive rendering) ✓
7. Thumbnails are cached as PNG files and survive app restarts ✓
8. Hovering over a card rotates the planet preview (only one at a time) ✓
9. Clicking a card loads the planet into main renderer ✓
10. Catalogue loads instantly on subsequent launches (offline-capable) ✓

### Required Artifacts (What must EXIST for truths to be true)
- `src/ui/CatalogueView.hpp/cpp` — Card grid rendering, filters, sort, search
- `src/render/ThumbnailRenderer.hpp/cpp` — Offscreen FBO, render-to-texture, PNG save
- `src/core/Application` enhancements — Prefetch trigger, thumbnail queue, hover animation
- `.cache/thumbnails/` directory — Persistent PNG cache (created at runtime)
- `cmake/Dependencies.cmake` — stb_image_write integration

### Key Links (Critical connections where breakage causes cascading failures)
- **Prefetch → CatalogueView:** Application::init() → prefetchNotable(500) → catalogue data population. If broken: empty catalogue.
- **Thumbnail render → PNG cache:** ThumbnailRenderer::renderThumbnail() → saveToPNG(). If broken: thumbnails don't persist, regenerate every launch.
- **Cache load → ImGui display:** loadThumbnailsFromCache() → ImGui_ImplVulkan_AddTexture() → CatalogueView::render() → ImGui::Image(). If broken: cached thumbnails not displayed.
- **Hover → animation:** CatalogueView hover detection → Application query → ThumbnailRenderer re-render. If broken: no hover animation.
- **Card click → planet load:** CatalogueView::onCardClicked() → Application::loadPlanet(). If broken: clicking cards does nothing.

---

## Risk Assessment

### High Risk Areas
1. **Vulkan offscreen FBO complexity** (Plan 02)
   - Image layout transitions have many subtle pitfalls (UNDEFINED → COLOR_ATTACHMENT → TRANSFER_SRC → SHADER_READ)
   - Mitigation: Copy-paste existing patterns from VulkanRenderer.cpp (lines 706-728 per RESEARCH.md)
   - Verification: Unit test + manual PNG output check

2. **VulkanRenderer API exposure** (Plan 03)
   - ThumbnailRenderer needs device, allocator, queue, cmdPool from VulkanRenderer
   - Current VulkanRenderer may not expose these via public getters
   - Mitigation: Add getter methods in VulkanRenderer.hpp (simple, non-breaking)

3. **Thumbnail rendering quality** (Plan 02-03 limitation)
   - Plan 02 renders simplified colored circles (not full procedural planets) per "deferred rendering" decision
   - User may expect high-fidelity previews
   - Mitigation: Checkpoint in Plan 03 to gather user feedback on quality; can upgrade in future optimization

### Medium Risk Areas
1. **Progressive rendering performance** (Plan 03)
   - Generating 500 thumbnails could take 5-25 seconds
   - Mitigation: Async one-per-frame, prioritize visible cards, non-blocking UI

2. **PNG cache invalidation** (Plan 03)
   - Cached thumbnails become stale if rendering code changes
   - Mitigation: Include cache version in filename (e.g., `planet-v2.png`) or metadata JSON
   - Current plan: Defer to future optimization (user can manually delete cache)

3. **Memory budget** (Plan 02-03)
   - 500 thumbnails × 128×128 × 4 bytes = 32MB VRAM (if all loaded)
   - Mitigation: Single reusable FBO for generation (saves memory), lazy load textures (only load visible cards into VRAM)

### Low Risk Areas
1. **UI implementation** (Plan 01) — Standard ImGui patterns, well-understood
2. **Prefetch integration** (Plan 01) — Already tested in Phase 01 (DataFusionEngine::prefetchNotable exists)
3. **Cache loading** (Plan 03) — std::filesystem is stable, stb_image is battle-tested

---

## Scope Boundaries

### In Scope for Phase 4
- Catalogue UI with card grid layout
- Filtering (planet type, habitable zone)
- Sorting (discovery date, mass, radius, name)
- Search with autocomplete
- Prefetch 500 planets on startup
- Offscreen Vulkan FBO rendering
- PNG thumbnail caching to disk
- Progressive thumbnail generation (deferred, lazy)
- Hover-to-animate (slow rotation, one at a time)
- Offline-first cache loading
- Click card → load planet in main renderer

### Explicitly Out of Scope (Deferred)
- High-fidelity procedural planet thumbnails (Plan 02 renders simplified colored circles; full integration deferred)
- Advanced filters (metallicity, eccentricity, discovery method)
- Side-by-side planet comparison (future milestone)
- Real-time orbit animation in thumbnails (static + hover rotation only)
- Texture atlas packing (individual PNG files chosen for simplicity)
- Thumbnail cache versioning/invalidation (manual cache deletion for now)
- Multi-planet system visualization (single planet per card)
- Custom card layouts (size, spacing, aspect ratio — fixed at 200×280px)

---

## Testing Strategy

### Plan 01 (Catalogue UI)
- **Automated:** Compile test (no unit tests for UI logic)
- **Manual:** Launch app, verify card grid renders, test filters/sort/search, verify prefetch progress display
- **Success metric:** Catalogue displays 500+ placeholder cards, filters/sort functional, no crashes

### Plan 02 (ThumbnailRenderer)
- **Automated:** Unit test for FBO creation, renderThumbnail, saveToPNG (skip if Vulkan unavailable)
- **Manual:** Standalone test program renders thumbnail, saves PNG, verify image in viewer
- **Success metric:** 128×128px PNG written to disk, no Vulkan validation errors

### Plan 03 (Integration + Progressive Generation)
- **Automated:** Build + launch smoke test, verify .cache/thumbnails/ contains PNGs
- **Manual (Checkpoint):** Verify thumbnail quality, hover animation, cache persistence
- **Success metric:** Thumbnails display in cards, hover animation smooth, cache survives restart

---

## Checkpoint Decision Point (Plan 03)

**Question:** Is 128px thumbnail resolution sufficient, or should we increase to 256px?

**Context:**
- 128px = 32MB disk cache, faster generation, lower VRAM
- 256px = 128MB disk cache, 4× slower generation, 4× more VRAM, higher quality

**Verification at checkpoint:**
- Open several cached PNGs in image viewer
- Check if planet features are recognizable at 128×128px
- Test on high-DPI display (if available) to assess pixelation

**Decision criteria:**
- If planets look crisp and recognizable → keep 128px
- If planets are too pixelated or lack detail → increase to 256px and regenerate

**Impact of decision:**
- Low: Change ThumbnailRenderer constructor argument from 128 to 256
- Medium: Delete .cache/thumbnails/ and regenerate (one-time cost)
- No code changes required beyond constructor argument

---

## File Modifications Summary

| File | Plan | Modifications | Lines |
|------|------|---------------|-------|
| `src/ui/CatalogueView.hpp` | 01, 03 | New class declaration, thumbnail state | ~80 |
| `src/ui/CatalogueView.cpp` | 01, 03 | Card grid, filters, sort, search, thumbnail display | ~250 |
| `src/core/Application.hpp` | 01, 03 | Catalogue members, prefetch, thumbnail queue | ~20 |
| `src/core/Application.cpp` | 01, 03 | Init prefetch, thumbnail generation loop, hover animation | ~150 |
| `src/render/ThumbnailRenderer.hpp` | 02 | New class declaration | ~60 |
| `src/render/ThumbnailRenderer.cpp` | 02 | Offscreen FBO, render, PNG save, ImGui registration | ~300 |
| `cmake/Dependencies.cmake` | 02 | stb_image_write integration | ~10 |
| `tests/test_thumbnail_renderer.cpp` | 02 | Unit test for rendering and PNG save | ~80 |

**Total new lines: ~950** (excluding blank/comment lines)
**Total plans: 1286 lines** (including frontmatter, context, verification)

---

## Context Budget Analysis

| Plan | Tasks | Estimated Context | Quality Zone |
|------|-------|-------------------|--------------|
| 01 | 2 tasks (UI impl + wiring) | ~40% | GOOD (30-50%) |
| 02 | 2 tasks (FBO + test) | ~45% | GOOD (30-50%) |
| 03 | 2 tasks + checkpoint | ~50% | GOOD (30-50%) |

**All plans target 40-50% context** per planner instructions. No plan should degrade into "efficiency mode" (>70%).

---

## Interface Contracts for Executors

Plans include full interface extraction from existing codebase to prevent "scavenger hunt" anti-pattern:

- **Plan 01:** DataFusionEngine API, ExoplanetData struct, Application members, UIManager theme
- **Plan 02:** VulkanRenderer Impl patterns, VMA usage, ImGui_ImplVulkan API, stb_image_write
- **Plan 03:** ThumbnailRenderer API (from Plan 02), CatalogueView API (from Plan 01), ExoplanetMapper

Executors can implement tasks without reading the full codebase — all necessary contracts provided inline.

---

## Success Criteria (Phase-Level)

Upon completion of all three plans:

- [ ] Catalogue displays 500+ planet cards in grid layout
- [ ] Default sort: discovery date (newest first)
- [ ] Filters functional: planet type (All/Terrestrial/Gas/Ice/Super-Earth) + habitable zone toggle
- [ ] Search bar filters by name with autocomplete
- [ ] Prefetch runs in background on app startup, progress visible
- [ ] Thumbnails progressively replace placeholder cards
- [ ] Hover over card: planet slowly rotates (10 sec/rotation)
- [ ] Only one planet animates at a time
- [ ] Thumbnails cached as PNGs in .cache/thumbnails/
- [ ] On restart: thumbnails load instantly from cache
- [ ] Click card: planet loads in main renderer, catalogue hides
- [ ] Back button: returns to catalogue
- [ ] No Vulkan validation errors, no crashes, no memory leaks
- [ ] Checkpoint verified: thumbnail quality acceptable (128px or 256px)

---

## Commit Message

```
plan(04): create phase 4 execution plans - catalogue UI, offscreen rendering, progressive thumbnails

- Plan 01: CatalogueView with card grid, filters, sorting, search (Wave 1, autonomous)
- Plan 02: ThumbnailRenderer with offscreen FBO, PNG caching (Wave 1, autonomous)
- Plan 03: Progressive thumbnail generation, cache loading, hover animation (Wave 2, checkpoint)

Requirements covered: R4.1, R4.2, R4.3
Dependencies: Plan 03 depends on 01+02 (wave 2)
User decisions honored: card grid layout, deferred rendering, 500 prefetch, hover animation
```

---

**Planning complete. Ready for execution via `/gsd:execute-phase`.**
