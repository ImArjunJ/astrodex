# Astrodex — Roadmap (Milestone 1)

## Milestone 1: Real Exoplanet Data + AI Inference Pipeline

### Phase 1: Data Aggregation Layer
**Goal:** Build a unified data pipeline that pulls exoplanet data from all 4 canonical sources, cross-matches host stars, and produces fused `ExoplanetData` records with full provenance tracking.

**Requirements:** R1.1, R1.2, R1.3, R1.4, R1.5

**Plans:** 4 plans

Plans:
- [x] 01-01-PLAN.md — Infrastructure setup (CMake/pugixml/Catch2), DataSource extension, NASA TAP cache + tests (15.6 min)
- [x] 01-02-PLAN.md — OEC XML client with pugixml parsing and unit tests (10 min)
- [x] 01-03-PLAN.md — Gaia DR3 + CDS/VizieR TAP clients with host star enrichment
- [x] 01-04-PLAN.md — CoordinateMatcher, DataFusionEngine, CacheManager (cross-matching + fusion + offline cache) (3 min)

**Success Criteria:**
- [x] NASA TAP client wired in and returning ExoplanetData for any named planet
- [x] OEC XML parser producing ExoplanetData records
- [x] CDS/VizieR TAP queries enriching host star properties
- [x] Gaia DR3 cross-matching providing stellar parallax, Teff, luminosity
- [x] Data fusion logic merging 4 sources with priority ordering and provenance
- [x] Local cache storing fused records for instant retrieval
- [x] Can fetch "Kepler-442b" and get a complete, multi-source ExoplanetData record

**Estimated Complexity:** High — 4 different API protocols, XML/JSON parsing, cross-matching logic

---

### Phase 2: AI Inference Pipeline
**Goal:** Wire the existing Bedrock/Claude inference engine into the data pipeline so missing exoplanet parameters are automatically inferred, and set up experimental ML model benchmarking.

**Requirements:** R2.1, R2.2, R2.3

**Plans:** 3 plans

Plans:
- [ ] 02-01-PLAN.md — InferenceEngine integration into DataFusionEngine + deterministic fallback + JSON serialization (R2.1)
- [ ] 02-02-PLAN.md — Physics-based CelestialBodyParams::fromObservations(ExoplanetData) mapping (R2.2)
- [ ] 02-03-PLAN.md — Python ML benchmarking harness: BERT, BART, TabTransformer, ReMasker (R2.3)

**Success Criteria:**
- [ ] InferenceEngine wired into data pipeline: fetch → identify gaps → infer → merge
- [ ] AI-inferred values correctly marked with DataSource::AI_INFERRED and reasoning
- [ ] ExoplanetData → CelestialBodyParams mapping implemented
- [ ] Given "Kepler-442b" data, produces a complete CelestialBodyParams ready for rendering
- [ ] Python experimental harness running BERT, BART, TabTransformer benchmarks
- [ ] Comparison document: Claude vs BERT vs BART vs TabTransformer for this domain

**Estimated Complexity:** High — inference integration, parameter mapping with physics, ML experimentation

---

### Phase 3: Search & Render Integration
**Goal:** Connect the data+inference pipeline to the renderer so users can type an exoplanet name and see it rendered with real data.

**Requirements:** R3.1, R3.2, R3.3

**Success Criteria:**
- [ ] ImGui search box with autocomplete from cached planet names
- [ ] Full pipeline: search → fetch → fuse → infer → convert → render (end-to-end)
- [ ] Data provenance UI showing NASA vs AI-inferred values with color-coding
- [ ] Planet info panel displaying key facts and host star data
- [ ] Smooth UX: loading state during fetch/inference, then seamless render transition

**Estimated Complexity:** Medium — mostly UI wiring + async pipeline orchestration

---

### Phase 4: Exoplanet Catalogue Browser
**Goal:** Build a browseable catalogue of known exoplanets with mini-render previews, sorted by discovery date.

**Requirements:** R4.1, R4.2, R4.3

**Success Criteria:**
- [ ] Local database of 500+ pre-fetched exoplanet records
- [ ] Scrollable ImGui catalogue panel with sorting/filtering
- [ ] Mini render previews via low-res offscreen FBO rendering
- [ ] Click catalogue entry → renders full planet in main viewport
- [ ] Background prefetch and progressive thumbnail generation

**Estimated Complexity:** Medium — offscreen rendering, texture caching, UI list virtualization

---

## Future Milestones (not in scope)
- **Milestone 2:** WebGPU/WASM browser port
- **Milestone 3:** Side-by-side comparison, multi-planet systems
- **Milestone 4:** Real-time orbit animation, transit visualization
