---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: milestone
current_plan: Phase 4 Plan 2 (COMPLETE)
status: in-progress
stopped_at: Completed 04-02-PLAN.md
last_updated: "2026-03-01T02:59:48Z"
progress:
  total_phases: 4
  completed_phases: 3
  total_plans: 12
  completed_plans: 11
---

# Astrodex — Project State

## Current Status
- **Milestone:** 1 (Real Exoplanet Data + AI Inference Pipeline)
- **Phase:** 4 -- IN PROGRESS (2/3 plans done)
- **Current Plan:** Phase 4 Plan 2 (COMPLETE)
- **Next Action:** Execute Phase 4 Plan 3 (Progressive Thumbnail Generation)
- **Last Session:** 2026-03-01T02:59:48Z
- **Stopped At:** Completed 04-01-PLAN.md
- **Phase 01 Summary:** Multi-source data fusion with NASA, OEC, Gaia, CDS clients. Uncertainty-based selection, JSON cache, haversine coordinate matching.
- **Phase 02 Plan 03 Summary:** Python ML benchmark harness with BERT, BART, TabTransformer, MaskedAutoencoder model wrappers and comparison document generator.
- **Phase 02 Plan 01 Summary:** InferenceEngine wired into DataFusionEngine with complete JSON serialization, deterministic fallback, and 17 integration tests (409 assertions).
- **Phase 02 Plan 02 Summary:** Physics-based CelestialBodyParams mapping with Rayleigh scattering from atmosphere composition, multi-factor terrestrial surface model, gas/ice giant heuristics, 15 test cases (114 assertions).
- **Phase 03 Plan 01 Summary:** Autocomplete dropdown with prefix-matching from cache, thread-safe pipeline stage status via atomic int, disabled input during loading, ExoplanetData stored for info panel.
- **Phase 03 Plan 02 Summary:** Planet info panel with provenance color-coding (white/cyan/yellow), AI reasoning tooltips, fade transition, DataFusionEngine multi-source pipeline wiring.
- **Phase 04 Plan 01 Summary:** CatalogueView card grid with type/HZ filtering, multi-criteria sorting, prefix search, offline-first prefetch integration into Application lifecycle.
- **Phase 04 Plan 02 Summary:** Vulkan offscreen FBO with reusable 128px framebuffer, GPU-to-CPU readback via staging buffer, PNG serialization via stb_image_write, ImGui texture registration.

## Completed
- [x] Codebase mapped (.planning/codebase/)
- [x] Project initialized (.planning/PROJECT.md)
- [x] Requirements defined (.planning/REQUIREMENTS.md)
- [x] Roadmap created (.planning/ROADMAP.md)
- [x] Research: NASA TAP API, OEC, CDS/VizieR, Gaia DR3, BERT/BART/TabTransformer
- [x] Phase 01 Plan 01: Build infrastructure + NASA client foundation (15.6 min, 3 tasks, 3 commits)
- [x] Phase 01 Plan 02: OEC client with XML parsing (10 min, 2 tasks, 2 commits)
- [x] Phase 01 Plan 03: Gaia DR3 + CDS/VizieR TAP clients (8 test cases, 75 assertions passing)
- [x] Phase 01 Plan 04: DataFusionEngine + CacheManager (3 min, 3 tasks, 4 commits, 242 assertions passing)
- [x] Phase 02 Plan 03: ML benchmarking harness (7 min, 2 tasks, 2 commits)
- [x] Phase 02 Plan 01: InferenceEngine integration + JSON serialization + deterministic fallback (8 min, 2 tasks, 3 commits, 409 assertions passing)
- [x] Phase 02 Plan 02: Physics-based CelestialBodyParams mapping with Rayleigh scattering, terrestrial/gas giant/ice giant models (5 min, 2 tasks, 2 commits, 114 assertions passing)
- [x] Phase 03 Plan 01: Search autocomplete + pipeline stage status (4 min, 2 tasks, 2 commits)
- [x] Phase 03 Plan 02: Planet info panel + data provenance + fade transition (~15 min, 2 tasks, 2 commits)
- [x] Phase 04 Plan 01: CatalogueView card grid with filtering/sorting/search, Application prefetch integration (6 min, 2 tasks, 2 commits)
- [x] Phase 04 Plan 02: ThumbnailRenderer with offscreen Vulkan FBO, PNG serialization, ImGui texture registration (5 min, 2 tasks, 2 commits, 129 assertions passing)

## Key Decisions
1. **Platform:** Desktop (C++/Vulkan) first, WebGPU later — active branches (feat-render, Tej) have migrated from OpenGL to Vulkan/Metal with IRenderer.hpp abstraction
2. **Data sources:** All 4 (NASA TAP, OEC, CDS/VizieR, Gaia DR3)
3. **Primary LLM:** Claude via AWS Bedrock (already integrated)
4. **Experimental models:** BERT, BART, TabTransformer, ReMasker (Python harness)
5. **UX priority:** Search+render first → catalogue browser → side-by-side (future)
6. **Focus:** Data aggregation + AI inference pipeline (over UI polish)
7. **NASA cache TTL:** 30 days (NASA updates quarterly) — Phase 01 Plan 01
8. **CURL build:** From source for headless environments — Phase 01 Plan 01
9. **OpenGL optional:** Support headless builds — Phase 01 Plan 01
10. **OEC cache TTL:** 7 days (OEC updates frequently from community) — Phase 01 Plan 02
11. **Cross-matching strategy:** Name-first with 5-arcsec coordinate fallback using haversine distance — Phase 01 Plan 04
12. **Data fusion priority:** Uncertainty-based selection with source priority fallback NASA > Gaia > CDS > OEC — Phase 01 Plan 04
13. **Fused cache TTL:** 30 days (fused records stable, sources update slowly) — Phase 01 Plan 04
14. **ML model implementations:** Lightweight PyTorch transformers (not full pretrained downloads) for portability — Phase 02 Plan 03
15. **ReMasker approach:** Simplified masked autoencoder in PyTorch (avoids timm/hyperimpute deps) — Phase 02 Plan 03
16. **Synthetic data fallback:** Auto-generate physically plausible exoplanet data when cache empty — Phase 02 Plan 03
17. **applyDeterministicDefaults public static:** For testability, same as mergeExoplanetData — Phase 02 Plan 01
18. **fmt::format over std::format:** GCC 12.2 lacks <format> header; use spdlog bundled fmt — Phase 02 Plan 01
19. **JSON sections:** Organized ExoplanetData into atmosphere/rendering/classification sections — Phase 02 Plan 01
20. **AI retry strategy:** One retry with 2-second delay per inference call, then fallback to deterministic defaults — Phase 02 Plan 01
21. **ExoplanetData forward declaration:** Forward declare in PlanetParams.hpp, full include in .cpp to avoid circular dependency — Phase 02 Plan 02
22. **Body type boundaries:** radius==2.0 is IceGiant (>=2), radius==6.0 is IceGiant (<=6), consistent with user's "2-6=IceGiant" spec — Phase 02 Plan 02
23. **Ice giant band count:** Clamped 4-10 (vs gas giant 4-20) for visually distinct fewer bands — Phase 02 Plan 02
24. **Greenhouse model:** CO2>90% + P>50atm multiplies T by 1.8; moderate CO2>10% uses T*(1+co2/500) — Phase 02 Plan 02
25. **Autocomplete via ImGui::Begin window:** Not popup, to avoid focus stealing from InputText — Phase 03 Plan 01
26. **std::atomic<int> for pipeline stage:** Single-writer-single-reader pattern, no mutex needed — Phase 03 Plan 01
27. **LoadResult as tuple:** Changed from pair to include ExoplanetData for downstream info panel — Phase 03 Plan 01
28. **CacheManager::retrieve() for name casing:** Recover proper planet name from cached JSON at startup — Phase 03 Plan 01
29. **Provenance color-coding:** getSourceColor() maps DataSource to ImVec4: white=measured, cyan=AI-inferred, yellow=calculated — Phase 03 Plan 02
30. **Fade transition from saved params:** Always multiply alpha from m_savedBaseParams to avoid floating-point drift — Phase 03 Plan 02
31. **DataFusionEngine in loadPlanet():** Full multi-source pipeline (NASA+OEC+Gaia+CDS) instead of NASA-only — Phase 03 Plan 02
32. **Single reusable 128px FBO:** One offscreen framebuffer reused serially for all thumbnails to save VRAM — Phase 04 Plan 02
33. **Placeholder thumbnails first:** Planet-type-colored clear instead of full procedural rendering; full pipeline deferred to optimization pass — Phase 04 Plan 02
34. **reinterpret_cast for ImTextureID:** VkDescriptorSet to unsigned long long conversion on Linux where ImTextureID is integer, not pointer — Phase 04 Plan 02
35. **No Y-flip for PNG readback:** Both Vulkan framebuffer and PNG use top-left origin with standard viewport — Phase 04 Plan 02
36. **Card dimensions 200x280px:** With 16px spacing and 8px corner rounding for Pokedex-style grid — Phase 04 Plan 01
37. **Full-screen catalogue overlay:** Catalogue replaces planet detail entirely (not sidebar), maximizes grid space — Phase 04 Plan 01
38. **Offline-first cache-then-prefetch:** Load cached records instantly, merge background prefetch results with deduplication — Phase 04 Plan 01
39. **Habitable zone filter:** Uses sqrt(host_star_luminosity) * 0.95/1.67 AU bounds from ExoplanetData constants — Phase 04 Plan 01

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
