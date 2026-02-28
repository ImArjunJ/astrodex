# Astrodex — Project Context

## Vision
A procedural exoplanet renderer that uses **real observational data** from multiple astronomical databases and **LLM inference** to fill data gaps, producing scientifically-grounded 3D visualizations of known exoplanets.

## Project Type
Desktop application (C++23 / Vulkan). Active development branches (`feat-render`, `Tej`) have migrated from OpenGL 4.5 to Vulkan/Metal with an `IRenderer.hpp` abstraction layer for cross-platform support (Windows, macOS, Linux, Apple Silicon, x64). WebGPU browser target is a future milestone.

## What Exists
- Procedural planet renderer with ~100 tunable parameters (terrain, biome, ocean, atmosphere, gas giant, rings, lighting)
- Manual presets for Solar System bodies (Earth, Mars, Jupiter, etc.)
- ImGui UI with sliders and preset selection
- Orbit camera, shader-based rendering pipeline
- **Built but not wired in:**
  - `NasaApiClient` — queries NASA Exoplanet Archive TAP endpoint
  - `ExoplanetData` — rich data model with `MeasuredValue<T>` provenance tracking (NASA vs AI-inferred)
  - `InferenceEngine` — AWS Bedrock/Claude integration for gap-filling
  - `PromptTemplates` — structured scientific prompts for atmosphere/render inference
  - `CelestialBodyParams::fromObservations()` — factory to convert observational data to render params

## What We're Building (Milestone 1)
1. **Data Aggregation Layer** — unified pipeline pulling from 4 canonical datasets:
   - NASA Exoplanet Archive (TAP) — primary catalog, confirmed exoplanets
   - Open Exoplanet Catalogue (OEC) — XML/GitHub-based, includes binary system hierarchies
   - CDS/VizieR (Strasbourg) — cross-match host star properties, large catalog access
   - Gaia DR3 (ESA) — precise stellar distances, temperatures, luminosities, metallicities

2. **AI Inference Pipeline** — multi-model approach:
   - **Primary (production)**: Claude via AWS Bedrock (already integrated) for structured JSON inference
   - **Experimental**: BERT, BART, TabTransformer, ReMasker for benchmarking
   - Goal: given known params, infer missing atmosphere, surface, rendering properties

3. **Search + Render** — type an exoplanet name → fetch data → LLM fills gaps → render in viewport
4. **Catalogue Browser** — scrollable list with mini-render previews, sorted by discovery date

## Key Data Sources
| Source | What It Provides | Access |
|--------|-----------------|--------|
| NASA TAP | Planet mass, radius, orbital params, equilibrium temp, discovery info | HTTP ADQL queries, JSON output |
| Open Exoplanet Catalogue | Same + binary system hierarchies, community-maintained | GitHub XML files, also CSV tables |
| CDS/VizieR | 27,200+ astronomical catalogs, host star enrichment | TAP/ADQL, astroquery Python |
| Gaia DR3 | Stellar parallax/distance, Teff, luminosity, metallicity, age | ESA Archive TAP, ADQL queries |

## LLM Architecture Notes
- **Claude/Bedrock** (primary): Best for structured scientific inference from partial data. Returns JSON with confidence levels.
- **BERT**: Encoder-only. Useful for classification (planet type, habitability tier). NOT for generation.
- **BART**: Encoder-decoder. Can generate structured output but weaker than Claude for scientific reasoning.
- **TabTransformer**: Purpose-built for tabular data. Strong candidate for numerical imputation (missing mass, radius, temperature).
- **ReMasker**: Masked autoencoding for tabular imputation. Research-grade, promising for this domain.
- **T5/Flan-T5**: Text-to-text, decent at structured prediction. Lighter-weight alternative to Claude.

## User (Keanu)
- Wants real exoplanet data driving the renderer, not just manual presets
- Wants to experiment with multiple LLM/ML architectures
- Prioritizes the data+inference pipeline over UI polish
- Future: WebGPU port, side-by-side comparison, more data sources

## Tech Stack
C++23, Vulkan (feat-render branch) / Metal (Tej branch), GLFW, ImGui, GLM, spdlog, nlohmann/json, CMake, AWS SDK (Bedrock)
Python (for experimental ML models: transformers, torch, PEFT/LoRA)

## Renderer Architecture Notes
- **master branch**: Original OpenGL 4.5 renderer with elaborate `CelestialBodyParams` (~100 params)
- **feat-render branch**: Vulkan implementation (`VulkanRenderer.cpp`, 1040 lines) with `IRenderer.hpp` abstraction and simplified `PlanetParams` struct
- **Tej branch**: Metal implementation (`MetalRenderer.mm`, 430 lines) with `IRenderer.hpp` abstraction and simplified `PlanetParams` struct
- Phase 1 (Data Aggregation) works against `ExoplanetData` model which is shared across all branches — renderer-specific mapping happens in Phase 2/3
- The `IRenderer.hpp` `PlanetParams` on active branches is simpler than master's `CelestialBodyParams` — the ExoplanetData→PlanetParams mapping (R2.2) must target the new interface
