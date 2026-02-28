# ML Model — Teammate Briefing

> Written for the C++ / rendering / SDK teams. You don't need to understand the model
> internals — just what it takes as input, what it gives back, and what to be aware of.

---

## What the model does

Takes a planet row with **some fields missing** and fills in the gaps.

```
Input:   { pl_orbper: 365.25, st_teff: 5778, st_mass: 1.0, pl_radj: 0.089 }
Output:  { pl_bmassj: 0.0031, pl_eqt: 254.6, pl_insol: 0.98, pl_dens: 5.4, ... }
```

It never overwrites a value you already have. It only fills `null` / `NaN` slots.
You give it whatever you know — orbital period, stellar temperature, radius, anything —
and it predicts the rest from patterns learned across 6,128 confirmed exoplanets
**plus all 8 solar system planets** (our most precisely measured reference points).

---

## One hard constraint

**Always provide at least one direct planet measurement** — either `pl_radj` (radius) or `pl_bmassj` (mass). Without one of these, the model has no physical anchor for the planet itself and will default toward the most common type in training (GasGiant/MiniNeptune). Stellar and orbital params alone are not enough.

In practice this is fine — any real exoplanet detection gives you at least one of: radius (from transit) or mass (from radial velocity).

---

## How to call it (Python)

```python
from src_ml.imputer import BertImputer

imputer = BertImputer(
    checkpoint="checkpoints/bert_imputer_best.pt",
    tokenizer_config="tokenizer_config.json"
)

filled = imputer.predict({
    "pl_orbper": 365.25,
    "st_teff":   5778.0,
    "st_mass":   1.0,
    "pl_radj":   0.089,
})

# filled is a dict of ALL features, measured + predicted
print(filled["pl_bmassj"])   # predicted planet mass in Jupiter masses
print(filled["pl_eqt"])      # predicted equilibrium temperature in K
```

The `imputer.predict()` call is synchronous, runs in ~10ms on CPU, no network needed.

---

## Output units (same as NASA archive)

| Field | What it is | Unit |
|---|---|---|
| `pl_orbper` | Orbital period | days |
| `pl_orbsmax` | Semi-major axis | AU |
| `pl_orbeccen` | Eccentricity | 0–1 |
| `pl_bmassj` | Planet mass | Jupiter masses |
| `pl_radj` | Planet radius | Jupiter radii |
| `pl_dens` | Planet density | g/cm³ |
| `pl_eqt` | Equilibrium temperature | Kelvin |
| `pl_insol` | Insolation flux | Earth flux (1.0 = same as Earth) |
| `st_teff` | Host star temperature | Kelvin |
| `st_mass` | Host star mass | Solar masses |
| `st_rad` | Host star radius | Solar radii |
| `st_lum` | Host star luminosity | log₁₀(Solar) — 0.0 = Sun |
| `st_met` | Stellar metallicity | [Fe/H] — 0.0 = Sun |
| `pl_orbincl` | Orbital inclination | degrees |
| `discoverymethod` | How it was found | string |
| `st_spectype_broad` | Host star class | O/B/A/F/G/K/M |

---

## Known accuracy (val set, epoch 80)

These are MAE values on the held-out validation split — real exoplanets the model
never trained on:

| Field | MAE | In context |
|---|---|---|
| `pl_radj` (radius) | **0.13 Rⱼ** | ~1.5× Earth radius precision |
| `pl_eqt` (eq. temp) | **60 K** | habitability zone classification safe |
| `pl_bmassj` (mass) | **1.9 Mⱼ** | weaker — only 48% of planets have measured mass |

Mass is the hardest because it's the most sparsely measured in the archive.
The solar system planets are included in training to anchor the rocky-planet end
(Earth, Mars, Venus) and the gas giant end (Jupiter, Saturn) — ranges that exoplanet
surveys under-represent because they're hard to detect.

---

## Planet type classification — the model now outputs this directly

The NASA archive has no `pl_type` column. But the model has a **third output head**
that classifies each planet into one of the 9 types from `PlanetTypes.hpp`.

Labels were derived by porting the existing `inferPlanetType()` C++ function to Python
and running it over all 6,128 training planets. 5,949 received labels (97%).

### Planet type distribution in training data

| Type | Count | % |
|---|---|---|
| GasGiant | 1,442 | 23.5% |
| MiniNeptune | 1,412 | 23.0% |
| DesertWorld | 1,049 | 17.1% |
| EarthLike | 706 | 11.5% |
| SuperEarth | 655 | 10.7% |
| HotJupiter | 622 | 10.2% |
| LavaWorld | 59 | 1.0% |
| IceWorld | 4 | 0.1% |
| OceanWorld | 0 | — (no ocean_frac data in archive) |

### How to use the planet_type output

```python
result = imputer.predict({"pl_orbper": 365, "st_teff": 5778})
print(result["planet_type"])      # → "EarthLike"
print(result["planet_type_conf"]) # → 0.87  (softmax confidence)
```

The `planet_type` string matches exactly the enum values in `PlanetTypes.hpp`.
Pass it directly to `stringToPlanetType()` — no mapping needed.

**OceanWorld is absent** from the archive because it requires `ocean_frac` data
that NASA TAP doesn't provide for exoplanets. The model will never predict OceanWorld.
That classification remains Claude's job (it infers ocean coverage from atmosphere
composition and stellar irradiation). This is intentional — we don't want the
physical imputation model guessing at rendering parameters.

---

## Data sources used and why

| Source | Used? | Why |
|---|---|---|
| NASA TAP archive (6,128 exoplanets) | ✅ yes | primary training signal |
| Solar system 8 planets ×20 | ✅ yes | most precisely measured bodies, anchor all planet types |
| Dwarf planets (Pluto, Ceres) ×20 | ✅ yes | only 20 sub-Mercury planets in archive — fills a gap |
| TRAPPIST-1 system ×8 | ✅ yes | 7 rocky planets with both mass+radius — extremely rare |
| SolarSystem rendering params (ocean/cloud/albedo) | ❌ no | Claude's domain |
| ExoAtmospheres database | ❌ no | placeholder only, no data |
| Moons (Titan, Europa, Io) | ❌ no | not planets — would confuse the physical model |

## What we deliberately left out

- **Atmospheric composition** — the NASA archive doesn't have this for most planets.
  Claude/Bedrock handles this (existing `inferAtmosphere()` pipeline). The BERT model
  fills physical parameters; Claude then uses those physical parameters to infer
  atmosphere. They complement each other.
- **Visual parameters** (cloud fraction, albedo, surface colour) — also Claude's job.
- **Uncertainty estimates** — the model outputs a single best-guess value per field,
  not a confidence interval. A future improvement would be to output the full bin
  probability distribution and use its spread as a confidence score.

---

## Training summary

| Item | Value |
|---|---|
| Training data | 4,904 NASA archive exoplanets |
| + Solar system (8 planets ×20) | Mercury, Venus, Earth, Mars, Jupiter, Saturn, Uranus, Neptune |
| + Dwarf planets (2 ×20) | Pluto, Ceres — anchor the sub-Mercury size range |
| + TRAPPIST-1 system (7 planets ×8) | Only system with mass+radius for 7 rocky planets |
| Val data | 612 exoplanets (archive only, no augmented bodies) |
| Model size | 882,019 parameters (includes planet_type head) |
| Training time | ~4 min on Apple MPS GPU |
| Epochs | 80 |
| Best val loss | 3.66 |
| Checkpoint | `checkpoints/bert_imputer_best.pt` |
| Tokenizer config | `tokenizer_config.json` |

---

## Files you care about

```
src_ml/imputer.py           ← call this (Phase 5, coming next)
tokenizer_config.json       ← bin boundaries — needed at runtime alongside checkpoint
checkpoints/
  bert_imputer_best.pt      ← trained weights
  training_log.csv          ← per-epoch loss + MAE if you want to plot it
```
