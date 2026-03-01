# Astrodex — Roadmap

## Milestones

- **v1.0 Real Exoplanet Data + AI Inference Pipeline** — Phases 1-4 (shipped 2026-03-01)

## Phases

<details>
<summary>v1.0 (Phases 1-4) — SHIPPED 2026-03-01</summary>

- [x] Phase 1: Data Aggregation Layer (4/4 plans) — 4-source pipeline with fusion and provenance
- [x] Phase 2: AI Inference Pipeline (3/3 plans) — Bedrock/Claude gap-filling, ExoplanetMapper, ML benchmarks
- [x] Phase 3: Search & Render Integration (2/2 plans) — Autocomplete, info panel, provenance display
- [x] Phase 4: Exoplanet Catalogue Browser (3/3 plans) — Card grid, thumbnails, sorting/filtering

See: `.planning/milestones/v1.0-ROADMAP.md` for full details.

</details>

### Phase 5: Port Info Panel to tejui UIManager
**Goal:** Cherry-pick the Phase 3 Plan 2 info panel rendering code into the tejui-merged UIManager — restoring data provenance color-coding, Physical/Orbital/Host Star collapsible sections, and AI reasoning tooltips.

**Requirements:** R3.2, R3.3

**Gap Closure:** Closes integration gap (Phase 3 Plan 2 → UIManager) and flow gap (Search → Load → Display Info Panel)

**Success Criteria:**
- [ ] Info panel renders Physical, Orbital, Host Star subsections when planet loaded
- [ ] Provenance color-coding: white (measured), cyan (AI-inferred), yellow (calculated)
- [ ] Provenance legend visible at top of info panel
- [ ] Hover tooltips on AI-inferred values showing reasoning and confidence
- [ ] m_exoData consumed in render() — no longer orphaned

**Estimated Complexity:** Low — porting existing code into new UI structure

---

## Future Milestones (not yet planned)

- **Milestone 2:** WebGPU/WASM browser port
- **Milestone 3:** Side-by-side comparison, multi-planet systems
- **Milestone 4:** Real-time orbit animation, transit visualization
