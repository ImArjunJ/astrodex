# Astrodex — Requirements (Milestone 1)

## Milestone Goal
Wire real exoplanet data from 4 astronomical databases through an AI inference pipeline into the existing procedural planet renderer, enabling search-and-render of any known exoplanet.

---

## R1: Unified Data Aggregation Layer
**Priority: Critical**

### R1.1: NASA TAP Integration (wire existing client) ✅
- [x] Wire existing `NasaApiClient` into the application lifecycle
- [x] Query `pscomppars` table for composite planet+host star parameters
- [x] Parse JSON response into `ExoplanetData` model
- [x] Handle missing fields gracefully (NaN for absent numerical values)
- [x] Local JSON cache with configurable TTL (30 days)
- **Completed:** Plan 01-01 — NasaApiClient with async queries, cache, tests

### R1.2: Open Exoplanet Catalogue Integration ✅
- [x] Fetch/clone OEC GitHub repository XML data
- [x] Parse XML into `ExoplanetData` model
- [x] Handle binary system hierarchies (OEC's unique strength)
- [x] Merge with NASA data (NASA as primary, OEC as supplement)
- **Completed:** Plan 01-02 — OecClient with pugixml parser, 7-day cache

### R1.3: CDS/VizieR Cross-Matching ✅
- [x] Query VizieR TAP endpoint for host star enrichment
- [x] Cross-match by star name or coordinates (RA/Dec)
- [x] Pull: spectral type, metallicity, age, photometry
- [x] Merge into `HostStarData` fields
- **Completed:** Plan 01-03 — CdsClient with SIMBAD name resolution, VizieR queries

### R1.4: Gaia DR3 Host Star Enrichment ✅
- [x] Query Gaia Archive TAP for host star matches
- [x] Pull: parallax (→ distance), Teff, luminosity, logg, metallicity
- [x] Cross-match via source_id or coordinate matching
- [x] Merge into `HostStarData`, marking source as Gaia
- **Completed:** Plan 01-03 — GaiaClient with TAP queries, cone search

### R1.5: Data Fusion & Conflict Resolution ✅
- [x] Priority ordering: NASA TAP > Gaia DR3 > CDS/VizieR > OEC
- [x] When multiple sources provide same field: use highest-precision measurement (uncertainty-based selection)
- [x] Track provenance via existing `DataSource` enum
- [x] Local JSON cache of fused exoplanet records (`.cache/fused/` with 30-day TTL)
- **Completed:** Plan 01-04 — DataFusionEngine with selectBestMeasurement(), CacheManager

---

## R2: AI Inference Pipeline
**Priority: Critical**

### R2.1: Bedrock/Claude Gap-Filling (wire existing engine) [COMPLETE]
- [x] Wire `InferenceEngine` into data pipeline
- [x] Flow: fetch data → identify missing fields → build prompt → infer → merge
- [x] Use existing `PromptTemplates` for atmosphere and render hints
- [x] Mark all inferred values with `DataSource::AI_INFERRED`
- [x] Store AI reasoning text for transparency

### R2.2: ExoplanetData → CelestialBodyParams Conversion
- Implement mapping from observational data to rendering parameters
- Use `CelestialBodyParams::fromObservations()` as foundation
- Map: equilibrium temp → surface colors, biome params
- Map: mass/radius → body type → terrain/atmosphere/gas giant params
- Map: AI-inferred atmosphere → Rayleigh scattering coefficients, cloud layers

### R2.3: Experimental Model Benchmarking Framework
- Python-based experimental harness alongside main C++ app
- Set up BERT fine-tuning for planet type classification
- Set up BART for structured parameter generation
- Evaluate TabTransformer and ReMasker for numerical imputation
- Comparison metrics: accuracy vs known values, inference latency, cost
- Output: recommendation document on which models to integrate long-term

---

## R3: Search & Single Planet Rendering
**Priority: High**

### R3.1: Search Interface
- ImGui text input for planet name search
- Autocomplete from local cache of known planet names
- Trigger: fetch → fuse → infer → convert → render pipeline
- Display loading state during async operations

### R3.2: Data Provenance Display
- Show which values are NASA-measured vs AI-inferred in UI
- Color-coding: white = measured, cyan = AI-inferred, yellow = calculated
- Expandable panel showing AI reasoning for inferred values
- Confidence indicators for each inferred parameter

### R3.3: Planet Info Panel
- Display key facts: name, mass, radius, temperature, orbital period
- Host star info: name, spectral type, distance
- Discovery info: method, year, facility

---

## R4: Exoplanet Catalogue Browser
**Priority: Medium**

### R4.1: Local Exoplanet Database
- Pre-fetch and cache top N exoplanets (configurable, default 500)
- Store fused records locally for instant browsing
- Background refresh on app start

### R4.2: Catalogue UI
- Scrollable list panel in ImGui
- Default sort: most recently discovered first
- Columns: name, type, mass, radius, temperature, discovery year
- Click to select → triggers render pipeline

### R4.3: Mini Render Previews
- Small thumbnail renders for each catalogue entry
- Strategy: low-resolution offscreen FBO renders, cached as textures
- Compute once, cache until data changes
- Progressive: render visible entries first, background-render rest

---

## Non-Requirements (Deferred)
- WebGPU/WASM port (future milestone)
- Side-by-side comparison view (future phase)
- Real-time orbit animation
- Multi-planet system visualization
- User accounts or cloud sync
