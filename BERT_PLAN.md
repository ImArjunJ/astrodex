# BERT Inference Plan — Exoplanet Missing-Data Imputation

> **Goal**: Train a double-headed BERT on the NASA Exoplanet Archive to predict missing
> physical parameters. The model slots in as a drop-in replacement (or augmentation) for
> `InferenceEngine::fillMissingParametersSync()`, with no AWS dependency.
>
> **Fallback**: The existing Claude/Bedrock pipeline remains fully intact. If BERT training
> doesn't converge in time, the app degrades gracefully — nothing breaks.

---

## How It Fits the Existing Pipeline

```
User types planet name
        │
        ▼
NasaApiClient → ExoplanetData (sparse — many fields are NaN)
        │
        ▼                   ← BERT slots in here
InferenceEngine::fillMissingParametersSync()
  └─ [NEW] BertImputer::predict(ExoplanetData&)   fills numerical NaNs
  └─ [EXISTING] inferAtmosphere() → Claude call 1 (atmosphere only)
        │
        ▼
ExoplanetMapper::toPlanetParams() → renderer
```

The BERT model handles **physical parameter imputation** (mass, radius, temperature,
orbital period, etc.). Claude keeps its job of inferring **visual/atmospheric render params**
from the now-complete physical picture. The two are complementary, not competing.

---

## Phases

### Phase 1 — Data Acquisition & Exploration
**Goal**: Understand what the NASA archive actually looks like before writing any model code.

- [ ] **1.1** Pull the full NASA Exoplanet Archive via TAP
  - Use the existing `NasaApiClient` or query directly:
    ```
    https://exoplanetarchive.ipac.caltech.edu/TAP/sync?query=SELECT+*+FROM+ps&format=json
    ```
  - Target table: `ps` (planetary systems — one row per planet per reference)
  - Save locally as `data/nasa_archive.csv`

- [ ] **1.2** Identify the target feature set
  - Key numerical columns to impute (regression head targets):
    - `pl_orbper` — orbital period (days)
    - `pl_orbsmax` — semi-major axis (AU)
    - `pl_orbeccen` — eccentricity
    - `pl_bmassj` — planet mass (Jupiter masses)
    - `pl_radj` — planet radius (Jupiter radii)
    - `pl_dens` — planet density (g/cm³)
    - `pl_eqt` — equilibrium temperature (K)
    - `pl_insol` — insolation flux (Earth flux)
    - `st_teff` — stellar effective temperature (K)
    - `st_mass` — stellar mass (solar masses)
    - `st_rad` — stellar radius (solar radii)
    - `st_lum` — stellar luminosity (log solar)
    - `st_met` — stellar metallicity [Fe/H]
  - Key categorical columns (classification head targets):
    - `pl_type` / planet type (hot Jupiter, super-Earth, etc.) — derived
    - `discoverymethod` — transit, radial velocity, imaging, etc.
    - `st_spectype` — host star spectral class (F, G, K, M...)

- [ ] **1.3** Compute a missingness matrix
  - For each column: what fraction of rows have a value?
  - This tells you which columns are worth imputing vs. too sparse to learn from
  - Aim for columns with >20% coverage as training signal

- [ ] **1.4** Plot correlation structure
  - Scatter: `pl_radj` vs `pl_bmassj`, `pl_eqt` vs `pl_orbsmax`, etc.
  - Confirms the physical correlations BERT needs to learn actually exist in the data

---

### Phase 2 — Custom Tokenizer
**Goal**: Build the vocabulary and encoding scheme for tabular exoplanet rows.

- [ ] **2.1** Define the feature vocabulary
  - One token per feature name: `[pl_radj]`, `[pl_orbper]`, `[st_teff]`, etc.
  - Special tokens: `[CLS]`, `[SEP]`, `[MASK]`, `[PAD]`, `[UNK]`
  - Total feature-name tokens: ~15–20 for the core set

- [ ] **2.2** Build the value encoding scheme
  - **Numerical values → log-binned tokens**
    - For each numerical feature, compute the log₁₀ range across all known values
    - Divide into N=200 equal bins → tokens `[val_000]` through `[val_199]`
    - Each feature gets its own bin boundaries (stored in `tokenizer_config.json`)
    - Log-scale is physically natural: mass spans ~4 orders of magnitude
  - **Categorical values → direct vocab tokens**
    - `[transit]`, `[radial_velocity]`, `[imaging]`, `[spectype_G]`, etc.
  - **Out-of-range values** → `[UNK]`
  - **Missing/unknown values** → `[MASK]` (at training time, also used for random masking)

- [ ] **2.3** Define the sequence format
  ```
  [CLS] [pl_radj] [val_112] [pl_orbper] [MASK] [st_teff] [val_089] [pl_bmassj] [MASK] [SEP]
  ```
  - Each feature is a **pair**: `(feature_name_token, value_token)`
  - Order: fix a canonical column order (same every row) for simplicity
  - Max sequence length: 2 × N_features + 2 special tokens ≈ 42 tokens — tiny

- [ ] **2.4** Implement `ExoplanetTokenizer` (Python class)
  - `tokenizer.encode(row: dict) -> List[int]` — row is a dict of `{col: value}`
  - `tokenizer.decode_value(token_id: int, feature: str) -> float` — inverse bin lookup
  - `tokenizer.save(path)` / `tokenizer.load(path)` — persist bin boundaries
  - Scrub checklist:
    - [ ] All values finite? (drop inf/NaN from bin boundary computation)
    - [ ] Bin boundaries monotonically increasing per feature?
    - [ ] No feature vocab collisions with value vocab?
    - [ ] Round-trip test: `encode → decode` recovers value within bin width tolerance

---

### Phase 3 — Model Architecture
**Goal**: Define the double-headed BERT in PyTorch.

- [ ] **3.1** Embedding layer
  - Token embedding: `nn.Embedding(vocab_size, d_model)` — vocab is ~300 tokens
  - Positional embedding: learned, one per sequence position (not sinusoidal — feature
    order is arbitrary so we want the model to learn position rather than assume structure)
  - Feature-type embedding (optional but useful): separate embedding that identifies
    *which feature* the value token belongs to, shared across the name/value pair

- [ ] **3.2** BERT encoder backbone
  - For hackathon scope: **4–6 transformer layers, d_model=128, 4 attention heads**
  - Full-size BERT (12 layers, d_model=768) is overkill — your vocabulary has ~300 tokens
    and ~5,500 training rows. Small model, fast to train.
  - Use `torch.nn.TransformerEncoder` or `transformers.BertConfig` with custom small dims

- [ ] **3.3** Regression head (Head 1)
  - Input: encoder output at each `[MASK]` value-token position
  - Output: scalar (the bin index as a float, treated as regression target)
  - Architecture: `Linear(d_model, 64) → ReLU → Linear(64, 1)`
  - Loss: **MSE on the bin index** (equivalent to log-scale regression)
  - At inference: round output to nearest bin, look up float value via bin boundaries

- [ ] **3.4** Classification head (Head 2)
  - Input: encoder output at each `[MASK]` categorical-token position
  - Output: logits over categorical vocab size
  - Architecture: `Linear(d_model, n_categories)` — simple, one per categorical feature
  - Loss: cross-entropy

- [ ] **3.5** Combined loss
  ```python
  loss = loss_regression + lambda_cls * loss_classification
  ```
  - Start with `lambda_cls = 1.0`, tune if one head dominates

---

### Phase 4 — Training
**Goal**: Train the model on the archive, validate on held-out rows.

- [ ] **4.1** Dataset class
  - `ExoplanetDataset(csv_path, tokenizer, mask_prob=0.15)`
  - For each row: randomly mask 15% of *known* values (MLM-style)
  - Return `(input_ids, attention_mask, target_ids, target_positions)`
  - Split: 80% train / 10% val / 10% test — stratify by planet type if possible

- [ ] **4.2** Training loop
  - Optimizer: AdamW, lr=1e-4, weight decay=0.01
  - Scheduler: linear warmup for 10% of steps, then cosine decay
  - Batch size: 64 (dataset is small, large batches help stability)
  - Epochs: 50–100 — the dataset is small, watch for overfitting after epoch ~30
  - Log: train loss, val loss, per-feature reconstruction MAE

- [ ] **4.3** Validation metrics
  - Per-feature **Mean Absolute Error** on held-out masked values
  - Compare against two baselines:
    - **Median imputation** (column median — simplest possible baseline)
    - **Solar analog matching** (the existing logic in `ExoplanetMapper`)
  - BERT should beat both baselines on >80% of features to justify integration

- [ ] **4.4** Training infrastructure
  - Everything runs in a single `train.py` script, no cluster needed
  - Dataset is ~5,500 rows × ~20 features — trains in minutes on CPU, seconds on GPU
  - Save best checkpoint by val loss: `checkpoints/bert_imputer_best.pt`

---

### Phase 5 — Integration
**Goal**: Wire the trained model into the existing C++ pipeline via a Python bridge.

- [ ] **5.1** Python inference wrapper
  - `BertImputer` class:
    ```python
    imputer = BertImputer("checkpoints/bert_imputer_best.pt", "tokenizer_config.json")
    filled = imputer.predict({"pl_radj": 1.2, "st_teff": 5800, ...})
    # returns {"pl_bmassj": 0.87, "pl_orbper": 15.3, ...} for missing fields only
    ```
  - Respects the **fill-empty-slots philosophy** from the existing code: never overrides
    a measured value, only fills `NaN` slots

- [ ] **5.2** Bridge to C++ (two options — pick one for hackathon)
  - **Option A — subprocess call** (fastest to wire up):
    - C++ `InferenceEngine` spawns `python bert_imputer.py --input '{json}'`
    - Reads stdout JSON, parses with nlohmann/json
    - Already have a similar pattern in `BedrockClient.cpp` with AWS CLI
  - **Option B — pybind11** (cleaner, already planned as Phase 7 in CMakeLists):
    - Expose `BertImputer::predict` as a C++ callable
    - Requires building the Python SDK first, then linking back in
  - **Recommendation**: Option A for hackathon, Option B post-hackathon

- [ ] **5.3** Update `InferenceEngine::fillMissingParametersSync()`
  - Before calling `inferAtmosphere()`, call the BERT imputer
  - BERT fills physical params → Claude now gets a richer input for atmosphere inference
  - Add `inference_source` tag to each field (`"bert"` vs `"claude"` vs `"physics"`)
    for the existing per-field accuracy log

- [ ] **5.4** Confidence scores
  - The regression head output (distance from nearest bin boundary) gives a natural
    confidence proxy — values landing near bin centres are more certain
  - Map to `[0, 1]` and pass as `_confidence` alongside each imputed value
  - Existing confidence-weighted lerp logic in `ExoplanetMapper` handles this already

---

### Phase 6 — Validation & Demo Prep
**Goal**: Prove it works on known planets before demoing with exoplanets.

- [ ] **6.1** Run known-planet validation
  - Earth, Mars, Jupiter, Saturn — deliberately blank out known fields, run BERT, compare
  - This mirrors the existing `Known-planet validation` pipeline in the app
  - Target: <20% MAE on mass and radius for solar system planets

- [ ] **6.2** End-to-end demo flow
  - Query `Kepler-452 b` (has radius measured, mass unknown)
  - BERT predicts mass → Claude gets mass context → richer atmosphere inference
  - Renderer shows a physically grounded planet, not just a guess

- [ ] **6.3** Hackathon talking points
  - "Unlike Claude which reasons from text, BERT learned the physical correlations
    directly from 5,500 confirmed exoplanets — it knows a 3-day orbit around a G-star
    almost certainly means a hot Jupiter, because it's seen hundreds of them"
  - "Two inference layers, complementary: BERT for physics, Claude for visuals"
  - "No AWS needed for the physical imputation — runs fully offline"

---

## File Layout

```
cooking/
├── BERT_PLAN.md              ← this file
├── TODO.md                   ← existing pipeline tasks
│
├── data/
│   └── nasa_archive.csv      ← Phase 1: downloaded archive
│
├── notebooks/
│   ├── 01_exploration.ipynb  ← Phase 1: missingness + correlations
│   └── 02_tokenizer_dev.ipynb← Phase 2: tokenizer scrub + round-trip tests
│
├── src_ml/
│   ├── tokenizer.py          ← Phase 2: ExoplanetTokenizer
│   ├── dataset.py            ← Phase 4: ExoplanetDataset
│   ├── model.py              ← Phase 3: double-headed BERT
│   ├── train.py              ← Phase 4: training loop
│   └── imputer.py            ← Phase 5: BertImputer inference wrapper
│
├── checkpoints/
│   └── bert_imputer_best.pt  ← Phase 4 output
│
└── tokenizer_config.json     ← Phase 2 output: bin boundaries + vocab
```

---

## Dependencies

```
torch>=2.0
transformers>=4.35   # optional — can use pure torch TransformerEncoder instead
pandas
numpy
scikit-learn         # for train/val split, baseline comparison
matplotlib           # for Phase 1 exploration plots
requests             # for TAP API pull if not using existing NasaApiClient
```

---

## Current Status

| Phase | Status |
|---|---|
| 1 — Data Acquisition | ✅ complete |
| 2 — Custom Tokenizer | ✅ complete |
| 3 — Model Architecture | ✅ complete |
| 4 — Training | ✅ complete |
| 5 — Integration | ✅ complete |
| 6 — Validation & Demo | ✅ complete |

---

## Phase 1 Findings (2026-02-28)

**Dataset**: 39,443 raw rows → **6,128 unique planets** after best-coverage deduplication.

### Feature coverage

| Column | Fill Rate | Notes |
|---|---|---|
| `discoverymethod` | 100% | all transit/RV |
| `st_mass` | 99% | nearly complete |
| `pl_orbper` | 95% | |
| `pl_orbsmax` | 94% | |
| `st_teff` | 92% | |
| `st_rad` | 89% | |
| `st_met` | 87% | |
| `pl_orbeccen` | 83% | |
| `pl_radj` | 75% | |
| `pl_orbincl` | 71% | |
| `pl_eqt` | 71% | |
| `pl_insol` | 57% | |
| `pl_bmassj` | 48% | **primary imputation target** |
| `st_lum` | 28% | |
| `st_spectype` | 27% | |
| `pl_dens` | 20% | borderline — keep but watch |

All 16 feature columns pass the >20% usability threshold.

### Key correlations (Pearson r on log-scaled values)

These are the physical relationships the BERT will learn:

| Pair | r | Implication |
|---|---|---|
| `pl_eqt` ↔ `pl_insol` | 0.996 | near-redundant; keep both, BERT can exploit |
| `pl_orbper` ↔ `pl_orbsmax` | 0.995 | Kepler's 3rd law — near-perfect predictor |
| `st_rad` ↔ `st_lum` | 0.931 | Stefan-Boltzmann |
| `st_mass` ↔ `st_lum` | 0.900 | mass-luminosity relation |
| `pl_bmassj` ↔ `pl_radj` | 0.873 | **mass-radius relation** — core imputation signal |
| `st_mass` ↔ `st_rad` | 0.784 | |
| `pl_orbper` ↔ `pl_insol` | 0.764 | closer orbit → more irradiation |

**Conclusion**: The correlations are strong and physically grounded. BERT has more than
enough signal to impute `pl_bmassj` (48% fill) from radius + stellar params, and vice versa.

### Discovery method breakdown
73.6% Transit, 19.1% Radial Velocity — transit planets have radius but often lack mass,
RV planets have mass but often lack radius. Perfect complementary imputation scenario.

### Outputs
- `data/nasa_archive.csv` — raw TAP download (39k rows)
- `data/nasa_deduped.csv` — 6,128 unique planets, best-coverage row each
- `data/missingness.csv` — fill rates
- `data/missingness.png` — bar chart
- `data/correlations.png` — Pearson heatmap
- `data/scatter.png` — mass-radius + period-temperature scatter
