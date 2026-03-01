# Phase 5: Port Info Panel to tejui UIManager - Context

**Gathered:** 2026-03-01
**Status:** Ready for planning
**Source:** v1.0-MILESTONE-AUDIT.md gap closure

<domain>
## Phase Boundary

Port the Phase 3 Plan 2 info panel rendering code from master's UIManager.cpp into the current tejui-merged UIManager.cpp. The code exists and was tested — this is a targeted merge, not new feature development.

</domain>

<decisions>
## Implementation Decisions

### What to Port
- `getSourceColor()` helper: DataSource → ImVec4 color mapping (white/cyan/yellow)
- `renderMeasuredValue()` template: provenance-colored value display with AI tooltips
- `renderTemperature()` helper: Kelvin + Celsius display with provenance color
- `renderProvenanceLegend()`: compact color key (measured/AI/calculated)
- Info panel UI: CollapsingHeader "Planet Info" with Physical/Orbital/Host Star TreeNodes
- `m_exoData` consumption: read stored ExoplanetData pointer and render fields

### Where to Insert
- Helper functions go in anonymous namespace at top of UIManager.cpp (before presets)
- Info panel rendering goes inside the Planet Editor's render() method, after the back button and before the tab bar
- Must include `data/ExoplanetData.hpp` and `spdlog/fmt/fmt.h` headers

### Adaptation for tejui Layout
- tejui uses a tab bar (DATA/WORLD/VISUAL/LIGHT) — info panel should go in the DATA tab or above the tab bar
- Claude's discretion on exact placement within the tejui layout

</decisions>

<code_context>
## Source Code Reference

The exact code to port is on `master` branch at `src/ui/UIManager.cpp`:
- Lines 22-92: Helper functions (getSourceColor, renderMeasuredValue, renderTemperature, renderProvenanceLegend)
- Lines 482-531: Info panel rendering (Planet Info header, Physical/Orbital/Host Star subsections)

Current tejui UIManager.cpp already has:
- `#include "data/ExoplanetData.hpp"` header
- `m_exoData` member (const ExoplanetData* pointer)
- `setExoplanetData()` method that stores the pointer
- Application calls setExoplanetData() at lines 261 and 818

</code_context>

<deferred>
## Deferred Ideas

None — this is a targeted gap closure

</deferred>
