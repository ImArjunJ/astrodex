# Phase 3: Search & Render Integration - Research

**Researched:** 2026-03-01
**Domain:** ImGui UI integration, async pipeline orchestration, data provenance display
**Confidence:** HIGH

## Summary

Phase 3 connects the existing data+inference pipeline to the renderer via enhanced UI. The foundational infrastructure is solid: `Application::loadPlanet()` already executes the full async pipeline (NASA query, AI inference, ExoplanetMapper conversion, render), `UIManager` already has a search box with callback wiring, and `MeasuredValue<T>` already tracks `DataSource`, `ai_reasoning`, and `confidence` per field. The work is primarily ImGui UI development and pipeline orchestration refinements.

The key technical challenges are: (1) building a responsive autocomplete popup in ImGui's immediate-mode paradigm, (2) threading stage-based status updates from the async pipeline back to the main thread safely, (3) rendering a data provenance display that color-codes values by `DataSource` with hover tooltips, and (4) implementing a smooth fade transition between planets using the proven `runIntro()` lerp pattern. All data structures and plumbing already exist; this phase adds the UI presentation layer on top.

**Primary recommendation:** Build incrementally -- autocomplete first, then pipeline status updates, then planet info panel with provenance, then fade transition. Each piece can be tested independently.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **Search & Autocomplete:** Dropdown below search input showing matching planet names. Planet name list sourced from local cache only (NasaApiClient cache + SolarSystemDatabase entries); grows as users search more planets. Prefix matching for filtering. Show 5-8 suggestions at a time, scrollable if more matches exist. Reuse existing `m_searchBuf` in UIManager; add autocomplete popup via ImGui's popup/listbox system.
- **Data Provenance Display:** Color-code values inline in the planet info panel by DataSource: White = measured (NASA_TAP, GAIA, CDS_VIZIER, OEC), Cyan = AI-inferred (AI_INFERRED), Yellow = calculated (CALCULATED). Hover tooltip on AI-inferred (cyan) values showing AI reasoning text and confidence percentage. Small one-line legend at top of info panel. Confidence levels shown in tooltip alongside reasoning.
- **Planet Info Panel:** New collapsible section inside existing Planet Editor window, below Exoplanet Lookup. Only appears after a planet is loaded. Three collapsible subsections: Physical, Orbital, Host Star. Discovery info shown inline with planet name header. Friendly units with context. Missing values shown as dashes in dimmed text.
- **Loading & Transition UX:** Stage-based status text: "Querying NASA..." -> "Running AI inference..." -> "Mapping parameters..." -> done. Reuse existing `setExoplanetStatus()` mechanism. Fade transition: shrink current planet (radius -> 0, atmosphere/clouds -> 0), then fade new planet in. Reuse lerp pattern from `runIntro()`. Search input and Load button disabled while pipeline runs.

### Claude's Discretion
- Exact ImGui popup styling for autocomplete dropdown
- Fade transition duration and easing curve
- Tooltip formatting and layout for AI reasoning
- How to thread-safely pass pipeline stage updates from async to main thread
- Whether to store ExoplanetData on the loaded planet for the info panel (likely add a member to Application)

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| R3.1 | Search Interface: ImGui text input with autocomplete from local cache, trigger full pipeline, display loading state | Autocomplete pattern via `ImGui::BeginPopup`/`ImGui::Selectable`, prefix filtering from `SolarSystemDatabase::entries()` + `CacheManager::listCached()`, existing `m_planetLoading` guard + `setExoplanetStatus()` |
| R3.2 | Data Provenance Display: Color-coding by DataSource, expandable AI reasoning panel, confidence indicators | `MeasuredValue<T>` already stores `source`, `ai_reasoning`, `confidence`; `ImGui::TextColored()` for source-based coloring; `ImGui::IsItemHovered()` + `ImGui::BeginTooltip()` for reasoning display |
| R3.3 | Planet Info Panel: Key facts display (mass, radius, temp, period), host star info, discovery info | `ExoplanetData` struct contains all needed fields; new collapsible sections via `ImGui::CollapsingHeader`; need to store `ExoplanetData` on `Application` after pipeline completes |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Dear ImGui | 1.91.6 | Immediate-mode GUI framework | Already integrated via Vulkan backend; all UI built with it |
| glm | (bundled) | Vec3/math types for color values | Already used throughout renderer |
| nlohmann/json | (bundled) | JSON parsing for cache files | Already used for ExoplanetData serialization |
| spdlog/fmt | (bundled) | Logging and string formatting | Already used project-wide; use `fmt::format` not `std::format` (GCC 12.2 lacks `<format>`) |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| std::async/std::future | C++23 | Async pipeline execution | Already used in `loadPlanet()` for background work |
| std::atomic | C++23 | Thread-safe status message passing | For async-to-main-thread stage updates |
| std::filesystem | C++17 | Cache directory scanning | Already used in `CacheManager::listCached()` |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `ImGui::BeginPopup` | `ImGui::BeginCombo` | Combo is simpler but less flexible; popup gives exact positioning control needed for autocomplete below input |
| `std::atomic<std::string>` | `std::mutex` + `std::string` | `std::atomic<std::string>` does not exist in standard C++; use `std::atomic<int>` for stage enum and string array lookup |
| Storing ExoplanetData* | Storing ExoplanetData copy | Copy is safer (no lifetime issues with async result); ExoplanetData is not huge |

## Architecture Patterns

### Recommended Changes to Existing Files

```
src/
├── core/
│   ├── Application.hpp     # + ExoplanetData m_loadedPlanet; + pipeline stage enum/atomic
│   └── Application.cpp     # + fade transition in update(); + stage updates in loadPlanet()
├── ui/
│   ├── UIManager.hpp       # + autocomplete state; + ExoplanetData const* for info panel
│   └── UIManager.cpp       # + autocomplete popup; + info panel section; + provenance coloring
└── data/
    └── (no changes needed — CacheManager::listCached() and SolarSystemDatabase already exist)
```

### Pattern 1: ImGui Autocomplete Popup
**What:** A popup window positioned directly below the search input that shows filtered planet name matches.
**When to use:** When the user types into the search box and matching names exist.
**Implementation approach:**

```cpp
// In UIManager::render(), after the InputText:
ImGui::InputText("##planet", m_searchBuf, sizeof(m_searchBuf), ...);

// Track if input is active and has text
bool inputActive = ImGui::IsItemActive();
bool hasText = m_searchBuf[0] != '\0';

if (inputActive && hasText) {
    // Position popup just below the input
    ImVec2 inputPos = ImGui::GetItemRectMin();
    ImVec2 inputSize = ImGui::GetItemRectSize();
    ImGui::SetNextWindowPos(ImVec2(inputPos.x, inputPos.y + inputSize.y));
    ImGui::SetNextWindowSize(ImVec2(inputSize.x, 0)); // auto-height

    // Use BeginPopup with ChildWindow flags for non-modal overlay
    if (ImGui::Begin("##autocomplete", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing)) {
        int shown = 0;
        for (const auto& name : m_cachedNames) {
            // Prefix match (case-insensitive)
            if (matchesPrefix(name, m_searchBuf) && shown < 8) {
                if (ImGui::Selectable(name.c_str())) {
                    strncpy(m_searchBuf, name.c_str(), sizeof(m_searchBuf) - 1);
                    // Trigger load
                }
                shown++;
            }
        }
    }
    ImGui::End();
}
```

**Key insight:** ImGui 1.91.6 does not have a built-in autocomplete widget. The standard approach is to use a separate `ImGui::Begin` window (not a popup, since popups close on focus loss from the input) positioned below the input. This window acts as a dropdown overlay.

### Pattern 2: Thread-Safe Pipeline Stage Updates
**What:** Communicate current pipeline stage from async thread to main thread for status display.
**When to use:** During the `loadPlanet()` async pipeline execution.
**Implementation approach:**

```cpp
// In Application.hpp:
enum class PipelineStage : int {
    Idle = 0,
    QueryingNasa,
    RunningAI,
    MappingParams,
    Done,
    Failed
};

static constexpr const char* stageMessages[] = {
    "",
    "Querying NASA...",
    "Running AI inference...",
    "Mapping parameters...",
    "Done",
    "Error"
};

std::atomic<int> m_pipelineStage{0};

// In loadPlanet() async lambda:
m_pipelineStage.store(static_cast<int>(PipelineStage::QueryingNasa));
auto results = m_nasa->queryByNameSync(name);
m_pipelineStage.store(static_cast<int>(PipelineStage::RunningAI));
data = m_inference->fillMissingParametersSync(std::move(data));
m_pipelineStage.store(static_cast<int>(PipelineStage::MappingParams));
// ...

// In update():
if (m_planetLoading) {
    int stage = m_pipelineStage.load();
    m_ui->setExoplanetStatus(stageMessages[stage]);
}
```

**Key insight:** Use `std::atomic<int>` (not `std::atomic<std::string>`, which is not trivially copyable). Map the int to a string array on the main thread. This avoids any mutex overhead and is safe for the single-writer-single-reader pattern.

### Pattern 3: Data Provenance Color-Coding
**What:** Color each value in the info panel based on its `DataSource` enum.
**When to use:** For every `MeasuredValue<T>` displayed in the planet info panel.
**Implementation approach:**

```cpp
// Helper function for provenance-colored text display
ImVec4 getSourceColor(DataSource source) {
    switch (source) {
        case DataSource::AI_INFERRED:
            return ImVec4(0.0f, 0.9f, 0.9f, 1.0f);  // Cyan
        case DataSource::CALCULATED:
            return ImVec4(0.9f, 0.9f, 0.0f, 1.0f);  // Yellow
        default:
            return ImVec4(0.96f, 0.97f, 1.0f, 1.0f); // White (measured)
    }
}

template<typename T>
void renderMeasuredValue(const char* label, const MeasuredValue<T>& val,
                         const char* unit, const char* altUnit = nullptr) {
    if (!val.hasValue()) {
        ImGui::TextDisabled("%s: ---", label);
        return;
    }
    ImGui::TextColored(getSourceColor(val.source), "%s: %.2f %s", label, val.value, unit);

    // Hover tooltip for AI-inferred values
    if (val.isAIInferred() && ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Source: AI-Inferred");
        ImGui::Text("Confidence: %.0f%%", val.confidence * 100.0f);
        if (val.ai_reasoning) {
            ImGui::Separator();
            ImGui::TextWrapped("%s", val.ai_reasoning->c_str());
        }
        ImGui::EndTooltip();
    }
}
```

### Pattern 4: Fade Transition Between Planets
**What:** Animate old planet out and new planet in during planet loading.
**When to use:** When a new planet load completes.
**Implementation approach:**

```cpp
// In Application — new members:
PlanetParams m_targetParams;     // Params to transition TO
bool m_transitioning = false;
float m_transitionAlpha = 1.0f;  // 1 = fully showing current planet
bool m_transitionShrinking = true; // true = shrink old, false = grow new

// In update(), after planet load result is ready:
if (params.has_value()) {
    m_targetParams = *params;
    m_transitioning = true;
    m_transitionShrinking = true;
    m_transitionAlpha = 1.0f;
}

// In update(), transition logic:
if (m_transitioning) {
    float speed = 3.0f; // ~0.33s per phase
    if (m_transitionShrinking) {
        m_transitionAlpha -= deltaTime * speed;
        if (m_transitionAlpha <= 0.0f) {
            // Switch to new planet params
            m_renderer->params() = m_targetParams;
            m_transitionShrinking = false;
            m_transitionAlpha = 0.0f;
        }
    } else {
        m_transitionAlpha += deltaTime * speed;
        if (m_transitionAlpha >= 1.0f) {
            m_transitionAlpha = 1.0f;
            m_transitioning = false;
        }
    }
    // Apply fade: lerp radius/atmosphere/clouds toward 0 or target
    auto& rp = m_renderer->params();
    float t = m_transitionAlpha;
    rp.radius = rp.radius * t; // or lerp with saved base values
    rp.atmosphereDensity = rp.atmosphereDensity * t;
    rp.cloudsDensity = rp.cloudsDensity * t;
}
```

**Key insight:** The exact pattern from `runIntro()` already works -- it lerps `radius`, `atmosphereDensity`, and `cloudsDensity` by a 0-1 alpha. The transition needs two phases: shrink old (alpha 1->0), then grow new (alpha 0->1). Store the target params separately so the renderer shows the old planet while shrinking.

### Anti-Patterns to Avoid
- **Blocking the main thread:** Never call synchronous network/AI functions on the main thread. The existing `std::async` pattern is correct; keep it.
- **Modifying PlanetParams from background thread:** Always stage results and apply on the main thread in `update()`. The current pattern already does this correctly.
- **Popup focus stealing:** Using `ImGui::OpenPopup()` for autocomplete will steal focus from the InputText. Use a separate `ImGui::Begin` window instead (see Pattern 1).
- **Rebuilding name list every frame:** Cache the sorted name list and only rebuild when the cache changes (after a successful planet load or on startup).

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| String prefix matching | Custom trie/lookup | Simple `std::string::find(prefix, 0) == 0` with `tolower` | Name list is <1000 entries; linear scan is sub-millisecond |
| Thread-safe string passing | Custom lock-free queue | `std::atomic<int>` stage enum + string array | Single-writer-single-reader, trivial pattern |
| Temperature unit conversion | Custom converter | Inline `kelvin - 273.15` | Trivial arithmetic, not worth a utility class |
| ImGui autocomplete widget | Custom widget class | Inline ImGui calls in `render()` | ImGui's immediate-mode style means the code IS the widget; wrapping it adds indirection |
| Cache name list aggregation | Custom database query | `SolarSystemDatabase::entries()` + `CacheManager::listCached()` | Both APIs already return exactly what we need |

**Key insight:** This phase is almost entirely UI wiring. The hard parts (data fetching, AI inference, physics mapping) are already done. Resist the urge to over-engineer the UI layer.

## Common Pitfalls

### Pitfall 1: ImGui Popup Focus Management
**What goes wrong:** Using `ImGui::OpenPopup`/`ImGui::BeginPopup` for autocomplete causes the input text to lose focus, closing the popup immediately. The user can never type while seeing suggestions.
**Why it happens:** ImGui popups are designed to be modal-ish -- they take focus and close when you click outside them.
**How to avoid:** Use a regular `ImGui::Begin` window with `NoFocusOnAppearing` and `NoTitleBar` flags instead of the popup system. Position it manually below the input using `ImGui::GetItemRectMin()`/`ImGui::GetItemRectSize()`.
**Warning signs:** Autocomplete flickers or never appears while typing.

### Pitfall 2: Race Condition on Status Message
**What goes wrong:** The async thread writes a string to `m_exoStatus` while the main thread reads it for display, causing data corruption.
**Why it happens:** `std::string` is not thread-safe for concurrent read/write.
**How to avoid:** Use `std::atomic<int>` for pipeline stage (integer is atomic by nature). Map to display string on the main thread only. Never pass `std::string` across threads without synchronization.
**Warning signs:** Garbled status text, intermittent crashes on planet load.

### Pitfall 3: ExoplanetData Lifetime for Info Panel
**What goes wrong:** The info panel reads from `ExoplanetData*` that was part of a `LoadResult` temporary, causing a dangling pointer.
**Why it happens:** The `std::future::get()` returns a temporary; references to it become invalid.
**How to avoid:** Store a copy of `ExoplanetData` as a member of `Application` (e.g., `std::optional<ExoplanetData> m_loadedExoData`). Copy it when the pipeline completes. Pass a `const ExoplanetData*` to UIManager for display.
**Warning signs:** Info panel shows garbage values or crashes on second planet load.

### Pitfall 4: Autocomplete Name List Not Growing
**What goes wrong:** The autocomplete list only shows solar system planets and never adds newly searched exoplanets.
**Why it happens:** The name list is built once at startup from `SolarSystemDatabase` but never refreshed after successful pipeline loads.
**How to avoid:** After each successful `loadPlanet()` completion, add the planet name to the cached names list. Either rebuild from `CacheManager::listCached()` or maintain a `std::set<std::string>` in Application that grows.
**Warning signs:** Searching "Kepler-442b" once works, but it never appears in autocomplete afterwards.

### Pitfall 5: Fade Transition Clobbering New Params
**What goes wrong:** The fade-in animation applies alpha to the wrong base values, resulting in a planet that never reaches full size.
**Why it happens:** Saving `m_renderer->params()` captures the already-alpha-modified values instead of the target values.
**How to avoid:** Store the target `PlanetParams` separately (e.g., `m_targetParams`). During fade-in, compute display params as `target * alpha`, never modify `m_targetParams` itself. Apply to `m_renderer->params()` each frame.
**Warning signs:** Planet radius is always smaller than expected, or atmosphere vanishes permanently.

### Pitfall 6: ImGui Window Ordering Z-Fight
**What goes wrong:** The autocomplete dropdown appears behind the Planet Editor window or other UI elements.
**Why it happens:** ImGui renders windows in order. If the autocomplete window is created before other windows in the frame, it may be drawn behind them.
**How to avoid:** Use `ImGui::SetNextWindowFocus()` before the autocomplete window, or render it after all other windows. In ImGui 1.91+, `ImGuiWindowFlags_NoBringToFrontOnFocus` should NOT be used on the autocomplete -- it should come to front.
**Warning signs:** Autocomplete suggestions are invisible or partially obscured.

## Code Examples

### Autocomplete Name Collection (Startup)
```cpp
// In Application::init() or a dedicated method:
void Application::buildPlanetNameList() {
    m_knownNames.clear();

    // Seed from solar system database
    for (const auto& entry : SolarSystemDatabase::instance().entries()) {
        m_knownNames.insert(entry.name);
    }

    // Add cached exoplanet names (from CacheManager fused cache)
    // Note: CacheManager::listCached() returns lowercase with underscores
    // Need original names from cached JSON, or store a name index
    auto cached = m_cacheManager->listCached();
    for (const auto& name : cached) {
        m_knownNames.insert(name);
    }
}
```

### Provenance Legend
```cpp
// One-line legend at top of info panel
void renderProvenanceLegend() {
    ImGui::TextColored(ImVec4(0.96f, 0.97f, 1.0f, 1.0f), "●"); ImGui::SameLine();
    ImGui::TextDisabled("Measured"); ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.0f, 0.9f, 0.9f, 1.0f), "●"); ImGui::SameLine();
    ImGui::TextDisabled("AI-Inferred"); ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.0f, 1.0f), "●"); ImGui::SameLine();
    ImGui::TextDisabled("Calculated");
}
```

### Disable Input During Loading
```cpp
// In UIManager::render() Exoplanet Lookup section:
bool isLoading = /* passed via setter or member flag */;
if (isLoading) ImGui::BeginDisabled();

ImGui::SetNextItemWidth(-80);
bool hitEnter = ImGui::InputText("##planet", m_searchBuf, sizeof(m_searchBuf),
                                 ImGuiInputTextFlags_EnterReturnsTrue);
ImGui::SameLine();
bool clicked = ImGui::Button("Load");

if (isLoading) ImGui::EndDisabled();
```

### Kelvin to Celsius Helper
```cpp
// For friendly temperature display: "288 K (15°C)"
void renderTemperature(const char* label, const MeasuredValue<double>& val) {
    if (!val.hasValue()) {
        ImGui::TextDisabled("%s: ---", label);
        return;
    }
    double celsius = val.value - 273.15;
    ImGui::TextColored(getSourceColor(val.source), "%s: %.0f K (%.0f°C)",
                       label, val.value, celsius);
    // Tooltip for AI-inferred...
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| ImGui::OpenPopup for dropdowns | ImGui::Begin with manual positioning | Always (popup = modal) | Autocomplete requires non-modal overlay |
| `std::atomic<std::string>` | `std::atomic<int>` + lookup table | N/A (string was never atomic) | Thread-safe stage communication |
| `ImGui::BeginDisabled()` | Same (stable since 1.87) | ImGui 1.87+ | Greying out controls during loading |
| Manual color push/pop | `ImGui::TextColored()` | Always available | Simpler API for per-text coloring |

**Deprecated/outdated:**
- Nothing deprecated applies. ImGui 1.91.6 is current. All APIs used are stable.

## Open Questions

1. **CacheManager name casing**
   - What we know: `CacheManager::listCached()` returns names derived from filenames (lowercase, underscores). The original casing is lost.
   - What's unclear: Whether the original planet name (proper casing) can be recovered from the cached JSON content, or whether we need to store a separate name index.
   - Recommendation: Read the "data.name" field from each cached JSON file to get the original name. This is an O(N) directory scan at startup but N is small (<100 initially). Alternatively, store a simple `names.json` index file alongside the cache.

2. **DataFusionEngine usage in loadPlanet**
   - What we know: `Application::loadPlanet()` currently uses `m_nasa` directly (NasaApiClient only, no multi-source fusion). `DataFusionEngine` exists but is not wired into the application.
   - What's unclear: Whether Phase 3 should upgrade `loadPlanet()` to use `DataFusionEngine::fetchAndFuseSync()` (which queries all 4 sources), or keep the current NASA-only flow.
   - Recommendation: Keep using `m_nasa` directly for Phase 3 (matches current working pipeline). DataFusionEngine integration could be a Phase 4 catalogue concern. The user decisions do not mention multi-source fusion for this phase.

3. **Autocomplete popup z-ordering across ImGui windows**
   - What we know: The autocomplete dropdown must appear on top of the Planet Editor window's content below it.
   - What's unclear: Whether ImGui's default window ordering handles this correctly when both windows exist.
   - Recommendation: Render the autocomplete window at the very end of the `render()` call (after all other sections) so it gets the highest z-order. Test and adjust if needed.

## Sources

### Primary (HIGH confidence)
- **Codebase inspection** -- `UIManager.hpp/cpp`, `Application.hpp/cpp`, `ExoplanetData.hpp`, `PlanetParams.hpp` (IRenderer.hpp), `SolarSystemDatabase.hpp/cpp`, `CacheManager.hpp/cpp`, `NasaApiClient.hpp/cpp`, `ExoplanetMapper.hpp`, `InferenceEngine.hpp`
- **ImGui 1.91.6** -- Confirmed version from `build/_deps/imgui-src/imgui.h`, `IMGUI_VERSION "1.91.6"`
- **ImGui API** -- `BeginDisabled()`/`EndDisabled()` (stable since 1.87), `TextColored()`, `BeginTooltip()`/`EndTooltip()`, `IsItemHovered()`, `Selectable()`, `GetItemRectMin()`/`GetItemRectSize()` -- all part of core ImGui API and used elsewhere in the codebase

### Secondary (MEDIUM confidence)
- **ImGui autocomplete pattern** -- The non-popup approach (using `ImGui::Begin` with manual positioning) is the established community pattern for autocomplete. This is based on multiple ImGui issue discussions and the fact that `ImGui::BeginPopup` is documented as closing on external click.

### Tertiary (LOW confidence)
- None -- all findings verified against codebase and ImGui API.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- all libraries already in use; no new dependencies needed
- Architecture: HIGH -- patterns verified against existing codebase (`runIntro()` fade, `loadPlanet()` async, `setExoplanetStatus()` wiring)
- Pitfalls: HIGH -- identified from direct code reading (thread safety, ImGui focus model, lifetime management)

**Research date:** 2026-03-01
**Valid until:** 2026-03-31 (stable -- ImGui API doesn't change frequently)
