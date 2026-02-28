# ML Branch — TODO & Status

> **Context**: This branch implements the full Claude/AWS Bedrock inference pipeline
> as the primary ML approach. It is also the **backup fallback** if a local BERT-based
> model cannot be trained or integrated in time. The entire pipeline degrades gracefully:
> if AWS credentials are absent, the app still renders planets from physics derivations
> and solar-system analog matching alone.

---

## What works right now

| Feature | Status |
|---|---|
| Real-time procedural planet renderer (Metal / OpenGL) | ✅ |
| Solar system direct lookup (Mercury → Neptune, instant, no AI) | ✅ |
| NASA TAP query with planet-name normalisation (`Kepler-452b` → `Kepler-452 b`) | ✅ |
| Claude atmosphere inference (pressure, albedo, composition, ice/ocean/cloud fractions) | ✅ requires AWS |
| Solar-system analog matching (log-normalised mass/radius/temp similarity) | ✅ |
| Analog context passed to Claude render-params prompt | ✅ |
| Fill-empty-slots philosophy (AI never overrides measured data) | ✅ |
| Confidence-weighted blending (`_confidence` JSON → lerp instead of hard replace) | ✅ requires AWS |
| Known-planet validation (AI pipeline run on solar system planets, accuracy % in UI) | ✅ requires AWS |
| Per-field accuracy log for prompt iteration | ✅ requires AWS |

---

## Pending tasks

### High priority

- [ ] **Install AWS CLI + configure Bedrock credentials**
  - `brew install awscli && aws configure`
  - Enable Claude Sonnet model access in AWS Console → Bedrock → Model access
  - Required before any of the AI features can be tested end-to-end

- [ ] **Run known-planet validation and measure baseline accuracy**
  - Load `Earth`, `Mars`, `Jupiter` etc. with AWS active
  - Check terminal logs for per-field breakdown
  - Identify weakest fields (likely surface colours) to iterate prompts on

- [ ] **Prompt iteration based on validation scores**
  - Target: >80% overall accuracy on Earth, Mars, Jupiter before expanding to exoplanets
  - Tweak `RENDER_PARAMS_SYSTEM_PROMPT` and `buildRenderParamsPrompt` in
    `src/ai/PromptTemplates.hpp` based on which fields score poorly

### Medium priority

- [ ] **BERT / local ML approach (primary goal if feasible)**
  - Train a small regression model on NASA archive data to predict missing
    `PlanetParams` fields from physical observables (mass, radius, temp, density)
  - Replace or augment the Claude `inferRenderParamsSync` call with local inference
  - Advantage: no AWS dependency, faster, deterministic, hackathon-demo friendly
  - Fallback: use this Claude pipeline if BERT training doesn't converge in time

- [ ] **Expand SolarSystemDatabase to include dwarf planets / moons**
  - Pluto, Titan, Europa, Io — good visual variety for demos
  - Increases analog-match coverage for icy/volcanic exoplanets

- [ ] **Cache NASA query results to disk**
  - `NasaApiClient` has a `use_cache` config flag but it's not wired up yet
  - Prevents repeated network calls for the same planet during a demo

### Low priority / polish

- [ ] **UI: show analog match name in a dedicated label** (currently appended to status string)
- [ ] **UI: display per-field validation breakdown in a collapsible panel**
- [ ] **UI: loading spinner while async pipeline runs**
- [ ] **Smooth parameter transition** — lerp current `PlanetParams` to new target over
  ~1 second instead of snapping instantly when a planet loads
- [ ] **Star colour influence** — use host star spectral type to tint `sunColor`
  (already available in `ExoplanetData.host_star.spectral_type`)

---

## Architecture summary (for onboarding)

```
User types planet name
        │
        ▼
SolarSystemDatabase::findByName()   ←── instant, no network
  hit  │           miss │
       ▼                ▼
render known      NasaApiClient::queryByNameSync()
params directly         │
       │           ExoplanetData (measured fields)
       │                │
       │           InferenceEngine::fillMissingParametersSync()
       │             └─ inferAtmosphere() → Claude call 1
       │                │
       │           ExoplanetMapper::toPlanetParams()   (physics pass)
       │             └─ tracks physicsFields (skipFields for AI)
       │                │
       │           find analog, build analogContext
       │                │
       │           InferenceEngine::inferRenderParamsSync()  → Claude call 2
       │             └─ skips physicsFields, uses analogContext
       │                │
       │           ExoplanetMapper::applyAIRenderOverrides()
       │             └─ confidence-weighted lerp, not hard replace
       │                │
       └──────────►  PlanetParams → renderer
```

---

## Key files

| File | Role |
|---|---|
| `src/ai/PromptTemplates.hpp` | All Claude prompts — edit to tune accuracy |
| `src/render/ExoplanetMapper.cpp` | Physics→params pass + AI override + validation |
| `src/data/SolarSystemDatabase.cpp` | Ground-truth visual params for 8 planets |
| `src/ai/BedrockClient.cpp` | AWS CLI invocation + JSON parsing |
| `src/core/Application.cpp` | Orchestrates the full 6-step pipeline |
