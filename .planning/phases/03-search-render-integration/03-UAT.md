---
status: testing
phase: 03-search-render-integration
source: 03-01-SUMMARY.md, 03-02-SUMMARY.md
started: 2026-03-01T01:00:00Z
updated: 2026-03-01T02:00:00Z
---

## Current Test
<!-- OVERWRITE each test - shows where we are -->

number: 3
name: Input Disabled During Loading
expected: |
  While a planet is loading, the search input and Load button are greyed out / disabled and cannot be interacted with until the pipeline completes.
awaiting: user response

## Tests

### 1. Search Autocomplete Dropdown
expected: Type a partial planet name into the search input. A dropdown appears below the input showing up to 8 matching planet names with prefix matching.
result: fixed
reported: "Clicking dropdown item did not fill search bar. Label said NASA but uses multiple sources."
fix: "Replaced frame-level hover tracking with persistent m_acOpen state + SetWindowFocus() for click routing. Label changed to 'Search Exoplanet Catalogs'."
commit: a00b8cc

### 2. Pipeline Status Display
expected: After selecting/entering a planet and triggering load, real-time status text appears showing pipeline stages like "Querying data sources...", "Running AI inference...", "Mapping parameters..." as the pipeline progresses.
result: pass
notes: "Pipeline completes too fast for stages to be individually readable, but status text does appear. User accepted this as positive ('which is good I guess')."

### 3. Input Disabled During Loading
expected: While a planet is loading, the search input and Load button are greyed out / disabled and cannot be interacted with until the pipeline completes.
result: [pending]

### 4. Planet Info Panel with Subsections
expected: After a planet loads successfully, an info panel appears with three collapsible subsections: Physical, Orbital, and Host Star, each showing key facts with friendly units.
result: [pending]

### 5. Data Provenance Color-Coding
expected: Values in the planet info panel are color-coded by source: white for measured data, cyan for AI-inferred, yellow for calculated. A provenance legend is visible at the top of the info panel.
result: [pending]

### 6. AI-Inferred Value Tooltips
expected: Hovering over a cyan (AI-inferred) value in the info panel shows a tooltip with the data source, confidence percentage, and reasoning text.
result: [pending]

### 7. Fade Transition Between Planets
expected: When loading a new planet while one is already displayed, the current planet shrinks out and the new one grows in with a smooth fade transition (~0.67s total).
result: [pending]

### 8. Growing Autocomplete Suggestions
expected: After successfully searching for a planet not previously in the autocomplete list, that planet name now appears in future autocomplete suggestions when typing.
result: [pending]

## Summary

total: 8
passed: 1
fixed: 1
issues: 0
pending: 6
skipped: 0

## Gaps

- truth: "Clicking an autocomplete suggestion fills the search bar and triggers load"
  status: fixed
  reason: "User reported: clicking dropdown item does not overwrite search bar. Also label says NASA but uses multiple sources."
  severity: major
  test: 1
  root_cause: "showAutocomplete tied to inputActive — clicking Selectable deactivates InputText first so autocomplete closes before click lands. Label text still said 'NASA Exoplanet Archive'."
  fix: "Persistent m_acOpen state + SetWindowFocus() when hovered. Label updated to 'Search Exoplanet Catalogs'."
  commit: a00b8cc

- truth: "Earth preset should match search-bar Earth appearance"
  status: fixed
  reason: "User reported: Earth from search bar looks different from preset Earth."
  severity: minor
  test: 2
  root_cause: "makePreset(0) only set continentScale=1.0f on PlanetParams{} defaults (no water, no polar caps). SolarSystemDatabase::makeEarth() has full hand-tuned Earth values."
  fix: "Copied makeEarth() values into makePreset(0) case so both paths produce identical Earth."
  commit: a00b8cc
