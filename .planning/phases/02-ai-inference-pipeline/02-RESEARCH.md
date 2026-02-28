# Phase 2: AI Inference Pipeline - Research

**Researched:** 2026-02-28
**Domain:** AI inference integration, physics-based parameter mapping, ML benchmarking
**Confidence:** HIGH

## Summary

Phase 2 wires the existing `InferenceEngine`/`BedrockClient` into the data pipeline so that `fetchAndFuseSync()` automatically fills missing exoplanet parameters via Claude, then maps the enriched `ExoplanetData` to `CelestialBodyParams` for rendering. The existing AI code is well-structured with `inferAtmosphere()` and `inferRenderHints()` already implemented -- the main work is integration into `DataFusionEngine`, implementing the `CelestialBodyParams::fromObservations()` upgrade with physics-based mapping, adding deterministic fallback when Bedrock is unavailable, and building a standalone Python ML benchmarking harness.

The codebase is in strong shape after Phase 1: `DataFusionEngine::fetchAndFuseSync()` is the clear integration point (line 219 in DataFusionEngine.cpp), `MeasuredValue<T>` already tracks `DataSource::AI_INFERRED` and `ai_reasoning`, and `CelestialBodyParams::fromObservations()` exists as a basic skeleton ready for the physics-based upgrade. The Python ML harness is greenfield -- no Python infrastructure exists yet.

**Primary recommendation:** Integrate InferenceEngine into DataFusionEngine after mergeExoplanetData(), implement physics-based CelestialBodyParams mapping with body type classification using the user's radius thresholds, add rule-based deterministic fallback marked as CALCULATED, and create a standalone Python experiments/ml/ directory with BERT/BART/TabTransformer/ReMasker benchmarks reading cached JSON from .cache/fused/.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
**Gap-filling strategy:**
- Always run AI inference after DataFusionEngine produces fused results -- every fetchAndFuse call automatically triggers inference on missing fields
- AI only fills truly missing (NaN) fields -- never overwrite measured or calculated data
- Cache AI-enriched records alongside fused data in the existing CacheManager (30-day TTL applies to the full enriched record)
- When a cached enriched record exists, skip both fusion and inference on repeat lookups

**Parameter mapping -- body type classification:**
- Use standard mass/radius thresholds for deterministic body type classification:
  - < 2 R_Earth = Terrestrial
  - 2-6 R_Earth = Neptune-like / IceGiant
  - > 6 R_Earth = GasGiant
- No AI dependency for body type -- purely deterministic from observational data

**Parameter mapping -- terrestrial surfaces:**
- Multi-factor physical model considering temperature AND atmosphere AND stellar distance AND stellar type
- Example: thick CO2 atmosphere creates greenhouse effect shifting biome zones (Venus analog)
- Equilibrium temperature is the primary driver, but atmosphere composition and pressure modulate surface appearance
- AI-inferred atmosphere_composition feeds into the physical model when available

**Parameter mapping -- atmosphere rendering:**
- Physics-based mapping from atmosphere_composition to Rayleigh scattering coefficients
- N2/O2 dominant = Earth-blue sky, CO2-thick = orange-haze (Venus), H2/He = pale blue (Neptune/Uranus)
- Cloud layers derived from temperature + pressure profiles
- Surface pressure drives atmosphere density and height parameters

**Parameter mapping -- gas giants:**
- Temperature + mass heuristics for band structure and storm patterns
- Hot Jupiters: muted/dark bands, intense storm activity
- Cold gas giants: Jupiter-like banded structure with higher contrast
- Band count scales with radius, great spot probability scales with planet size

**ML benchmarking:**
- Fully standalone Python scripts in separate directory (e.g., experiments/ml/)
- Own requirements.txt, reads cached exoplanet JSON files from .cache/fused/ for test data
- Full comparison across accuracy (vs known NASA values), inference latency, and cost
- Output: comparison document presenting trade-offs, not picking a single winner
- Methodology: mask known fields from complete exoplanet records, predict, measure error

**Fallback behavior:**
- When AWS Bedrock is unavailable: apply rule-based deterministic defaults
- Rule-based defaults use mass/radius/temperature to produce a renderable planet (same physical models as parameter mapping, minus AI-specific fields like detailed composition)
- Fallback values marked as DataSource::CALCULATED to distinguish from AI_INFERRED and measured data
- INFO-level logging when fallback is used (e.g., "AI unavailable for Kepler-442b, using rule-based atmosphere")

### Claude's Discretion
- Whether to consolidate atmosphere + render hints into a single Bedrock call or keep separate
- Retry strategy when individual Bedrock calls fail (one retry then fallback, or immediate fallback)
- ML benchmark dataset size (50-100 vs 500+ well-characterized planets)
- Exact structure and formatting of the ML comparison document

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| R2.1 | Bedrock/Claude Gap-Filling (wire existing engine) | InferenceEngine already has `fillMissingParametersSync()`, `inferAtmosphere()`, `inferRenderHints()`. Integration point is `DataFusionEngine::fetchAndFuseSync()` line 296 -- add InferenceEngine call after `mergeExoplanetData()`. Must add InferenceEngine as member of DataFusionEngine::Impl, guard with `isAvailable()`, add fallback path. Cache enriched records via existing CacheManager. |
| R2.2 | ExoplanetData to CelestialBodyParams Conversion | `CelestialBodyParams::fromObservations()` exists as basic skeleton in PlanetParams.cpp:464. Needs full physics-based upgrade: body type classification per user thresholds, Rayleigh scattering from atmosphere composition, multi-factor terrestrial surface model, gas giant heuristics. All sub-structs (TerrainParams, BiomeParams, AtmosphereParams, GasGiantParams, OceanParams, SurfaceColors) need population. |
| R2.3 | Experimental Model Benchmarking Framework | Greenfield Python directory. Use PyTorch + HuggingFace Transformers for BERT/BART, tab-transformer-pytorch for TabTransformer, remasker for ReMasker. Read .cache/fused/*.json, mask known fields, predict, measure MAE/RMSE vs NASA ground truth. Output comparison markdown document. |
</phase_requirements>

## Standard Stack

### Core (C++ -- inference integration and parameter mapping)
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| nlohmann/json | v3.11.3 | JSON parsing for AI responses and cache | Already in project, used everywhere |
| spdlog | v1.14.1 | Structured logging for inference pipeline | Already in project, LOG_* macros |
| Catch2 | v3.5.2 | Unit testing for new integration code | Already in project from Phase 1 |
| libcurl | 8.5.0 | HTTP for Bedrock (via AWS CLI) | Already in project |

### Core (Python -- ML benchmarking harness)
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| torch | 2.x | PyTorch for model training/inference | Foundation for all 4 models |
| transformers | 5.x | HuggingFace for BERT and BART models | Industry standard for transformer models |
| tab-transformer-pytorch | 0.6.1 | TabTransformer implementation | Maintained PyTorch implementation by lucidrains |
| scikit-learn | 1.x | Metrics (MAE, RMSE, R2), data splitting | Standard ML evaluation toolkit |
| pandas | 2.x | DataFrame operations for tabular data | Standard for structured data manipulation |
| numpy | 1.x/2.x | Numerical operations | Foundation for all scientific Python |

### Supporting (Python -- ML benchmarking)
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| timm | latest | Required dependency for ReMasker | When running ReMasker imputation |
| hyperimpute | latest | Required dependency for ReMasker | When running ReMasker imputation |
| matplotlib | 3.x | Visualization of comparison results | For generating benchmark charts |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| ReMasker (masked autoencoder) | MICE/IterativeImputer | MICE is simpler but less accurate on complex tabular patterns |
| tab-transformer-pytorch | FT-Transformer | FT-Transformer adds numerical feature tokenization but heavier setup |
| AWS CLI for Bedrock | AWS SDK C++ | SDK is more robust but heavy dependency; CLI is already working |

**Python Installation:**
```bash
cd experiments/ml
python -m venv .venv
source .venv/bin/activate
pip install torch transformers tab-transformer-pytorch scikit-learn pandas numpy matplotlib timm hyperimpute
```

## Architecture Patterns

### Recommended Project Structure
```
src/
├── ai/
│   ├── InferenceEngine.hpp/cpp    # EXISTING: fills missing params via Bedrock
│   ├── BedrockClient.hpp/cpp      # EXISTING: AWS Bedrock integration
│   └── PromptTemplates.hpp        # EXISTING: structured prompts
├── data/
│   ├── DataFusionEngine.hpp/cpp   # MODIFY: add InferenceEngine integration
│   ├── ExoplanetData.hpp/cpp      # MODIFY: extend toJson/fromJson for AI fields
│   └── CacheManager.hpp/cpp       # EXISTING: cache enriched records
├── render/
│   └── PlanetParams.hpp/cpp       # MODIFY: implement full fromObservations()
experiments/
└── ml/
    ├── requirements.txt           # NEW: Python dependencies
    ├── benchmark.py               # NEW: Main benchmark orchestrator
    ├── data_loader.py             # NEW: Load .cache/fused/*.json
    ├── models/
    │   ├── bert_classifier.py     # NEW: BERT fine-tuning for classification
    │   ├── bart_generator.py      # NEW: BART for structured parameter generation
    │   ├── tab_transformer.py     # NEW: TabTransformer for numerical imputation
    │   └── remasker_imputer.py    # NEW: ReMasker masked autoencoder imputation
    ├── metrics.py                 # NEW: MAE, RMSE, R2 evaluation
    └── results/                   # NEW: Output comparison documents
```

### Pattern 1: Pipeline Integration (InferenceEngine into DataFusionEngine)
**What:** Add InferenceEngine as a member of DataFusionEngine::Impl, call after merge
**When to use:** Every fetchAndFuseSync call
**Example:**
```cpp
// In DataFusionEngine.cpp - modified fetchAndFuseSync()
struct DataFusionEngine::Impl {
    NasaApiClient nasa;
    OecClient oec;
    GaiaClient gaia;
    CdsClient cds;
    CacheManager cache{"fused"};
    InferenceEngine inference;  // NEW
};

ExoplanetData DataFusionEngine::fetchAndFuseSync(const std::string& planetName) {
    // Check cache first (includes AI-enriched data)
    auto cached = m_impl->cache.retrieve(planetName);
    if (cached.has_value()) {
        return *cached;  // Skip fusion AND inference
    }

    // ... existing fetch + merge code ...
    ExoplanetData fused = mergeExoplanetData(sources);

    // NEW: AI inference gap-filling
    if (m_impl->inference.isAvailable()) {
        fused = m_impl->inference.fillMissingParametersSync(fused);
    } else {
        applyDeterministicDefaults(fused);  // Fallback
        LOG_INFO("AI unavailable for {}, using rule-based defaults", planetName);
    }

    // Cache the enriched record
    m_impl->cache.store(planetName, fused);
    return fused;
}
```

### Pattern 2: Deterministic Fallback (Rule-Based Defaults)
**What:** When Bedrock is unavailable, apply physics-based defaults so planets are still renderable
**When to use:** When InferenceEngine::isAvailable() returns false or inference fails
**Example:**
```cpp
void DataFusionEngine::applyDeterministicDefaults(ExoplanetData& data) {
    // Only fill truly missing fields -- never overwrite measured data
    if (!data.albedo.hasValue()) {
        // Default albedo based on equilibrium temperature
        if (data.equilibrium_temp_k.hasValue()) {
            double T = data.equilibrium_temp_k.value;
            if (T > 700) data.albedo = MeasuredValue<double>(0.75, DataSource::CALCULATED); // Venus-like
            else if (T > 250) data.albedo = MeasuredValue<double>(0.3, DataSource::CALCULATED);  // Earth-like
            else data.albedo = MeasuredValue<double>(0.5, DataSource::CALCULATED); // Ice world
        }
    }
    // Similar for surface_pressure_atm, atmosphere_composition, etc.
}
```

### Pattern 3: Physics-Based Parameter Mapping (fromObservations upgrade)
**What:** Full ExoplanetData -> CelestialBodyParams conversion with physics models
**When to use:** Converting enriched ExoplanetData to render-ready parameters
**Example:**
```cpp
CelestialBodyParams CelestialBodyParams::fromObservations(const ExoplanetData& data) {
    CelestialBodyParams params;
    params.name = data.name;

    // Body type: deterministic from radius (user decision)
    float R = data.radius_earth.hasValue() ? data.radius_earth.value : 1.0f;
    if (R < 2.0f) params.bodyType = CelestialBodyType::Terrestrial;
    else if (R <= 6.0f) params.bodyType = CelestialBodyType::IceGiant;
    else params.bodyType = CelestialBodyType::GasGiant;

    // Rayleigh scattering from atmosphere composition
    if (data.atmosphere_composition.hasValue()) {
        params.atmosphere.rayleighCoeff = computeRayleighFromComposition(
            data.atmosphere_composition.value);
    }

    // Temperature-modulated surface for terrestrials
    if (params.bodyType == CelestialBodyType::Terrestrial) {
        mapTerrestrialSurface(params, data);
    } else {
        mapGasGiantParams(params, data);
    }

    return params;
}
```

### Pattern 4: ML Benchmark Methodology (Mask-Predict-Evaluate)
**What:** Mask known fields from complete records, predict with each model, measure error
**When to use:** Python experiments comparing model accuracy
**Example:**
```python
import pandas as pd
import numpy as np
from sklearn.metrics import mean_absolute_error, mean_squared_error

def evaluate_model(model, complete_records, fields_to_mask):
    """Mask known fields, predict, measure error vs ground truth."""
    results = []
    for record in complete_records:
        ground_truth = {f: record[f] for f in fields_to_mask}
        masked = record.copy()
        for f in fields_to_mask:
            masked[f] = np.nan

        predicted = model.predict(masked)

        for field in fields_to_mask:
            results.append({
                'field': field,
                'true': ground_truth[field],
                'predicted': predicted[field],
                'error': abs(ground_truth[field] - predicted[field])
            })

    df = pd.DataFrame(results)
    return {
        'mae': mean_absolute_error(df['true'], df['predicted']),
        'rmse': np.sqrt(mean_squared_error(df['true'], df['predicted'])),
    }
```

### Anti-Patterns to Avoid
- **Overwriting measured data with AI values:** The InferenceEngine must check `hasValue()` before setting ANY field. Only fill NaN fields.
- **Coupling Python experiments to C++ build:** The Python harness must be fully standalone -- own venv, own requirements.txt, reads JSON files from disk. No pybind11 or CMake integration in this phase.
- **Hardcoding body type thresholds in multiple places:** Use the user's thresholds (2/6 R_Earth) in ONE authoritative location, not scattered across files.
- **Making AI inference synchronous-blocking in the main loop:** The `fetchAndFuseSync()` approach is fine for data pipeline calls, but never call from the render thread. Use async wrappers for UI-triggered fetches.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| ML model training/eval | Custom training loops from scratch | HuggingFace Trainer + sklearn metrics | Handles batching, evaluation, logging automatically |
| Tabular data imputation | Custom neural imputer | ReMasker (scikit-learn compatible API) | Peer-reviewed, handles missing data natively |
| Tabular transformer | Custom attention on tabular data | tab-transformer-pytorch | Tested implementation matching the paper |
| Rayleigh scattering physics | Approximate color lookups | Physics formula: beta proportional to 1/lambda^4 with composition-dependent refractive index | Produces physically accurate sky colors for any composition |
| JSON cache read in Python | Custom JSON parser | pandas.read_json or json.load | Standard, handles edge cases |

**Key insight:** The C++ AI integration is almost entirely about WIRING -- the InferenceEngine, BedrockClient, PromptTemplates, and MeasuredValue tracking are all built. The real engineering challenge is the physics-based parameter mapping (fromObservations) and the Python ML harness.

## Common Pitfalls

### Pitfall 1: AI-Inferred Values Overwriting Measured Data
**What goes wrong:** InferenceEngine fills a field that already has a NASA-measured value, destroying ground truth.
**Why it happens:** `fillMissingParametersSync()` already checks `hasValue()` for atmosphere fields but the pattern must be enforced for ALL fields consistently.
**How to avoid:** Guard every `applyInferredValues` call with `if (!target.hasValue())` check. Add a unit test that constructs ExoplanetData with some measured values and verifies inference never overwrites them.
**Warning signs:** Tests show DataSource changing from NASA_TAP to AI_INFERRED for a field that had a measured value.

### Pitfall 2: Cache Storing Partial Results
**What goes wrong:** If AI inference fails partway through, the cache stores a record with some AI fields and some gaps. Next lookup returns this partial record and skips inference entirely.
**Why it happens:** The cache stores after the inference call regardless of whether inference succeeded.
**How to avoid:** Only cache records that are "complete enough" -- either all AI fields filled, or fallback defaults applied. Never cache a record that went through inference but failed.
**Warning signs:** Repeat lookups for the same planet returning fewer fields than initial lookups.

### Pitfall 3: fromObservations Signature Breaking Change
**What goes wrong:** Current `fromObservations()` takes 5 scalar parameters. Upgrading it to take `ExoplanetData` changes the signature and breaks any existing callers.
**Why it happens:** The function signature was designed before the data pipeline existed.
**How to avoid:** Add a NEW overload `fromObservations(const ExoplanetData&)` that coexists with the old signature. Deprecate the old one but don't remove it yet.
**Warning signs:** Compilation failures in existing code that calls the old signature.

### Pitfall 4: Python Harness Can't Find Cache Files
**What goes wrong:** Python scripts look for `.cache/fused/` relative to their own directory instead of the project root.
**Why it happens:** Working directory differs between running from `experiments/ml/` vs project root.
**How to avoid:** Accept cache path as a command-line argument with a sensible default (`../../.cache/fused/` or configurable).
**Warning signs:** FileNotFoundError when running benchmarks from experiments/ml/ directory.

### Pitfall 5: Rayleigh Coefficient Scale Mismatch
**What goes wrong:** Computed Rayleigh scattering coefficients are orders of magnitude too large or small for the renderer.
**Why it happens:** Physics formulas give values in SI units (m^-1) while the renderer uses arbitrary-scale float coefficients like `{5.5e-6f, 13.0e-6f, 22.4e-6f}`.
**How to avoid:** Normalize computed coefficients to the renderer's expected scale by using Earth's atmosphere as reference (the existing Earth preset values serve as calibration). Map relative to Earth: `coeff = earth_coeff * (composition_factor / earth_composition_factor)`.
**Warning signs:** Sky renders as pure black (coefficients too small) or pure white (too large).

### Pitfall 6: BERT/BART Misapplied to Numerical Prediction
**What goes wrong:** Using BERT/BART for direct numerical regression produces poor results because they're designed for text.
**Why it happens:** These are language models, not numerical regressors.
**How to avoid:** BERT should be used for classification tasks (planet type, biome). BART for structured text generation (atmosphere composition descriptions). TabTransformer and ReMasker for numerical imputation. Match model type to task type.
**Warning signs:** Huge MAE/RMSE from BERT/BART on numerical fields compared to TabTransformer/ReMasker.

## Code Examples

### Rayleigh Scattering from Atmosphere Composition
```cpp
// Map atmosphere composition string to Rayleigh scattering coefficients
// Reference: Earth N2/O2 -> {5.5e-6, 13.0e-6, 22.4e-6}
// Physics: beta proportional to (n^2-1)^2 / (N * lambda^4)
// Different gases have different refractive indices, changing the scattering profile
glm::vec3 computeRayleighFromComposition(const std::string& atmosphereJson) {
    // Earth-like N2/O2 baseline
    glm::vec3 earthCoeff = {5.5e-6f, 13.0e-6f, 22.4e-6f};

    try {
        auto comp = nlohmann::json::parse(atmosphereJson);

        float n2_frac = comp.value("N2", 0.0f);
        float o2_frac = comp.value("O2", 0.0f);
        float co2_frac = comp.value("CO2", 0.0f);
        float h2_frac = comp.value("H2", 0.0f);
        float he_frac = comp.value("He", 0.0f);

        // CO2-thick atmosphere: shift toward red scattering (Venus orange)
        // Refractive index of CO2 > N2 -> stronger scattering at all wavelengths
        // but also more Mie scattering from dense atmosphere
        if (co2_frac > 50.0f) {
            return {10.0e-6f, 6.0e-6f, 3.0e-6f};  // Orange-tinted (Venus analog)
        }

        // H2/He dominant: slightly blue-shifted (Neptune/Uranus)
        if (h2_frac + he_frac > 50.0f) {
            return {3.0e-6f, 8.0e-6f, 15.0e-6f};  // Pale blue
        }

        // N2/O2 dominant: Earth-like blue
        if (n2_frac + o2_frac > 50.0f) {
            return earthCoeff;
        }

        // Default: scaled from Earth
        return earthCoeff;
    } catch (...) {
        return earthCoeff;  // Fallback to Earth-like
    }
}
```

### Multi-Factor Terrestrial Surface Mapping
```cpp
void mapTerrestrialSurface(CelestialBodyParams& params, const ExoplanetData& data) {
    float T = data.equilibrium_temp_k.hasValue() ? data.equilibrium_temp_k.value : 288.0f;
    float pressure = data.surface_pressure_atm.hasValue() ? data.surface_pressure_atm.value : 1.0f;

    // Greenhouse effect: thick CO2 atmosphere raises effective temperature
    float effectiveT = T;
    if (data.atmosphere_composition.hasValue()) {
        try {
            auto comp = nlohmann::json::parse(data.atmosphere_composition.value);
            float co2 = comp.value("CO2", 0.0f);
            if (co2 > 90.0f && pressure > 50.0f) {
                effectiveT = T * 1.8f;  // Venus-like greenhouse
            } else if (co2 > 10.0f) {
                effectiveT = T * (1.0f + co2 / 500.0f);  // Moderate greenhouse
            }
        } catch (...) {}
    }

    // Surface classification based on effective temperature
    if (effectiveT > 700.0f) {
        // Venus/lava world
        params.terrain.seaLevel = 0.0f;
        params.terrain.volcanicActivity = 0.6f;
        params.biome.vegetationDensity = 0.0f;
        params.atmosphere.density = pressure;
        params.atmosphere.hazeStrength = 0.9f;
        params.atmosphere.hazeColor = {0.9f, 0.8f, 0.5f};
        params.colors.lava = {1.0f, 0.4f, 0.1f};
        params.colors.volcanic = {0.2f, 0.15f, 0.1f};
    } else if (effectiveT > 350.0f) {
        // Hot desert
        params.terrain.seaLevel = 0.1f;
        params.biome.globalMoisture = 0.1f;
        params.biome.vegetationDensity = 0.05f;
        params.colors.desert = {0.85f, 0.65f, 0.4f};
    } else if (effectiveT > 250.0f) {
        // Habitable zone -- consider ocean coverage from AI
        float ocean = data.ocean_coverage_fraction.hasValue()
            ? data.ocean_coverage_fraction.value : 0.5f;
        params.terrain.seaLevel = ocean * 0.8f;  // Scale to terrain param range
        params.biome.globalMoisture = ocean * 0.8f;
        params.biome.vegetationDensity = std::max(0.0f, (effectiveT - 250.0f) / 100.0f * 0.7f);
    } else if (effectiveT > 150.0f) {
        // Cold but possibly habitable (Mars-like)
        params.terrain.seaLevel = 0.0f;
        params.biome.polarIceExtent = 0.3f;
        params.biome.vegetationDensity = 0.0f;
        params.terrain.craterDensity = 0.3f;
        params.atmosphere.hazeStrength = 0.4f;
    } else {
        // Frozen world
        params.terrain.seaLevel = 0.0f;
        params.biome.polarIceExtent = 1.0f;
        params.biome.vegetationDensity = 0.0f;
        params.colors.lowlandGrass = {0.8f, 0.85f, 0.9f};
    }
}
```

### Gas Giant Parameter Mapping
```cpp
void mapGasGiantParams(CelestialBodyParams& params, const ExoplanetData& data) {
    float T = data.equilibrium_temp_k.hasValue() ? data.equilibrium_temp_k.value : 165.0f;
    float R = data.radius_earth.hasValue() ? data.radius_earth.value : 11.2f;
    float M = data.mass_earth.hasValue() ? data.mass_earth.value : 317.8f;

    // Band count scales with radius (larger planets = more bands)
    params.gasGiant.bandCount = std::clamp(R / 11.2f * 12.0f, 4.0f, 20.0f);

    // Hot Jupiters: muted bands, intense storms
    if (T > 1000.0f) {
        params.gasGiant.bandContrast = 0.15f;  // Muted
        params.gasGiant.stormFrequency = 0.3f;  // Intense
        params.gasGiant.bandColor1 = {0.4f, 0.35f, 0.3f};  // Dark
        params.gasGiant.bandColor2 = {0.3f, 0.25f, 0.2f};
    } else {
        // Cold gas giants: Jupiter-like high contrast
        params.gasGiant.bandContrast = 0.35f;
        params.gasGiant.stormFrequency = 0.15f;
        params.gasGiant.bandColor1 = {0.85f, 0.75f, 0.65f};
        params.gasGiant.bandColor2 = {0.65f, 0.5f, 0.4f};
    }

    // Great spot probability scales with size
    float spotProb = std::clamp(M / 317.8f, 0.0f, 1.0f);
    params.gasGiant.greatSpotSize = (spotProb > 0.5f) ? 0.12f * spotProb : 0.0f;
}
```

### Python Data Loader for Cached JSON
```python
import json
import os
import pandas as pd
import numpy as np
from pathlib import Path

def load_cached_exoplanets(cache_dir: str = "../../.cache/fused/") -> pd.DataFrame:
    """Load cached fused exoplanet JSON records into a DataFrame."""
    records = []
    cache_path = Path(cache_dir)

    if not cache_path.exists():
        raise FileNotFoundError(f"Cache directory not found: {cache_path.resolve()}")

    for json_file in cache_path.glob("*.json"):
        with open(json_file) as f:
            data = json.load(f)

        record = {
            'name': data.get('name', ''),
            'mass_earth': data.get('physical', {}).get('mass_earth', np.nan),
            'radius_earth': data.get('physical', {}).get('radius_earth', np.nan),
            'density_gcc': data.get('physical', {}).get('density_gcc', np.nan),
            'equilibrium_temp_k': data.get('physical', {}).get('equilibrium_temp_k', np.nan),
            'orbital_period_days': data.get('orbital', {}).get('period_days', np.nan),
            'semi_major_axis_au': data.get('orbital', {}).get('semi_major_axis_au', np.nan),
            'eccentricity': data.get('orbital', {}).get('eccentricity', np.nan),
            'planet_type': data.get('classification', {}).get('planet_type', ''),
            'star_temp_k': data.get('host_star', {}).get('effective_temp_k', np.nan),
        }
        records.append(record)

    return pd.DataFrame(records)
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Full fine-tuning of LLMs for domain tasks | PEFT/LoRA for efficient adaptation | 2023-2024 | 100x less compute for comparable results |
| Simple lookup tables for planet rendering | Physics-based procedural generation | Ongoing | Much more realistic and varied planet appearances |
| MICE/KNN for tabular imputation | Masked autoencoder (ReMasker) | 2023 | Better handling of complex feature interactions |
| Manual tree-based models for tabular data | TabTransformer (attention on categoricals) | 2021+ | Competitive with gradient boosting on tabular |
| Claude Sonnet 3.5 on Bedrock | Claude Sonnet 4.5 on Bedrock | 2025 | Better structured output, more reliable JSON responses |

**Deprecated/outdated:**
- The project's TESTING.md mentions Google Test but Phase 1 actually adopted Catch2 v3.5.2 -- all future tests should use Catch2
- The `fromObservations()` 5-parameter signature is superseded by the new `ExoplanetData`-based overload

## Open Questions

1. **Single vs. Dual Bedrock Calls**
   - What we know: Current InferenceEngine makes two separate calls (`inferAtmosphere` + `inferRenderHints`). Consolidating into one call reduces latency but increases prompt complexity.
   - What's unclear: Whether a single combined prompt produces worse results than two focused prompts.
   - Recommendation: Keep two separate calls for now (existing pattern works), but add a timing measurement. If total latency exceeds 10s, consolidate later. **(Claude's Discretion)**

2. **Retry vs. Immediate Fallback**
   - What we know: Network calls to Bedrock can fail transiently.
   - What's unclear: How frequently Bedrock fails vs. how much latency a retry adds.
   - Recommendation: One retry with 2-second timeout, then fallback to deterministic defaults. This balances reliability with responsiveness. **(Claude's Discretion)**

3. **ML Benchmark Dataset Size**
   - What we know: NASA Exoplanet Archive has ~5700 confirmed exoplanets; not all have complete measurements.
   - What's unclear: How many have enough fields for meaningful benchmarking.
   - Recommendation: Start with all planets that have at least mass, radius, AND temperature measured (likely 500-1000). This gives enough data for statistical significance without requiring pre-fetching thousands. **(Claude's Discretion)**

4. **ExoplanetData JSON Serialization Completeness**
   - What we know: `toJson()` and `fromJson()` exist but may not serialize ALL fields (atmospheric params, AI reasoning, etc.)
   - What's unclear: Whether the current serialization captures enough fields for Python harness to read
   - Recommendation: Audit and extend toJson/fromJson to include ALL atmospheric and rendering fields before the Python harness consumes them. This is a prerequisite for R2.3.

## Sources

### Primary (HIGH confidence)
- Codebase analysis: `src/ai/InferenceEngine.cpp`, `src/ai/BedrockClient.cpp`, `src/ai/PromptTemplates.hpp` -- full implementation reviewed
- Codebase analysis: `src/data/DataFusionEngine.cpp`, `src/data/ExoplanetData.hpp/cpp` -- integration points identified
- Codebase analysis: `src/render/PlanetParams.hpp/cpp` -- all preset planets and fromObservations reviewed
- Codebase analysis: `tests/CMakeLists.txt` -- Catch2 v3.5.2 test framework confirmed
- PyPI: tab-transformer-pytorch v0.6.1 -- confirmed active, MIT license
- GitHub: ReMasker (tydusky/remasker) -- scikit-learn compatible API confirmed

### Secondary (MEDIUM confidence)
- HuggingFace docs: BART model documentation -- seq2seq architecture for structured generation
- arXiv 2309.13793: ReMasker -- masked autoencoder for tabular imputation
- arXiv 2012.06678: TabTransformer -- attention on categorical features
- Scratchapixel: Rayleigh scattering formulas -- physics reference for atmosphere rendering

### Tertiary (LOW confidence)
- H2O wiki: BERT overview -- general reference, not specific to tabular data
- PEFT/Adapter blog: Parameter-efficient fine-tuning patterns -- relevant for BERT adaptation but specifics may vary

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - All C++ libraries already in project, Python libraries are well-established with active maintenance
- Architecture: HIGH - Integration points are clearly identified in existing code, patterns follow established project conventions
- Physics mapping: MEDIUM - Rayleigh scattering formulas are well-established physics, but tuning coefficients to the renderer's scale requires calibration against existing presets
- ML benchmarking: MEDIUM - Model capabilities are well-documented but specific performance on exoplanet data is unproven
- Pitfalls: HIGH - Based on direct code analysis and domain experience

**Research date:** 2026-02-28
**Valid until:** 2026-03-28 (30 days -- stable domain, no rapidly changing APIs)
