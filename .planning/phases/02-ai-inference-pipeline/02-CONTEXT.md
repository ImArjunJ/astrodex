# Phase 2: AI Inference Pipeline - Context

**Gathered:** 2026-02-28
**Status:** Ready for planning

<domain>
## Phase Boundary

Wire the existing Bedrock/Claude inference engine into the data pipeline so missing exoplanet parameters are automatically inferred after data fusion. Implement ExoplanetData to CelestialBodyParams mapping with physics-based modeling. Set up Python experimental ML benchmarking harness (BERT, BART, TabTransformer) alongside the main C++ app. Rendering UI, search interface, and provenance display are separate phases.

</domain>

<decisions>
## Implementation Decisions

### Gap-filling strategy
- Always run AI inference after DataFusionEngine produces fused results — every fetchAndFuse call automatically triggers inference on missing fields
- AI only fills truly missing (NaN) fields — never overwrite measured or calculated data
- Cache AI-enriched records alongside fused data in the existing CacheManager (30-day TTL applies to the full enriched record)
- When a cached enriched record exists, skip both fusion and inference on repeat lookups

### Parameter mapping — body type classification
- Use standard mass/radius thresholds for deterministic body type classification:
  - < 2 R_Earth = Terrestrial
  - 2-6 R_Earth = Neptune-like / IceGiant
  - > 6 R_Earth = GasGiant
- No AI dependency for body type — purely deterministic from observational data

### Parameter mapping — terrestrial surfaces
- Multi-factor physical model considering temperature AND atmosphere AND stellar distance AND stellar type
- Example: thick CO2 atmosphere creates greenhouse effect shifting biome zones (Venus analog)
- Equilibrium temperature is the primary driver, but atmosphere composition and pressure modulate surface appearance
- AI-inferred atmosphere_composition feeds into the physical model when available

### Parameter mapping — atmosphere rendering
- Physics-based mapping from atmosphere_composition to Rayleigh scattering coefficients
- N2/O2 dominant = Earth-blue sky, CO2-thick = orange-haze (Venus), H2/He = pale blue (Neptune/Uranus)
- Cloud layers derived from temperature + pressure profiles
- Surface pressure drives atmosphere density and height parameters

### Parameter mapping — gas giants
- Temperature + mass heuristics for band structure and storm patterns
- Hot Jupiters: muted/dark bands, intense storm activity
- Cold gas giants: Jupiter-like banded structure with higher contrast
- Band count scales with radius, great spot probability scales with planet size

### ML benchmarking
- Fully standalone Python scripts in separate directory (e.g., experiments/ml/)
- Own requirements.txt, reads cached exoplanet JSON files from .cache/fused/ for test data
- Full comparison across accuracy (vs known NASA values), inference latency, and cost
- Output: comparison document presenting trade-offs, not picking a single winner
- Methodology: mask known fields from complete exoplanet records, predict, measure error

### Fallback behavior
- When AWS Bedrock is unavailable: apply rule-based deterministic defaults
- Rule-based defaults use mass/radius/temperature to produce a renderable planet (same physical models as parameter mapping, minus AI-specific fields like detailed composition)
- Fallback values marked as DataSource::CALCULATED to distinguish from AI_INFERRED and measured data
- INFO-level logging when fallback is used (e.g., "AI unavailable for Kepler-442b, using rule-based atmosphere")

### Claude's Discretion
- Whether to consolidate atmosphere + render hints into a single Bedrock call or keep separate
- Retry strategy when individual Bedrock calls fail (one retry then fallback, or immediate fallback)
- ML benchmark dataset size (50-100 vs 500+ well-characterized planets)
- Exact structure and formatting of the ML comparison document

</decisions>

<specifics>
## Specific Ideas

- The pipeline should feel seamless: fetch data, fuse sources, fill gaps, convert to render params — one call produces a complete renderable planet
- Physics fidelity matters — prefer physically grounded models over simple lookups for surface/atmosphere appearance
- Gas giant rendering should vary meaningfully between hot Jupiters and cold gas giants (not just color swaps)
- ML comparison should be data-driven with tables, not just qualitative assessment

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `InferenceEngine` (src/ai/InferenceEngine.hpp): Already has `fillMissingParametersSync()`, `inferAtmosphere()`, `inferRenderHints()` — wire into pipeline
- `BedrockClient` (src/ai/BedrockClient.hpp): AWS Bedrock integration with sync/async calls, credential checking
- `PromptTemplates` (src/ai/PromptTemplates.hpp): Atmosphere and render hints prompts already defined with physics-based system prompt
- `DataFusionEngine` (src/data/DataFusionEngine.cpp): `fetchAndFuseSync()` is the integration point — currently returns without AI enrichment
- `CacheManager`: JSON-based cache with TTL, already used by DataFusionEngine for fused records
- `CelestialBodyParams::fromObservations()` (src/render/PlanetParams.hpp): Declared but not implemented — this is the mapping entry point

### Established Patterns
- `MeasuredValue<T>` template tracks value + uncertainty + DataSource + ai_reasoning + confidence — all inference results should use this
- Pimpl idiom (DataFusionEngine::Impl) for hiding implementation details
- `std::future` for async operations with sync wrappers
- nlohmann::json for all serialization/deserialization
- spdlog macros (LOG_INFO, LOG_WARN, LOG_ERROR) for structured logging

### Integration Points
- `DataFusionEngine::fetchAndFuseSync()` — add InferenceEngine call after mergeExoplanetData()
- `CelestialBodyParams::fromObservations()` — implement the full mapping logic
- `ExoplanetData::calculateDerivedValues()` — may need extension for physics-based calculations
- `.cache/fused/` directory — Python scripts will read these JSON files for benchmark data

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 02-ai-inference-pipeline*
*Context gathered: 2026-02-28*
