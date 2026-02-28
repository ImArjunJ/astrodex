# Astrodex — Project State

## Current Status
- **Milestone:** 1 (Real Exoplanet Data + AI Inference Pipeline)
- **Phase:** 1 — context gathered, plan-phase initialized but not yet spawned agents
- **Next Action:** `/gsd:plan-phase 1` (researching then planning)
- **Last Session:** 2026-02-28 — context exhausted during plan-phase init, resuming

## Completed
- [x] Codebase mapped (.planning/codebase/)
- [x] Project initialized (.planning/PROJECT.md)
- [x] Requirements defined (.planning/REQUIREMENTS.md)
- [x] Roadmap created (.planning/ROADMAP.md)
- [x] Research: NASA TAP API, OEC, CDS/VizieR, Gaia DR3, BERT/BART/TabTransformer

## Key Decisions
1. **Platform:** Desktop (C++/Vulkan) first, WebGPU later — active branches (feat-render, Tej) have migrated from OpenGL to Vulkan/Metal with IRenderer.hpp abstraction
2. **Data sources:** All 4 (NASA TAP, OEC, CDS/VizieR, Gaia DR3)
3. **Primary LLM:** Claude via AWS Bedrock (already integrated)
4. **Experimental models:** BERT, BART, TabTransformer, ReMasker (Python harness)
5. **UX priority:** Search+render first → catalogue browser → side-by-side (future)
6. **Focus:** Data aggregation + AI inference pipeline (over UI polish)

## Research Artifacts
- `.firecrawl/nasa-tap.md` — NASA TAP API docs
- `.firecrawl/nasa-api.md` — NASA API user guide
- `.firecrawl/gaia-dr3.md` — Gaia DR3 contents and access
- `.firecrawl/gaia-programmatic.md` — Gaia programmatic access
- `.firecrawl/cds-tools.md` — CDS Strasbourg tools
- `.firecrawl/cds-services.md` — CDS services (SIMBAD, VizieR, Aladin)
- `.firecrawl/oec-site.md` — Open Exoplanet Catalogue overview
- `.firecrawl/astroquery-vizier.md` — astroquery VizieR Python docs
- `.firecrawl/hf-bart-docs.md` — BART model documentation
- `.firecrawl/h2o-bert.md` — BERT overview
- `.firecrawl/adapters-blog.md` — Adapter/LoRA patterns
- `.firecrawl/hf-bert-lora-discuss.md` — BERT LoRA discussion
