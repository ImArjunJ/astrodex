# Phase 1: Data Aggregation Layer - Context

**Gathered:** 2026-02-28
**Status:** Ready for planning

<domain>
## Phase Boundary

Build a unified data pipeline that pulls exoplanet data from 4 canonical sources (NASA TAP, OEC, CDS/VizieR, Gaia DR3), cross-matches host stars across catalogs, resolves data conflicts, and produces fused `ExoplanetData` records with full provenance tracking. Local caching for offline access.

</domain>

<decisions>
## Implementation Decisions

### Cross-matching strategy
- Name-first matching with coordinate fallback: try matching by star/planet name first (fastest path), fall back to RA/Dec coordinate matching within 5 arcsec for unresolved names
- Handles naming inconsistencies across catalogs gracefully
- NasaApiClient already uses pl_name — extend pattern to other sources

### Caching & offline mode
- Pre-fetch top 500 most notable exoplanets on first run, cache locally as JSON
- Additional planets fetched on-demand when user searches, cached after retrieval
- Works offline for all cached planets
- Extend existing `.cache/nasa/` pattern to `.cache/oec/`, `.cache/gaia/`, `.cache/cds/`

### Gaia DR3 integration depth
- Full stellar enrichment: pull Teff, luminosity, metallicity, age, distance, logg for every host star
- This gives the LLM inference engine (Phase 2) much better inputs — directly improves rendering quality
- Query Gaia Archive TAP endpoint with ADQL, cross-match by host star name or coordinates

### Data conflict resolution
- Lowest uncertainty wins: pick the measurement with smallest error bars regardless of source
- Fall back to fixed source priority (NASA > Gaia > CDS > OEC) when uncertainties are equal or missing
- MeasuredValue already tracks source + uncertainty — extend to support this logic

### Claude's Discretion
- Exact ADQL query structures for each source
- OEC XML parsing implementation details
- Cache file format and TTL values
- HTTP retry and timeout strategies

</decisions>

<specifics>
## Specific Ideas

- User wants ALL FOUR sources integrated in the first milestone — not incremental
- NASA TAP `pscomppars` table is the primary catalog (composite planet parameters)
- OEC provides binary system hierarchies that NASA doesn't include
- CDS/VizieR accessed via TAP/ADQL (same protocol as NASA and Gaia)
- Gaia DR3 has 470M stars with astrophysical parameters — match by host star
- User has Gaia archive credentials (do NOT store — use environment variables)
- Data provenance tracking is critical: `DataSource` enum already exists with NASA_TAP, AI_INFERRED, etc. — extend with OEC, GAIA, CDS_VIZIER

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `NasaApiClient` (src/data/NasaApiClient.hpp/cpp): Complete libcurl HTTP client with ADQL builder, JSON parsing, file cache. Pattern to replicate for OEC/CDS/Gaia clients.
- `ExoplanetData` (src/data/ExoplanetData.hpp): Rich data model with MeasuredValue<T> provenance tracking. Already has fields for all 4 sources' data.
- `DataSource` enum: Tracks NASA_TAP, EXOATMOS, AI_INFERRED, CALCULATED. Extend for new sources.
- `HostStarData` struct: Already has fields for Teff, radius, mass, luminosity, metallicity, spectral type, distance, age.

### Established Patterns
- PIMPL pattern for API clients (NasaApiClient::Impl hides libcurl details)
- std::future for async queries (queryByName returns future)
- nlohmann/json for all JSON parsing
- spdlog logging macros throughout
- File-based cache in `.cache/` directory

### Integration Points
- `NasaApiClient` is constructed in isolation — needs to be wired into Application lifecycle
- `ExoplanetData::calculateDerivedValues()` already computes fields from known data
- `ExoplanetData::toJson()/fromJson()` for serialization — use for cache files
- New clients (OEC, CDS, Gaia) should follow same interface pattern as NasaApiClient

</code_context>

<deferred>
## Deferred Ideas

- WebGPU/WASM port — future milestone
- Side-by-side planet comparison — future phase
- TESS lightcurve data integration — could enrich transit visualization
- ExoMerCat synthetic spectra catalog — could improve atmosphere color inference

</deferred>

---

*Phase: 01-data-aggregation-layer*
*Context gathered: 2026-02-28*
