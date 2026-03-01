# Retrospective

## Milestone: v1.0 — Real Exoplanet Data + AI Inference Pipeline

**Shipped:** 2026-03-01
**Phases:** 4 | **Plans:** 12

### What Was Built
- 4-source data fusion pipeline (NASA TAP, OEC, Gaia DR3, CDS/VizieR) with uncertainty-based selection
- AI inference via AWS Bedrock/Claude filling missing exoplanet parameters
- Physics-based ExoplanetData → PlanetParams mapper for procedural rendering
- Search with autocomplete, pipeline status, and provenance color-coding
- Pokedex-style catalogue of 500+ exoplanets with card grid, sorting, filtering
- Offscreen Vulkan FBO for progressive thumbnail generation with PNG disk cache

### What Worked
- Wave-based parallel plan execution (Plans 04-01 and 04-02 ran simultaneously)
- Phase-level verification caught real issues before they compounded
- OEC CSV bulk fetch + NASA merge gave 5000+ planets in seconds vs 500 individual API calls
- MeasuredValue<T> provenance tracking pattern scaled well across all 4 data sources
- Atomic task commits made rollback and debugging straightforward

### What Was Inefficient
- tejui branch merge overwrote Phase 3 Plan 2 info panel code — required re-porting
- NASA TAP ADQL LOWER() incompatibility wasn't caught until runtime (400 errors)
- Build cache invalidation across environments (local vs cloud) caused repeated cmake reconfigures
- Autocomplete z-order bug required 3 iterations to fix (hover tracking → persistent state → manual position check)

### Patterns Established
- **Manual mouse position checking** for ImGui z-order issues (bypasses IsWindowHovered limitations)
- **OEC CSV bulk + NASA TAP merge** pattern for catalogue-scale data loading
- **Offscreen FBO reuse** — single 128px framebuffer rendered serially for all thumbnails
- **Progressive thumbnail queue** — one render per frame, visible cards prioritized
- **Auto-detect stale cmake cache** in Makefile for cross-environment builds

### Key Lessons
- Merge UI branches early and verify integration — late merges overwrite functional code
- Test API queries against real endpoints before committing (ADQL LOWER() failure)
- ImGui separate windows need explicit z-order management — prefer same-window rendering when possible
- Physics-based defaults (SolarSystemDatabase analog matching) are better fallbacks than AI inference for rendering

### Cost Observations
- Model mix: ~70% Opus (execution), ~25% Sonnet (research, verification, integration check), ~5% Haiku (exploration)
- Sessions: ~6 across 2 days
- Notable: Parallel plan execution in Phase 4 saved ~30 minutes vs sequential

---

## Cross-Milestone Trends

| Metric | v1.0 |
|--------|------|
| Phases | 4 |
| Plans | 12 |
| LOC | ~21,000 |
| Tests | 538 assertions |
| Timeline | 2 days |
| Tech debt items | 3 |
