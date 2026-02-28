---
phase: 01-data-aggregation-layer
plan: 04
subsystem: data
tags: [data-fusion, cross-matching, cache, haversine, uncertainty-based-selection, json-storage]

# Dependency graph
requires:
  - phase: 01-01
    provides: NASA TAP client, ExoplanetData model, build infrastructure
  - phase: 01-02
    provides: OEC client with XML parsing
  - phase: 01-03
    provides: Gaia DR3 and CDS/VizieR TAP clients
provides:
  - CoordinateMatcher: name-first matching with 5-arcsec coordinate fallback, haversine distance calculation
  - DataFusionEngine: uncertainty-based measurement selection, source priority fallback (NASA > Gaia > CDS > OEC)
  - CacheManager: JSON storage with TTL-based expiration, offline access support
  - Complete multi-source data fusion: fetchAndFuseSync orchestrates all 4 clients
affects: [02-ai-inference, 03-rendering, search-interface]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Uncertainty-based data selection: prefer measurements with lower uncertainty"
    - "Source priority fallback: NASA (1) > Gaia (2) > CDS (3) > OEC (4) when uncertainties equal"
    - "Name-first matching with coordinate fallback: try nameMatch(), fall back to angularDistance() within 5 arcsec"
    - "Haversine formula for great-circle distance: handles RA wrapping at 0/360, polar edge cases"
    - "JSON cache with metadata: store cached_at timestamp and ttl_days for expiration checks"
    - "Pimpl idiom for client composition: DataFusionEngine::Impl owns all 4 clients + CacheManager"

key-files:
  created:
    - src/data/CoordinateMatcher.hpp
    - src/data/CoordinateMatcher.cpp
    - src/data/DataFusionEngine.hpp
    - src/data/DataFusionEngine.cpp
    - src/data/CacheManager.hpp
    - src/data/CacheManager.cpp
    - tests/data/test_data_fusion.cpp
  modified:
    - CMakeLists.txt
    - tests/CMakeLists.txt

key-decisions:
  - "Cross-matching: name-first with 5-arcsec coordinate fallback (per plan user decision)"
  - "Data fusion: uncertainty-based selection with source priority fallback NASA > Gaia > CDS > OEC"
  - "Cache TTL: 30 days default (fused records are stable, sources update slowly)"
  - "Haversine formula: handles RA 0/360 wrapping and polar coordinates correctly"
  - "Provenance tracking: every MeasuredValue retains source, uncertainty, confidence"

patterns-established:
  - "MeasuredValue merging: collect candidates from all sources, selectBestMeasurement() by uncertainty + priority"
  - "Star name sanitization: lowercase, replace spaces with underscores, remove non-alphanumeric for filesystem safety"
  - "Cache expiration: compare file mtime against TTL, automatic cleanup on retrieval"
  - "TDD for fusion logic: write tests first (RED), implement (GREEN), all 242 assertions pass"

requirements-completed: [R1.5]

# Metrics
duration: 3min
completed: 2026-02-28
---

# Phase 01 Plan 04: Data Fusion Engine Summary

**Multi-source data fusion with uncertainty-based selection, haversine coordinate matching, and JSON cache with 30-day TTL**

## Performance

- **Duration:** 3 minutes
- **Started:** 2026-02-28T18:08:51Z
- **Completed:** 2026-02-28T18:12:39Z
- **Tasks:** 3 (Task 1 completed in prior session)
- **Files modified:** 9

## Accomplishments
- DataFusionEngine orchestrates all 4 clients (NASA, OEC, Gaia, CDS) with uncertainty-based measurement selection
- Source priority fallback NASA > Gaia > CDS > OEC when uncertainties equal or missing
- CacheManager stores fused ExoplanetData as JSON with TTL-based expiration (30 days default)
- Complete data aggregation layer: 242 assertions in 26 test cases pass (all plans 01-01 through 01-04)

## Task Commits

Each task was committed atomically:

1. **Task 1: CoordinateMatcher** - `7065385` (feat) — [Completed in prior session]
2. **Task 2: DataFusionEngine** - `4775093` (test), `c33dcb6` (feat) — TDD: RED → GREEN
3. **Task 3: CacheManager integration** - `d042448` (feat)

**Plan metadata:** (to be committed)

_Note: Task 2 used TDD with separate test and implementation commits_

## Files Created/Modified

**Created:**
- `src/data/DataFusionEngine.hpp` - Multi-source fusion engine with uncertainty-based selection
- `src/data/DataFusionEngine.cpp` - Orchestrates 4 clients, merges HostStarData and ExoplanetData
- `src/data/CacheManager.hpp` - JSON cache with TTL expiration
- `src/data/CacheManager.cpp` - Store/retrieve fused records, list/clear operations
- `tests/data/test_data_fusion.cpp` - 6 test cases for fusion logic (38 assertions)

**Modified:**
- `CMakeLists.txt` - Added DataFusionEngine.cpp and CacheManager.cpp to ASTROCORE_SOURCES
- `tests/CMakeLists.txt` - Added test_data_fusion.cpp to test target

## Decisions Made

**1. Uncertainty-based selection with source priority fallback**
- Prefer measurements with lower uncertainty
- Tie-break by source priority: NASA (1) > Gaia (2) > CDS/ExoAtmos (3) > OEC (4) > CALCULATED (5) > AI_INFERRED (6)
- If one value has uncertainty and another doesn't, prefer the one with uncertainty (explicit confidence signal)

**2. Cache structure with metadata**
- Store cached_at timestamp and ttl_days in metadata object
- Planet data stored in separate "data" field parsed via ExoplanetData::fromJson()
- TTL checked on retrieval: compare file mtime against configured expiration window

**3. Star name sanitization**
- Lowercase + replace spaces with underscores for cache filenames
- Strip non-alphanumeric characters except hyphens and underscores
- Reversible transformation for listCached() display

**4. TDD for fusion logic**
- Wrote 6 test cases first (RED phase): selectBestMeasurement, source priority, mergeHostStarData, mergeExoplanetData, provenance tracking
- Implemented DataFusionEngine to pass all tests (GREEN phase)
- No refactor needed: implementation clean on first pass

## Deviations from Plan

None - plan executed exactly as written. Task 1 (CoordinateMatcher) was completed in prior session (commit 7065385). This session implemented Tasks 2-3 (DataFusionEngine, CacheManager) per plan specification.

## Issues Encountered

**Template macro type deduction**
- Initial MERGE_FIELD macro used `decltype(merged.field_name)::value_type` which failed for MeasuredValue<T> (no value_type typedef)
- Fixed by using `decltype(merged.field_name)` directly (MeasuredValue<T> is the candidate type)
- Compile succeeded after fix, all tests passed

## User Setup Required

None - no external service configuration required. Cache uses local filesystem (`.cache/fused/`), automatically created on CacheManager construction.

## Next Phase Readiness

**Phase 02 (AI Inference Pipeline) is ready:**
- DataFusionEngine::fetchAndFuseSync() provides complete multi-source ExoplanetData records
- All fields have DataSource provenance tracking for visualization color-coding
- CacheManager enables offline access to 500 pre-fetched notable exoplanets (via prefetchNotable())
- Test coverage: 242 assertions across all data layer components

**Phase 03 (Rendering) is ready:**
- Fused records include all physical parameters for rendering (mass, radius, temperature, orbital elements)
- Host star data enriched with Gaia astrometry + CDS stellar parameters
- calculateDerivedValues() computes dependent quantities automatically

**Blockers:** None

**Concerns:** None

## Self-Check: PASSED

**Files verified:**
- FOUND: CoordinateMatcher.hpp
- FOUND: CoordinateMatcher.cpp
- FOUND: DataFusionEngine.hpp
- FOUND: DataFusionEngine.cpp
- FOUND: CacheManager.hpp
- FOUND: CacheManager.cpp
- FOUND: test_data_fusion.cpp

**Commits verified:**
- FOUND: 7065385 (CoordinateMatcher)
- FOUND: 4775093 (test - DataFusionEngine RED)
- FOUND: c33dcb6 (feat - DataFusionEngine GREEN)
- FOUND: d042448 (CacheManager)

**Test suite:**
- All tests passed (242 assertions in 26 test cases)

---
*Phase: 01-data-aggregation-layer*
*Completed: 2026-02-28*
