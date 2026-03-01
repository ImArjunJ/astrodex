# Phase 4: Exoplanet Catalogue Browser - Context

**Gathered:** 2026-03-01
**Status:** Ready for planning

<domain>
## Phase Boundary

Build a browseable catalogue of 500+ known exoplanets with mini-render previews, sorting, and filtering. Users can scan the catalogue, sort by discovery date or other criteria, filter by planet type, and click to load any planet into the main renderer. Creating new data sources or modifying the inference pipeline are separate concerns.

</domain>

<decisions>
## Implementation Decisions

### Catalogue Layout
- Card grid layout (Pokedex-style) — visual and scannable
- Catalogue replaces the Galaxy view as the browse/landing screen
- Click a card to load the planet into the main renderer with the existing fade transition
- Back button returns to catalogue
- Claude's discretion on card info density — pick the right balance of data per card (name, type, discovery year, key stat) based on available grid space

### Mini-Render Previews
- Deferred rendering: show colored placeholders initially, render real previews lazily as user scrolls them into view
- Resolution: Claude's discretion (128-256px based on layout needs and VRAM budget)
- Static snapshots by default, animate (slowly rotate) on hover — only 1 planet rendering at a time
- Disk cache thumbnails as PNGs in `.cache/thumbnails/` for instant reload across sessions

### Sorting & Filtering
- Default sort: discovery date (newest first) — matches roadmap requirement
- Claude's discretion on sort direction toggle and additional sort options
- Filters: planet type + habitable zone toggle at minimum
- Include a search bar with autocomplete within the catalogue (reuse existing pattern)
- Claude's discretion on filter UI complexity

### Data Prefetch Strategy
- Prefetch 500+ records at app startup in background using existing `prefetchNotable(500)` API
- Show subtle count in catalogue header during load ("Loading... 142/500 planets")
- Fully offline-capable: cache all fetched records, show cached data immediately on subsequent launches while refreshing in background
- AI inference: on-demand only (when a planet is selected for full rendering). Catalogue shows raw data with 'data incomplete' indicator for missing fields
- Use physics-based defaults / SolarSystemDatabase analog matching for catalogue thumbnail rendering

### Claude's Discretion
- Exact card dimensions and spacing
- Loading skeleton / placeholder design
- Error state handling (failed fetches, missing data)
- Thumbnail VRAM budget management
- Sort options beyond discovery date (mass, name, etc.)
- Filter UI layout (dropdown vs sidebar vs chips)

</decisions>

<specifics>
## Specific Ideas

- Pokedex feel — each planet is a collectible entry you can browse and inspect
- Galaxy view integration: catalogue replaces the galaxy view as the main browse screen
- Existing fade transition for planet loading keeps the UX consistent with search
- Hover-to-animate thumbnails gives a sense of each planet being "alive"

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `DataFusionEngine::prefetchNotable(500)`: Async bulk fetch API already exists
- `CacheManager`: File-based JSON cache with TTL, `listCached()` for enumeration
- `ExoplanetData`: Full data model with discovery_year, planet_type, MeasuredValue provenance
- `SolarSystemDatabase::findClosestAnalog()`: Fallback render params for planets missing AI inference
- `ExoplanetMapper`: Physics-to-PlanetParams conversion
- GalaxyView scrollable child window + selectable list pattern
- UIManager theme system (dark/light) — catalogue should match
- Existing autocomplete pattern for search

### Established Patterns
- Async/future with atomic stage reporting for background operations
- `setExoplanetCallback` / `setExoplanetStatus` pattern for UI updates from async pipeline
- ImGui tab bar (DATA/WORLD/VISUAL/LIGHT) in UIManager
- Fade transition (shrink out / grow in) for planet swaps
- Color-coded data quality (white=measured, cyan=AI, yellow=calculated)

### Integration Points
- `Application::init()`: Wire up prefetch trigger
- `Application::update()`: Poll prefetch future, feed results to catalogue
- UIManager: New catalogue panel (replaces or extends GalaxyView)
- VulkanRenderer: New offscreen FBO infrastructure for thumbnail rendering
- `ImGui_ImplVulkan_AddTexture()`: Bind rendered thumbnails as ImGui texture IDs

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 04-exoplanet-catalogue-browser*
*Context gathered: 2026-03-01*
