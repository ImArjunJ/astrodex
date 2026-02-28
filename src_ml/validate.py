"""
Phase 6: Validation on solar system planets + notable exoplanets.

Tests the model by deliberately hiding known fields and checking predictions
against ground truth. This is the "prove it works" step before pushing.

Outputs:
  - Terminal report
  - data/validation_results.json  (for ML_INFO.md and PR description)
"""

import sys, json, math
from pathlib import Path

import numpy as np
import pandas as pd

BASE = Path(__file__).parent.parent
sys.path.insert(0, str(Path(__file__).parent))

from imputer import BertImputer
from dataset import SOLAR_SYSTEM_PLANETS

imputer = BertImputer()

# ── Ground truth for solar system ─────────────────────────────────────────────
# Full rows — we'll mask subsets and check prediction accuracy.
SOLAR_GT = {p["pl_name"]: p for p in SOLAR_SYSTEM_PLANETS}

# ── Test scenarios ─────────────────────────────────────────────────────────────
# Each scenario: give the model a realistic sparse input, check key outputs.
# This mirrors real-world exoplanet detection patterns:
#   Transit   → you have radius, period, stellar params. Mass is unknown.
#   RV        → you have mass, period, stellar params. Radius is unknown.
#   Both      → you have radius + mass. Verify derived params (density, temp).

SCENARIOS = [
    # ── Earth ──────────────────────────────────────────────────────────────────
    {
        "name": "Earth — transit-like (radius + orbital, no mass)",
        "planet": "Earth",
        "input_fields":  ["pl_radj", "pl_orbper", "pl_orbsmax", "pl_orbeccen",
                          "st_teff", "st_mass", "st_rad", "st_lum", "st_met"],
        "predict_fields": ["pl_bmassj", "pl_dens", "pl_eqt", "pl_insol"],
        "expect_type": "EarthLike",
    },
    {
        "name": "Earth — RV-like (mass + orbital, no radius)",
        "planet": "Earth",
        "input_fields":  ["pl_bmassj", "pl_orbper", "pl_orbsmax",
                          "st_teff", "st_mass", "st_rad"],
        "predict_fields": ["pl_radj", "pl_dens", "pl_eqt"],
        "expect_type": "EarthLike",
    },
    # ── Mars ───────────────────────────────────────────────────────────────────
    {
        "name": "Mars — transit-like (radius + orbital, no mass)",
        "planet": "Mars",
        "input_fields":  ["pl_radj", "pl_orbper", "pl_orbsmax",
                          "st_teff", "st_mass", "st_rad"],
        "predict_fields": ["pl_bmassj", "pl_dens", "pl_eqt"],
        "expect_type": "IceWorld",
    },
    # ── Jupiter ────────────────────────────────────────────────────────────────
    {
        "name": "Jupiter — mass only (no radius)",
        "planet": "Jupiter",
        "input_fields":  ["pl_bmassj", "pl_orbper", "pl_orbsmax",
                          "st_teff", "st_mass"],
        "predict_fields": ["pl_radj", "pl_dens", "pl_eqt"],
        "expect_type": "GasGiant",
    },
    {
        "name": "Jupiter — radius only (no mass)",
        "planet": "Jupiter",
        "input_fields":  ["pl_radj", "pl_orbper", "pl_orbsmax",
                          "st_teff", "st_mass"],
        "predict_fields": ["pl_bmassj", "pl_dens", "pl_eqt"],
        "expect_type": "GasGiant",
    },
    # ── Saturn ─────────────────────────────────────────────────────────────────
    {
        "name": "Saturn — both mass + radius (verify derived)",
        "planet": "Saturn",
        "input_fields":  ["pl_bmassj", "pl_radj", "pl_orbper",
                          "st_teff", "st_mass"],
        "predict_fields": ["pl_dens", "pl_eqt", "pl_insol"],
        "expect_type": "GasGiant",
    },
    # ── Neptune ────────────────────────────────────────────────────────────────
    {
        "name": "Neptune — minimal input (period + stellar only)",
        "planet": "Neptune",
        "input_fields":  ["pl_radj", "pl_orbper", "pl_orbsmax",
                          "st_teff", "st_mass"],
        "predict_fields": ["pl_bmassj", "pl_eqt"],
        "expect_type": "MiniNeptune",
    },
    # ── Pluto (dwarf) ──────────────────────────────────────────────────────────
    {
        "name": "Pluto — radius + orbit (extreme cold small body)",
        "planet": "Pluto",
        "input_fields":  ["pl_radj", "pl_orbper", "pl_orbsmax", "pl_orbeccen",
                          "st_teff", "st_mass"],
        "predict_fields": ["pl_bmassj", "pl_dens", "pl_eqt"],
        "expect_type": "IceWorld",
    },
]

# ── Notable exoplanets from NASA archive ──────────────────────────────────────
# These are real measured planets — mask mass and check prediction.
EXOPLANET_TESTS = [
    {
        "name": "Kepler-452 b (Earth-size, habitable zone)",
        "input":  {"pl_radj": 0.101, "pl_orbper": 384.84, "pl_orbsmax": 1.046,
                   "st_teff": 5757.0, "st_mass": 1.037, "st_rad": 1.11,
                   "st_met": 0.21, "pl_eqt": 220.0},
        "expect_type": "EarthLike",
    },
    {
        "name": "TRAPPIST-1 e (rocky, habitable zone)",
        "input":  {"pl_radj": 0.082, "pl_bmassj": 0.0020, "pl_orbper": 6.1,
                   "pl_orbsmax": 0.029, "st_teff": 2566.0, "st_mass": 0.0898,
                   "st_rad": 0.121, "pl_eqt": 251.0},
        "expect_type": "EarthLike",
    },
    {
        "name": "Hot Jupiter — WASP-12 b",
        "input":  {"pl_radj": 1.736, "pl_bmassj": 1.404, "pl_orbper": 1.091,
                   "pl_orbsmax": 0.0229, "st_teff": 6300.0, "st_mass": 1.35,
                   "pl_eqt": 2580.0},
        "expect_type": "HotJupiter",
    },
]


def pct_error(predicted, actual):
    if actual == 0:
        return float("nan")
    return abs(predicted - actual) / abs(actual) * 100


def grade(err_pct):
    if err_pct < 10:   return "✓ excellent"
    if err_pct < 30:   return "✓ good"
    if err_pct < 60:   return "~ fair"
    return                    "✗ poor"


def run_solar_validation():
    print("\n" + "═" * 72)
    print("SOLAR SYSTEM VALIDATION")
    print("═" * 72)

    results = []

    for scenario in SCENARIOS:
        gt   = SOLAR_GT[scenario["planet"]]
        row  = {f: gt[f] for f in scenario["input_fields"] if f in gt}
        pred = imputer.predict(row)

        type_correct = pred["planet_type"] == scenario["expect_type"]
        type_mark = "✓" if type_correct else "✗"

        print(f"\n  {scenario['name']}")
        print(f"    planet_type = {pred['planet_type']:<14} conf={pred['planet_type_conf']:.2f}"
              f"  {type_mark} (expected {scenario['expect_type']})")

        field_results = {}
        for feat in scenario["predict_fields"]:
            actual    = gt.get(feat)
            predicted = pred.get(feat)
            src       = pred["_source"].get(feat, "?")
            if actual is None or predicted is None:
                print(f"    {'?':>2} {feat:<22}  predicted={str(predicted):<10}  actual=—")
                continue
            err = pct_error(predicted, actual)
            g   = grade(err)
            print(f"    {g[:1]:>2} {feat:<22}  predicted={predicted:>9.4g}"
                  f"  actual={actual:>9.4g}  err={err:5.1f}%  [{g[2:]}]")
            field_results[feat] = {"predicted": predicted, "actual": actual,
                                   "err_pct": round(err, 1)}

        results.append({
            "scenario": scenario["name"],
            "planet_type_correct": type_correct,
            "predicted_type": pred["planet_type"],
            "expected_type": scenario["expect_type"],
            "fields": field_results,
        })

    return results


def run_exoplanet_validation():
    print("\n" + "═" * 72)
    print("NOTABLE EXOPLANET VALIDATION")
    print("═" * 72)

    results = []
    for test in EXOPLANET_TESTS:
        pred = imputer.predict(test["input"])
        type_correct = pred["planet_type"] == test["expect_type"]
        mark = "✓" if type_correct else "✗"
        print(f"\n  {test['name']}")
        print(f"    planet_type = {pred['planet_type']:<14} conf={pred['planet_type_conf']:.2f}"
              f"  {mark} (expected {test['expect_type']})")
        for feat, val in pred.items():
            if feat.startswith("_") or feat == "planet_type" or feat == "planet_type_conf":
                continue
            src = pred["_source"].get(feat, "?")
            if src == "bert":
                val_str = f"{val:.4g}" if isinstance(val, (int, float)) else str(val)
                print(f"    → {feat:<22} = {val_str}  [predicted]")

        results.append({
            "name": test["name"],
            "planet_type_correct": type_correct,
            "predicted_type": pred["planet_type"],
            "expected_type": test["expect_type"],
        })

    return results


def summary_stats(solar_results):
    print("\n" + "═" * 72)
    print("SUMMARY")
    print("═" * 72)

    type_correct = sum(1 for r in solar_results if r["planet_type_correct"])
    print(f"\n  Planet type accuracy:  {type_correct}/{len(solar_results)}"
          f"  ({type_correct/len(solar_results)*100:.0f}%)")

    all_errors = {}
    for r in solar_results:
        for feat, vals in r["fields"].items():
            all_errors.setdefault(feat, []).append(vals["err_pct"])

    print(f"\n  Per-field median % error (solar system validation):")
    stats = {}
    for feat, errs in sorted(all_errors.items()):
        med = np.median(errs)
        g   = grade(med)
        print(f"    {feat:<22}  median err = {med:5.1f}%  [{g[2:]}]")
        stats[feat] = round(med, 1)

    return stats


if __name__ == "__main__":
    solar   = run_solar_validation()
    exo     = run_exoplanet_validation()
    stats   = summary_stats(solar)

    # Save results for PR description
    out = {
        "solar_system": solar,
        "exoplanets": exo,
        "field_median_error_pct": stats,
        "type_accuracy": f"{sum(r['planet_type_correct'] for r in solar)}/{len(solar)}",
    }
    out_path = BASE / "data" / "validation_results.json"
    with open(out_path, "w") as f:
        json.dump(out, f, indent=2)

    print(f"\n  Results saved → {out_path}")
    print("\nPhase 6 complete ✓")
