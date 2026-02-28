"""
Phase 5: BertImputer — the public interface for teammates.

Usage:
    from src_ml.imputer import BertImputer

    imputer = BertImputer()   # auto-finds checkpoint + tokenizer from project root

    filled = imputer.predict({
        "pl_orbper": 365.25,
        "st_teff":   5778.0,
        "st_mass":   1.0,
    })

    print(filled["pl_bmassj"])       # predicted mass in Jupiter masses
    print(filled["pl_eqt"])          # predicted equilibrium temperature (K)
    print(filled["planet_type"])     # "EarthLike" / "GasGiant" / etc.
    print(filled["planet_type_conf"])# softmax confidence 0–1

All input values are passed through unchanged.
Only NaN / missing fields are predicted.
"""

import sys
import json
import math
from pathlib import Path
from typing import Any

import torch
import numpy as np

BASE = Path(__file__).parent.parent
sys.path.insert(0, str(Path(__file__).parent))

from tokenizer import ExoplanetTokenizer, FEATURE_ORDER, NUMERICAL_FEATURES, CATEGORICAL_FEATURES
from model import ExoplanetBERT, BertConfig, build_model, PLANET_TYPES


class BertImputer:
    """
    Load a trained checkpoint and fill missing exoplanet parameters.

    Args:
        checkpoint:       path to bert_imputer_best.pt
                          (defaults to <project_root>/checkpoints/bert_imputer_best.pt)
        tokenizer_config: path to tokenizer_config.json
                          (defaults to <project_root>/tokenizer_config.json)
        device:           "cpu" / "mps" / "cuda" — auto-detected if None
    """

    def __init__(
        self,
        checkpoint: str | None = None,
        tokenizer_config: str | None = None,
        device: str | None = None,
    ):
        ckpt_path = Path(checkpoint) if checkpoint else BASE / "checkpoints" / "bert_imputer_best.pt"
        tok_path  = Path(tokenizer_config) if tokenizer_config else BASE / "tokenizer_config.json"

        if not ckpt_path.exists():
            raise FileNotFoundError(f"Checkpoint not found: {ckpt_path}\n"
                                    f"Run src_ml/train.py first.")
        if not tok_path.exists():
            raise FileNotFoundError(f"Tokenizer config not found: {tok_path}\n"
                                    f"Run src_ml/tokenizer.py first.")

        # Device
        if device:
            self.device = torch.device(device)
        elif torch.cuda.is_available():
            self.device = torch.device("cuda")
        elif torch.backends.mps.is_available():
            self.device = torch.device("mps")
        else:
            self.device = torch.device("cpu")

        # Tokenizer
        self.tokenizer = ExoplanetTokenizer.load(str(tok_path))

        # Model
        self.model, self.cfg = build_model(self.tokenizer)
        ckpt = torch.load(ckpt_path, map_location=self.device, weights_only=False)
        self.model.load_state_dict(ckpt["model_state_dict"])
        self.model.to(self.device)
        self.model.eval()

        self._val_offset = self.tokenizer._val_offset

    # ── Public API ────────────────────────────────────────────────────────────

    def predict(self, known: dict[str, Any]) -> dict[str, Any]:
        """
        Fill in missing physical parameters for a planet.

        Args:
            known: dict of {field_name: value} for measured parameters.
                   Any field absent or set to None / NaN is treated as missing.

        Returns:
            Complete dict with all fields:
              - measured fields passed through unchanged
              - missing fields replaced with model predictions
              - "planet_type"      → string matching PlanetTypes.hpp enum
              - "planet_type_conf" → softmax confidence (0–1) for that prediction
              - "_source"          → per-field dict: "measured" or "bert"
        """
        known = self._normalise_input(known)

        # Which fields are genuinely missing?
        missing_fields = [
            f for f in FEATURE_ORDER
            if known.get(f) is None or (isinstance(known.get(f), float) and math.isnan(known[f]))
        ]

        # Encode — all missing fields become [MASK]
        encoded = self.tokenizer.encode(known, mask_features=missing_fields)
        input_ids = torch.tensor(encoded["input_ids"], dtype=torch.long).unsqueeze(0).to(self.device)
        attn_mask = torch.tensor(encoded["attention_mask"], dtype=torch.long).unsqueeze(0).to(self.device)

        # Forward pass
        with torch.no_grad():
            out = self.model(input_ids, attn_mask)

        reg_logits = out["regression_logits"][0]        # (seq_len, n_bins)
        pt_logits  = out["planet_type_logits"][0]       # (n_planet_types,)

        # ── Decode missing numerical / categorical fields ─────────────────────
        result = dict(known)
        source = {f: "measured" for f in known if known[f] is not None}

        for feat in missing_fields:
            pos = self.tokenizer.value_position_for_feature(feat)

            if self.tokenizer.is_numerical(feat):
                bin_idx   = reg_logits[pos].argmax().item()
                token_id  = self._val_offset + bin_idx
                value     = self.tokenizer.decode_value(token_id, feat)
                if value is not None:
                    result[feat] = value
                    source[feat] = "bert"

            else:  # categorical
                cls_logits_feat = out["classification_logits"].get(feat)
                if cls_logits_feat is not None:
                    cat_idx   = cls_logits_feat[0, pos].argmax().item()
                    cat_offset = self.tokenizer._cat_offset.get(feat, 0)
                    token_id  = cat_offset + cat_idx
                    value     = self.tokenizer.decode_value(token_id, feat)
                    if value is not None:
                        result[feat] = value
                        source[feat] = "bert"

        # ── Planet type (sentence-level from [CLS]) ───────────────────────────
        pt_probs = torch.softmax(pt_logits, dim=-1)
        pt_idx   = pt_probs.argmax().item()
        result["planet_type"]      = PLANET_TYPES[pt_idx]
        result["planet_type_conf"] = round(pt_probs[pt_idx].item(), 4)

        result["_source"] = source
        return result

    def predict_batch(self, rows: list[dict[str, Any]]) -> list[dict[str, Any]]:
        """Convenience wrapper — predict for a list of planet rows."""
        return [self.predict(row) for row in rows]

    # ── Helpers ───────────────────────────────────────────────────────────────

    def _normalise_input(self, known: dict) -> dict:
        """Coerce values to float where expected, mark non-finite as None."""
        out = {}
        for feat in FEATURE_ORDER:
            val = known.get(feat)
            if val is None:
                out[feat] = None
                continue
            if feat in NUMERICAL_FEATURES:
                try:
                    v = float(val)
                    out[feat] = v if math.isfinite(v) else None
                except (TypeError, ValueError):
                    out[feat] = None
            else:
                out[feat] = str(val) if val is not None else None
        # Also carry through any extra keys (e.g. pl_name) unchanged
        for k, v in known.items():
            if k not in out:
                out[k] = v
        return out

    @property
    def feature_names(self) -> list[str]:
        """All feature names the model handles."""
        return list(FEATURE_ORDER)


# ── CLI spot-check ────────────────────────────────────────────────────────────

if __name__ == "__main__":
    print("Loading imputer...")
    imputer = BertImputer()
    print(f"  Device:     {imputer.device}")
    print(f"  Parameters: {imputer.model.count_parameters():,}")

    TESTS = [
        {
            "name": "Earth (only orbital period + stellar params)",
            "input": {"pl_orbper": 365.25, "pl_orbsmax": 1.0, "st_teff": 5778.0,
                      "st_mass": 1.0, "st_rad": 1.0},
            "expect": {"pl_radj": 0.0892, "pl_bmassj": 0.00315, "pl_eqt": 288.0,
                       "pl_dens": 5.51, "planet_type": "EarthLike"},
        },
        {
            "name": "Jupiter (mass + period only)",
            "input": {"pl_bmassj": 1.0, "pl_orbper": 4332.59, "st_mass": 1.0,
                      "st_teff": 5778.0},
            "expect": {"pl_radj": 1.0, "pl_eqt": 165.0, "planet_type": "GasGiant"},
        },
        {
            "name": "Hot Jupiter (short period, high temp)",
            "input": {"pl_orbper": 3.5, "pl_orbsmax": 0.045, "st_teff": 6000.0,
                      "st_mass": 1.2, "pl_radj": 1.3},
            "expect": {"planet_type": "HotJupiter"},
        },
        {
            "name": "Kepler-452 b (radius only, no mass)",
            "input": {"pl_radj": 0.101, "pl_orbper": 384.84, "st_teff": 5757.0,
                      "st_mass": 1.037, "pl_eqt": 220.0},
            "expect": {"planet_type": "EarthLike"},
        },
    ]

    print("\n" + "=" * 70)
    for test in TESTS:
        result = imputer.predict(test["input"])
        print(f"\n  Test: {test['name']}")
        print(f"  Input fields:  {list(test['input'].keys())}")
        print(f"  Predicted:     planet_type={result['planet_type']}"
              f"  (conf={result['planet_type_conf']:.2f})")

        for feat, expected in test["expect"].items():
            if feat == "planet_type":
                status = "✓" if result.get(feat) == expected else "✗"
                print(f"    {status} planet_type = {result.get(feat)!r}  (expected {expected!r})")
            else:
                predicted = result.get(feat)
                src       = result.get("_source", {}).get(feat, "?")
                if predicted is not None and isinstance(expected, float):
                    err_pct = abs(predicted - expected) / max(abs(expected), 1e-9) * 100
                    status = "✓" if err_pct < 50 else "~"
                    print(f"    {status} {feat:<22} predicted={predicted:>10.4g}"
                          f"  expected={expected:>10.4g}  err={err_pct:.0f}%  src={src}")
                else:
                    print(f"    ? {feat:<22} predicted={predicted!r}  src={src}")

    print("\n" + "=" * 70)
    print("\nPhase 5 complete ✓  —  imputer.py ready for teammates")
