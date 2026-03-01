# Phase 3: Search & Render Integration - Context

**Gathered:** 2026-03-01
**Status:** Ready for planning

<domain>
## Phase Boundary

Connect the data+inference pipeline to the renderer so users can type an exoplanet name and see it rendered with real data. Includes: search autocomplete, data provenance UI, planet info panel, and smooth loading UX. The basic pipeline (search → fetch → infer → convert → render) already works end-to-end via `Application::loadPlanet()` — this phase enhances the UI layer around it.

</domain>

<decisions>
## Implementation Decisions

### Search & Autocomplete
- Dropdown below the search input showing matching planet names
- Planet name list sourced from local cache only (NasaApiClient cache + SolarSystemDatabase entries); grows as users search more planets
- Prefix matching for filtering (type "Kepler-4" → shows Kepler-4, Kepler-42b, Kepler-442b, etc.)
- Show 5-8 suggestions at a time, scrollable if more matches exist
- Reuse existing `m_searchBuf` in UIManager; add autocomplete popup via ImGui's popup/listbox system

### Data Provenance Display
- Color-code values inline in the planet info panel by DataSource:
  - White = measured (NASA_TAP, GAIA, CDS_VIZIER, OEC)
  - Cyan = AI-inferred (AI_INFERRED)
  - Yellow = calculated (CALCULATED)
- Hover tooltip on AI-inferred (cyan) values showing AI reasoning text and confidence percentage
- Small one-line legend at the top of the info panel: "● Measured  ● AI-Inferred  ● Calculated"
- Confidence levels shown in tooltip alongside reasoning (MeasuredValue already has `confidence` field)

### Planet Info Panel
- New collapsible section inside the existing Planet Editor window, below Exoplanet Lookup
- Only appears after a planet is loaded (hidden by default)
- Three collapsible subsections:
  - **Physical:** mass (M⊕), radius (R⊕), equilibrium temp (K), density, surface gravity
  - **Orbital:** period (days), semi-major axis (AU), eccentricity, inclination
  - **Host Star:** name, spectral type, distance (pc), effective temp
- Discovery info (method, year) shown inline with planet name header
- Friendly units with context: e.g. "Mass: 1.2 M⊕ (Earth masses)", "Temp: 288 K (15°C)"
- Missing values shown as dashes ("—") in dimmed text, not hidden

### Loading & Transition UX
- Stage-based status text through pipeline: "Querying NASA..." → "Running AI inference..." → "Mapping parameters..." → done
- Reuse existing `setExoplanetStatus()` mechanism (pass status updates from async thread)
- Fade transition: shrink current planet (radius → 0, atmosphere/clouds → 0), then fade new planet in
- Reuse the same lerp pattern from `runIntro()` which already does this
- Search input and Load button disabled (greyed out) while pipeline runs; existing `m_planetLoading` guard enforces this
- No special indicator on the rendered planet during loading — just status text in UI panel

### Claude's Discretion
- Exact ImGui popup styling for autocomplete dropdown
- Fade transition duration and easing curve
- Tooltip formatting and layout for AI reasoning
- How to thread-safely pass pipeline stage updates from async to main thread
- Whether to store ExoplanetData on the loaded planet for the info panel (likely add a member to Application)

</decisions>

<specifics>
## Specific Ideas

No specific requirements — open to standard approaches. All recommended options selected.

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `UIManager::m_searchBuf[256]` + `m_exoCallback`: Search box already exists with callback wiring
- `UIManager::setExoplanetStatus()`: Status text display already wired
- `Application::loadPlanet()`: Full async pipeline already works (NASA → AI → ExoplanetMapper → render)
- `Application::runIntro()`: Fade transition pattern (lerping radius/atmosphere/clouds) already proven
- `MeasuredValue<T>`: Tracks `DataSource`, `ai_reasoning`, and `confidence` per field — ready for provenance display
- `ExoplanetData`: Full data model with all fields needed for info panel
- `SolarSystemDatabase`: Provides known planet names for autocomplete seeding
- `ExoplanetMapper::classify()` + `categoryName()`: Planet classification for display
- `dataSourceToString()`: Helper to convert DataSource enum to display string

### Established Patterns
- ImGui immediate-mode rendering with glassmorphism styling (semi-transparent panels, rounded corners)
- `CollapsingHeader` sections for organizing UI groups (Presets, Planet, Terrain, etc.)
- Async pipeline via `std::async` + `std::future` with main-thread polling in `update()`
- `PlanetParams` struct as central state, directly modified by UI and pipeline
- Vulkan backend with ImGui_ImplVulkan integration

### Integration Points
- `UIManager::render()`: Add info panel section and autocomplete popup here
- `Application::update()`: Already polls `m_planetFuture` — add fade transition logic here
- `Application::loadPlanet()`: Update status messages at each pipeline stage
- Need to store `ExoplanetData` on Application for the info panel to read after load completes

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 03-search-render-integration*
*Context gathered: 2026-03-01*
