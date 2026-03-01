# Astrodex — Project Context

## What This Is

A procedural exoplanet renderer that pulls **real observational data** from 4 astronomical databases (NASA TAP, OEC, Gaia DR3, CDS/VizieR), uses **AI inference** to fill data gaps, and produces scientifically-grounded 3D visualizations of any known exoplanet. Ships with a Pokedex-style catalogue of 500+ planets with mini-render previews.

## Core Value

Type any exoplanet name → see it rendered with real data, AI-inferred where needed, with full provenance transparency.

## Current State (after v1.0)

- **Codebase:** 97 C++ source files, ~21,000 LOC | 9 test files, 538 assertions
- **Tech stack:** C++23, Vulkan 1.2, GLFW, ImGui 1.91.6-docking, GLM, spdlog, nlohmann/json, VMA, VkBootstrap, pugixml, stb_image_write, libcurl, AWS SDK (Bedrock)
- **Python:** Experimental ML harness (BERT, BART, TabTransformer, MaskedAutoencoder benchmarks)
- **Branch:** `refactor/ai` (Vulkan-only, tejui UI merged)

### What's Shipped
- 4-source data pipeline with uncertainty-based fusion and provenance tracking
- AI inference via AWS Bedrock/Claude with confidence scores and reasoning
- Physics-based ExoplanetData → PlanetParams mapping
- Search with autocomplete, real-time pipeline status display
- Catalogue browser: card grid, sorting, filtering, search, progressive thumbnails
- Offscreen Vulkan FBO renderer for catalogue mini-previews with PNG disk cache
- Dark/light theme toggle, tab-based planet editor (DATA/WORLD/VISUAL/LIGHT)

### Known Tech Debt
- Info panel provenance display (R3.2/R3.3) needs porting into tejui-merged UIManager
- Catalogue thumbnails are colored placeholders, not full procedural renders
- CoordinateMatcher unused (cone searches used instead)

## Requirements

### Validated

- ✓ NASA TAP Integration — v1.0
- ✓ Open Exoplanet Catalogue Integration — v1.0
- ✓ CDS/VizieR Cross-Matching — v1.0
- ✓ Gaia DR3 Host Star Enrichment — v1.0
- ✓ Data Fusion & Conflict Resolution — v1.0
- ✓ Bedrock/Claude AI Gap-Filling — v1.0
- ✓ ExoplanetData → PlanetParams Conversion — v1.0
- ✓ ML Benchmarking Framework — v1.0
- ✓ Search Interface with Autocomplete — v1.0
- ✓ Data Provenance Display — v1.0
- ✓ Planet Info Panel — v1.0
- ✓ Local Exoplanet Database (500+ prefetched) — v1.0
- ✓ Catalogue UI with Sorting/Filtering — v1.0
- ✓ Mini Render Previews — v1.0

### Active

(None — next milestone not yet planned)

### Out of Scope

- WebGPU/WASM browser port (future milestone)
- Side-by-side planet comparison view
- Real-time orbit animation
- Multi-planet system visualization
- User accounts or cloud sync
- Mobile app

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Vulkan-only (drop Metal/OpenGL) | Unified codebase, modern GPU features | ✓ Good |
| NASA > Gaia > CDS > OEC priority | NASA has highest quality measured data | ✓ Good |
| Claude/Bedrock for production AI | Structured JSON, scientific reasoning | ✓ Good |
| OEC CSV bulk + NASA TAP merge for catalogue | Single HTTP call for 5000 planets | ✓ Good |
| Deferred full procedural thumbnails | Colored placeholders ship faster | ⚠️ Revisit |
| tejui UI merge (tab bar, theme, galaxy view) | Modern UI, but overwrote info panel | ⚠️ Revisit |

## User (Keanu)

- Wants real exoplanet data driving the renderer
- Experiments with multiple LLM/ML architectures
- Prioritizes data+inference pipeline over UI polish
- Future goals: WebGPU port, side-by-side comparison

---
*Last updated: 2026-03-01 after v1.0 milestone*
