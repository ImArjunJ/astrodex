"""
Phase 4: ExoplanetDataset
Combines NASA archive exoplanets + solar system planets into one training set.

Solar system rows are upsampled (repeated N times) to give the model strong
anchoring signal from our most accurately measured planets.

Each __getitem__ call:
  1. Takes a planet row
  2. Randomly masks 15-40% of known fields (MLM-style)
  3. Returns tokenized input + targets for masked positions
"""

import sys
import random
from pathlib import Path
from model import PLANET_TYPES

import numpy as np
import pandas as pd
import torch
from torch.utils.data import Dataset

sys.path.insert(0, str(Path(__file__).parent))
from tokenizer import (
    ExoplanetTokenizer,
    NUMERICAL_FEATURES,
    CATEGORICAL_FEATURES,
    FEATURE_ORDER,
    N_BINS,
    SPECIAL_TOKENS,
)

# ── Solar system + dwarf planet ground truth ──────────────────────────────────
# Values synced exactly with SolarSystemDatabase.cpp (mass_earth, radius_earth,
# temp_k, density_gcc as stored in the C++ codebase).
# pl_eqt = equilibrium temperature (matches NASA archive convention).
# Dwarf planets added to anchor the sub-Mercury end of the distribution.
#
# NOT included here: ocean_frac, cloud_frac, albedo, atmosphere_composition
# Those are rendering/atmospheric params — Claude's domain, not physical imputation.

_SUN = {"hostname": "Sun", "st_teff": 5778.0, "st_mass": 1.0, "st_rad": 1.0,
        "st_lum": 0.0, "st_met": 0.0, "discoverymethod": "Direct observation",
        "st_spectype": "G2 V"}

SOLAR_SYSTEM_PLANETS = [
    # ── 8 main planets (values from SolarSystemDatabase.cpp) ─────────────────
    {**_SUN, "pl_name": "Mercury", "planet_type": "DesertWorld",
     "pl_orbper": 87.969,   "pl_orbsmax": 0.387,  "pl_orbeccen": 0.206,
     "pl_bmassj": 1.742e-4, "pl_radj": 0.0353,    "pl_dens": 5.43,
     "pl_eqt": 440.0,       "pl_insol": 6.67,      "pl_orbincl": 7.0},

    {**_SUN, "pl_name": "Venus",   "planet_type": "DesertWorld",
     "pl_orbper": 224.701,  "pl_orbsmax": 0.723,  "pl_orbeccen": 0.007,
     "pl_bmassj": 2.564e-3, "pl_radj": 0.0868,    "pl_dens": 5.24,
     "pl_eqt": 737.0,       "pl_insol": 1.911,     "pl_orbincl": 3.4},

    {**_SUN, "pl_name": "Earth",   "planet_type": "EarthLike",
     "pl_orbper": 365.25,   "pl_orbsmax": 1.0,    "pl_orbeccen": 0.017,
     "pl_bmassj": 3.146e-3, "pl_radj": 0.0892,    "pl_dens": 5.51,
     "pl_eqt": 288.0,       "pl_insol": 1.0,       "pl_orbincl": 0.0},

    {**_SUN, "pl_name": "Mars",    "planet_type": "IceWorld",
     "pl_orbper": 686.97,   "pl_orbsmax": 1.524,  "pl_orbeccen": 0.093,
     "pl_bmassj": 3.38e-4,  "pl_radj": 0.0477,    "pl_dens": 3.93,
     "pl_eqt": 210.0,       "pl_insol": 0.431,     "pl_orbincl": 1.9},

    {**_SUN, "pl_name": "Jupiter", "planet_type": "GasGiant",
     "pl_orbper": 4332.59,  "pl_orbsmax": 5.204,  "pl_orbeccen": 0.049,
     "pl_bmassj": 1.0,      "pl_radj": 1.0,        "pl_dens": 1.33,
     "pl_eqt": 165.0,       "pl_insol": 0.0369,    "pl_orbincl": 1.3},

    {**_SUN, "pl_name": "Saturn",  "planet_type": "GasGiant",
     "pl_orbper": 10759.22, "pl_orbsmax": 9.582,  "pl_orbeccen": 0.057,
     "pl_bmassj": 0.2994,   "pl_radj": 0.8362,     "pl_dens": 0.69,
     "pl_eqt": 134.0,       "pl_insol": 0.0108,    "pl_orbincl": 2.5},

    {**_SUN, "pl_name": "Uranus",  "planet_type": "MiniNeptune",
     "pl_orbper": 30688.5,  "pl_orbsmax": 19.191, "pl_orbeccen": 0.046,
     "pl_bmassj": 0.04582,  "pl_radj": 0.3577,     "pl_dens": 1.27,
     "pl_eqt": 76.0,        "pl_insol": 0.00273,   "pl_orbincl": 0.77},

    {**_SUN, "pl_name": "Neptune", "planet_type": "MiniNeptune",
     "pl_orbper": 60195.0,  "pl_orbsmax": 30.07,  "pl_orbeccen": 0.010,
     "pl_bmassj": 0.05395,  "pl_radj": 0.3461,     "pl_dens": 1.64,
     "pl_eqt": 72.0,        "pl_insol": 0.00111,   "pl_orbincl": 1.77},

    # ── Dwarf planets — anchor sub-Mercury end of distribution ───────────────
    # Only 20 exoplanets in the archive have radius < Mercury (pl_radj < 0.05).
    # These fill a real gap in the training distribution.
    {**_SUN, "pl_name": "Pluto",   "planet_type": "IceWorld",
     "pl_orbper": 90560.0,  "pl_orbsmax": 39.48,  "pl_orbeccen": 0.249,
     "pl_bmassj": 6.56e-6,  "pl_radj": 0.01659,   "pl_dens": 1.88,
     "pl_eqt": 44.0,        "pl_insol": 0.00064,   "pl_orbincl": 17.1},

    {**_SUN, "pl_name": "Ceres",   "planet_type": "IceWorld",
     "pl_orbper": 1682.0,   "pl_orbsmax": 2.767,  "pl_orbeccen": 0.076,
     "pl_bmassj": 4.72e-7,  "pl_radj": 0.00428,   "pl_dens": 2.16,
     "pl_eqt": 168.0,       "pl_insol": 0.131,     "pl_orbincl": 10.6},
]

# TRAPPIST-1 planets: the only system with mass+radius both measured for 7 rocky
# planets. Upsample separately — they're the best rocky-planet training signal.
TRAPPIST1_NAMES = [f"TRAPPIST-1 {l}" for l in "bcdefgh"]


# ── Dataset ───────────────────────────────────────────────────────────────────

class ExoplanetDataset(Dataset):
    """
    PyTorch Dataset for masked imputation training.

    Args:
        df:               NASA archive DataFrame (nasa_deduped.csv)
        tokenizer:        fitted ExoplanetTokenizer
        mask_prob:        fraction of known fields to randomly mask per sample
        solar_upsample:   how many copies of each solar system planet to add
                          (they are complete + accurate, so we weight them up)
        augment:          if True, randomly drop additional fields to simulate
                          sparser inputs (helps model generalise to any subset)
        seed:             for reproducibility of train/val split
    """

    def __init__(
        self,
        df: pd.DataFrame,
        tokenizer: ExoplanetTokenizer,
        mask_prob: float = 0.25,
        solar_upsample: int = 20,
        trappist_upsample: int = 8,
        augment: bool = True,
        seed: int = 42,
    ):
        self.tokenizer = tokenizer
        self.mask_prob = mask_prob
        self.augment = augment

        rows = df.to_dict(orient="records")

        # Solar system + dwarf planets — our most precisely measured bodies
        if solar_upsample > 0:
            for planet in SOLAR_SYSTEM_PLANETS:
                rows.extend([planet] * solar_upsample)

        # TRAPPIST-1: 7 rocky planets with BOTH mass+radius known — rare and
        # valuable. Upsample modestly to strengthen the rocky-planet signal
        # without biasing toward one star system.
        if trappist_upsample > 0:
            trappist_rows = df[df["pl_name"].isin(TRAPPIST1_NAMES)].to_dict("records")
            for row in trappist_rows:
                rows.extend([row] * trappist_upsample)

        self.rows = rows
        random.seed(seed)

    def __len__(self):
        return len(self.rows)

    def __getitem__(self, idx):
        row = dict(self.rows[idx])

        # Which fields are actually populated in this row?
        known_fields = [
            f for f in FEATURE_ORDER
            if row.get(f) is not None
            and not (isinstance(row.get(f), float) and np.isnan(row[f]))
        ]

        # Decide which known fields to mask for this training example.
        # We always mask at least 1 and at most (n_known - 1) fields
        # so the model always has something to predict AND something to condition on.
        n_to_mask = max(1, int(len(known_fields) * self.mask_prob))
        if self.augment and len(known_fields) > 3:
            # Randomly vary mask rate between 15% and 50% for robustness
            rate = random.uniform(0.15, 0.50)
            n_to_mask = max(1, min(len(known_fields) - 1, int(len(known_fields) * rate)))

        mask_fields = random.sample(known_fields, n_to_mask)

        encoded = self.tokenizer.encode(row, mask_features=mask_fields)

        input_ids    = torch.tensor(encoded["input_ids"], dtype=torch.long)
        attn_mask    = torch.tensor(encoded["attention_mask"], dtype=torch.long)
        target_ids   = torch.tensor(encoded["target_ids"], dtype=torch.long)

        # Build numerical_mask and categorical_masks for the loss function
        L = len(input_ids)
        numerical_mask = torch.zeros(L, dtype=torch.bool)
        categorical_masks = {feat: torch.zeros(L, dtype=torch.bool)
                             for feat in CATEGORICAL_FEATURES}

        val_offset = self.tokenizer._val_offset

        for pos, target in enumerate(encoded["target_ids"]):
            if target == -100:
                continue
            # Determine which feature this value position belongs to
            # Value positions are at odd indices: 2, 4, 6, ... (0=CLS, 1=feat, 2=val, ...)
            # pos = 1 + feat_idx*2 + 1  =>  feat_idx = (pos - 2) // 2
            feat_idx = (pos - 2) // 2
            if 0 <= feat_idx < len(FEATURE_ORDER):
                feat = FEATURE_ORDER[feat_idx]
                if feat in NUMERICAL_FEATURES:
                    numerical_mask[pos] = True
                    # Convert token id → bin index for CE loss
                    bin_idx = target - val_offset
                    target_ids[pos] = max(0, min(N_BINS - 1, bin_idx.item()
                                                 if hasattr(bin_idx, 'item') else int(bin_idx)))
                elif feat in CATEGORICAL_FEATURES:
                    categorical_masks[feat][pos] = True
                    # Convert absolute token id → category-relative index for CE loss
                    cat_offset = self.tokenizer._cat_offset.get(feat, 0)
                    cat_idx = int(target_ids[pos].item()) - cat_offset
                    n_cats = len(self.tokenizer.cat_vocabs.get(feat, []))
                    target_ids[pos] = max(0, min(n_cats - 1, cat_idx))

        # Planet type label (-100 = unknown, ignored by CE loss)
        pt_str = row.get("planet_type")
        if pt_str and pt_str in PLANET_TYPES:
            planet_type_label = torch.tensor(PLANET_TYPES.index(pt_str), dtype=torch.long)
        else:
            planet_type_label = torch.tensor(-100, dtype=torch.long)

        return {
            "input_ids": input_ids,
            "attention_mask": attn_mask,
            "target_ids": target_ids,
            "numerical_mask": numerical_mask,
            "categorical_masks": categorical_masks,
            "planet_type_label": planet_type_label,
            "pl_name": row.get("pl_name", "unknown"),
        }


def collate_fn(batch):
    """Stack batch items into tensors. categorical_masks needs special handling."""
    input_ids    = torch.stack([b["input_ids"] for b in batch])
    attn_mask    = torch.stack([b["attention_mask"] for b in batch])
    target_ids   = torch.stack([b["target_ids"] for b in batch])
    num_mask     = torch.stack([b["numerical_mask"] for b in batch])

    cat_masks = {}
    for feat in CATEGORICAL_FEATURES:
        cat_masks[feat] = torch.stack([b["categorical_masks"][feat] for b in batch])

    planet_type_labels = torch.stack([b["planet_type_label"] for b in batch])

    return {
        "input_ids": input_ids,
        "attention_mask": attn_mask,
        "target_ids": target_ids,
        "numerical_mask": num_mask,
        "categorical_masks": cat_masks,
        "planet_type_labels": planet_type_labels,
    }


def make_splits(df: pd.DataFrame, val_frac: float = 0.1, test_frac: float = 0.1,
                seed: int = 42):
    """
    Split the archive DataFrame into train/val/test.
    Solar system planets are NOT in the archive so they go into training only
    (added by ExoplanetDataset). Test set uses well-characterised exoplanets.
    """
    rng = np.random.default_rng(seed)
    idx = rng.permutation(len(df))

    n_test = int(len(df) * test_frac)
    n_val  = int(len(df) * val_frac)

    test_idx  = idx[:n_test]
    val_idx   = idx[n_test:n_test + n_val]
    train_idx = idx[n_test + n_val:]

    return (
        df.iloc[train_idx].reset_index(drop=True),
        df.iloc[val_idx].reset_index(drop=True),
        df.iloc[test_idx].reset_index(drop=True),
    )


# ── Smoke test ────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    from pathlib import Path
    from torch.utils.data import DataLoader

    BASE = Path(__file__).parent.parent

    print("Loading tokenizer and data...")
    tok = ExoplanetTokenizer.load(str(BASE / "tokenizer_config.json"))
    df  = pd.read_csv(BASE / "data" / "nasa_deduped.csv", low_memory=False)

    train_df, val_df, test_df = make_splits(df)
    print(f"Split: train={len(train_df):,}  val={len(val_df):,}  test={len(test_df):,}")

    train_ds = ExoplanetDataset(train_df, tok, solar_upsample=20)
    val_ds   = ExoplanetDataset(val_df,   tok, solar_upsample=0, augment=False)

    print(f"\nDataset sizes:")
    print(f"  train: {len(train_ds):,}  (archive + 8×20 solar system copies)")
    print(f"  val:   {len(val_ds):,}")

    # Check a solar system sample
    solar_idx = len(train_df)  # first solar system entry
    sample = train_ds[solar_idx]
    print(f"\nSolar system sample ({sample['pl_name']}):")
    print(f"  input_ids:      {sample['input_ids'].tolist()}")
    print(f"  target_ids:     {sample['target_ids'].tolist()}")
    print(f"  numerical_mask: {sample['numerical_mask'].tolist()}")
    n_masked = sample['numerical_mask'].sum().item()
    print(f"  masked fields:  {n_masked}")

    # Decode masked targets back to physical values
    print(f"\n  Masked field predictions (ground truth targets):")
    val_offset = tok._val_offset
    for pos, (tid, nm) in enumerate(zip(sample['target_ids'].tolist(),
                                        sample['numerical_mask'].tolist())):
        if nm:
            feat_idx = (pos - 2) // 2
            feat = FEATURE_ORDER[feat_idx]
            token_id = val_offset + tid
            decoded = tok.decode_value(token_id, feat)
            original = SOLAR_SYSTEM_PLANETS[0].get(feat, "—")
            print(f"    {feat:<22} original={original!s:<12}  target_decoded={decoded:.4g}")

    # Check DataLoader
    loader = DataLoader(train_ds, batch_size=32, shuffle=True, collate_fn=collate_fn)
    batch  = next(iter(loader))
    print(f"\nDataLoader batch shapes:")
    print(f"  input_ids:      {list(batch['input_ids'].shape)}")
    print(f"  target_ids:     {list(batch['target_ids'].shape)}")
    print(f"  numerical_mask: {list(batch['numerical_mask'].shape)}")
    print(f"  masked positions per sample: "
          f"{batch['numerical_mask'].sum(dim=1).float().mean():.1f} avg")

    print("\nPhase 4 dataset smoke test passed ✓")
