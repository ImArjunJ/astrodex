"""
Phase 2: ExoplanetTokenizer
Converts a dict of {feature: value} into a token-id sequence for the BERT model.

Sequence format (canonical feature order, fixed):
  [CLS] [feat_0] [val_or_mask_0] [feat_1] [val_or_mask_1] ... [SEP]

Numerical values  → log-binned into N_BINS per feature (own bin boundaries)
Categorical values → direct vocab token per category
Missing values    → [MASK] token (model learns to predict these)
"""

import json
import os
import numpy as np
import pandas as pd
from pathlib import Path

# ── Vocabulary layout ────────────────────────────────────────────────────────
# [PAD]=0  [UNK]=1  [CLS]=2  [SEP]=3  [MASK]=4
# [feat_*] = 5 .. 5+N_FEATURES-1
# [val_bin_NNN] = 5+N_FEATURES .. 5+N_FEATURES+N_BINS-1
# [cat_*] = 5+N_FEATURES+N_BINS .. end
# ─────────────────────────────────────────────────────────────────────────────

N_BINS = 200

NUMERICAL_FEATURES = [
    "pl_orbper",   # orbital period (days)
    "pl_orbsmax",  # semi-major axis (AU)
    "pl_orbeccen", # eccentricity (0–1, not log-scaled)
    "pl_bmassj",   # planet mass (Jupiter masses)
    "pl_radj",     # planet radius (Jupiter radii)
    "pl_dens",     # planet density (g/cm³)
    "pl_eqt",      # equilibrium temperature (K)
    "pl_insol",    # insolation flux (Earth flux)
    "st_teff",     # stellar effective temperature (K)
    "st_mass",     # stellar mass (solar)
    "st_rad",      # stellar radius (solar)
    "st_lum",      # stellar luminosity (log solar — already log, bin linearly)
    "st_met",      # stellar metallicity [Fe/H] (can be negative, bin linearly)
    "pl_orbincl",  # orbital inclination (degrees)
]

CATEGORICAL_FEATURES = [
    "discoverymethod",
    "st_spectype_broad",  # we coarsen st_spectype → broad class (O,B,A,F,G,K,M)
]

# Features where log10-scaling is NOT appropriate (already log, linear, or bounded)
NO_LOG_FEATURES = {"pl_orbeccen", "st_lum", "st_met", "pl_orbincl", "st_spectype_broad"}

FEATURE_ORDER = NUMERICAL_FEATURES + CATEGORICAL_FEATURES

SPECIAL_TOKENS = {"[PAD]": 0, "[UNK]": 1, "[CLS]": 2, "[SEP]": 3, "[MASK]": 4}
N_SPECIAL = len(SPECIAL_TOKENS)


def _broad_spectype(s: str) -> str:
    """Coarsen e.g. 'G5 V', 'K0 III' → 'G', 'K'."""
    if not isinstance(s, str) or not s.strip():
        return None
    c = s.strip()[0].upper()
    return c if c in "OBAFGKM" else "other"


class ExoplanetTokenizer:
    """
    Build from a DataFrame, then encode/decode individual planet rows.
    Persist to / load from a JSON config file.
    """

    def __init__(self):
        self.bin_boundaries: dict[str, list[float]] = {}  # feature → N_BINS+1 edges
        self.use_log: dict[str, bool] = {}                # feature → bool
        self.cat_vocabs: dict[str, list[str]] = {}        # feature → ordered categories

        self._feat_offset = N_SPECIAL                     # token id of first feature token
        self._val_offset = N_SPECIAL + len(FEATURE_ORDER) # token id of first value bin
        self._cat_offset: dict[str, int] = {}             # feature → token id of first cat

        self.vocab_size: int = 0

    # ── Build ────────────────────────────────────────────────────────────────

    def build(self, df: pd.DataFrame) -> "ExoplanetTokenizer":
        """Fit bin boundaries from a DataFrame. Call once on the training split."""
        df = self._preprocess(df.copy())

        for feat in NUMERICAL_FEATURES:
            if feat not in df.columns:
                continue
            values = df[feat].dropna().values.astype(float)
            values = values[np.isfinite(values)]

            use_log = feat not in NO_LOG_FEATURES and (values > 0).all()
            self.use_log[feat] = use_log

            scaled = np.log10(values) if use_log else values
            lo, hi = np.percentile(scaled, 0.1), np.percentile(scaled, 99.9)
            # small guard for degenerate ranges
            if hi <= lo:
                hi = lo + 1e-6
            edges = np.linspace(lo, hi, N_BINS + 1).tolist()
            self.bin_boundaries[feat] = edges

        # Categorical vocabs
        for feat in CATEGORICAL_FEATURES:
            if feat not in df.columns:
                continue
            cats = sorted(df[feat].dropna().unique().tolist())
            self.cat_vocabs[feat] = cats

        self._build_offsets()
        return self

    def _preprocess(self, df: pd.DataFrame) -> pd.DataFrame:
        """Add derived columns (broad spectype), clean obvious outliers."""
        if "st_spectype" in df.columns:
            df["st_spectype_broad"] = df["st_spectype"].apply(_broad_spectype)
        return df

    def _build_offsets(self):
        offset = N_SPECIAL + len(FEATURE_ORDER) + N_BINS
        for feat in CATEGORICAL_FEATURES:
            self.cat_vocabs.setdefault(feat, [])
            self._cat_offset[feat] = offset
            offset += len(self.cat_vocabs[feat])
        self.vocab_size = offset

    # ── Encode ───────────────────────────────────────────────────────────────

    def encode(self, row: dict, mask_features: list[str] | None = None) -> dict:
        """
        Encode a planet row into token ids.

        Args:
            row: {feature_name: value} — missing values should be absent or None/NaN
            mask_features: features to replace with [MASK] regardless of whether
                           they have a value (used during training to create targets)

        Returns:
            {
              "input_ids":      List[int],   length = 2 + 2*N_FEATURES
              "attention_mask": List[int],   1 = real token, 0 = pad (unused here)
              "target_ids":     List[int],   token id of ground-truth for each value
                                             position; -100 for non-masked positions
              "masked_positions": List[int], indices into input_ids that are [MASK]
              "feature_order":  List[str],   canonical feature order
            }
        """
        if mask_features is None:
            mask_features = []

        row = self._preprocess_row(row)
        input_ids = [SPECIAL_TOKENS["[CLS]"]]
        target_ids_full = [-100]  # CLS has no target

        for i, feat in enumerate(FEATURE_ORDER):
            feat_token = self._feat_offset + i
            input_ids.append(feat_token)
            target_ids_full.append(-100)  # feature name tokens are never targets

            value = row.get(feat)
            is_missing = value is None or (isinstance(value, float) and np.isnan(value))
            should_mask = feat in mask_features

            if is_missing or should_mask:
                val_token = SPECIAL_TOKENS["[MASK]"]
                if should_mask and not is_missing:
                    # we know the true value — record as target
                    true_token = self._encode_value(feat, value)
                    target_ids_full.append(true_token)
                else:
                    target_ids_full.append(-100)  # genuinely unknown
            else:
                val_token = self._encode_value(feat, value)
                target_ids_full.append(-100)  # known value — not a target

            input_ids.append(val_token)

        input_ids.append(SPECIAL_TOKENS["[SEP]"])
        target_ids_full.append(-100)

        masked_positions = [i for i, tid in enumerate(target_ids_full) if tid != -100]
        attention_mask = [1] * len(input_ids)

        return {
            "input_ids": input_ids,
            "attention_mask": attention_mask,
            "target_ids": target_ids_full,
            "masked_positions": masked_positions,
            "feature_order": FEATURE_ORDER,
        }

    def _preprocess_row(self, row: dict) -> dict:
        row = dict(row)
        if "st_spectype" in row and "st_spectype_broad" not in row:
            row["st_spectype_broad"] = _broad_spectype(row.get("st_spectype"))
        return row

    def _encode_value(self, feat: str, value) -> int:
        if feat in NUMERICAL_FEATURES:
            return self._encode_numerical(feat, value)
        else:
            return self._encode_categorical(feat, value)

    def _encode_numerical(self, feat: str, value: float) -> int:
        if feat not in self.bin_boundaries:
            return SPECIAL_TOKENS["[UNK]"]
        try:
            v = float(value)
        except (TypeError, ValueError):
            return SPECIAL_TOKENS["[UNK]"]
        if not np.isfinite(v):
            return SPECIAL_TOKENS["[UNK]"]

        scaled = np.log10(v) if self.use_log.get(feat, False) else v
        edges = self.bin_boundaries[feat]
        # clip to range then find bin
        bin_idx = int(np.searchsorted(edges, scaled, side="right")) - 1
        bin_idx = max(0, min(N_BINS - 1, bin_idx))
        return self._val_offset + bin_idx

    def _encode_categorical(self, feat: str, value) -> int:
        vocab = self.cat_vocabs.get(feat, [])
        offset = self._cat_offset.get(feat, None)
        if offset is None or not isinstance(value, str):
            return SPECIAL_TOKENS["[UNK]"]
        try:
            idx = vocab.index(value)
            return offset + idx
        except ValueError:
            return SPECIAL_TOKENS["[UNK]"]

    # ── Decode ───────────────────────────────────────────────────────────────

    def decode_value(self, token_id: int, feat: str):
        """
        Convert a predicted token id back to a float (numerical) or string (categorical).
        Returns None if the token is special / out of range.
        """
        if feat in NUMERICAL_FEATURES:
            return self._decode_numerical(token_id, feat)
        else:
            return self._decode_categorical(token_id, feat)

    def _decode_numerical(self, token_id: int, feat: str):
        if feat not in self.bin_boundaries:
            return None
        bin_idx = token_id - self._val_offset
        if not (0 <= bin_idx < N_BINS):
            return None
        edges = self.bin_boundaries[feat]
        # return bin centre
        centre_scaled = (edges[bin_idx] + edges[bin_idx + 1]) / 2.0
        if self.use_log.get(feat, False):
            return float(10 ** centre_scaled)
        return float(centre_scaled)

    def _decode_categorical(self, token_id: int, feat: str):
        offset = self._cat_offset.get(feat)
        vocab = self.cat_vocabs.get(feat, [])
        if offset is None:
            return None
        idx = token_id - offset
        if 0 <= idx < len(vocab):
            return vocab[idx]
        return None

    # ── Feature metadata ─────────────────────────────────────────────────────

    def feature_token_id(self, feat: str) -> int:
        idx = FEATURE_ORDER.index(feat)
        return self._feat_offset + idx

    def value_position_for_feature(self, feat: str) -> int:
        """Index in the input_ids sequence for the value token of a given feature."""
        idx = FEATURE_ORDER.index(feat)
        return 1 + idx * 2 + 1  # CLS + (feat_token + val_token)*i + val_token

    def is_numerical(self, feat: str) -> bool:
        return feat in NUMERICAL_FEATURES

    def bin_centre_for_index(self, feat: str, bin_idx: int) -> float:
        edges = self.bin_boundaries[feat]
        return (edges[bin_idx] + edges[bin_idx + 1]) / 2.0

    # ── Persist ──────────────────────────────────────────────────────────────

    def save(self, path: str):
        config = {
            "n_bins": N_BINS,
            "numerical_features": NUMERICAL_FEATURES,
            "categorical_features": CATEGORICAL_FEATURES,
            "feature_order": FEATURE_ORDER,
            "no_log_features": list(NO_LOG_FEATURES),
            "bin_boundaries": self.bin_boundaries,
            "use_log": {k: bool(v) for k, v in self.use_log.items()},
            "cat_vocabs": self.cat_vocabs,
        }
        Path(path).parent.mkdir(parents=True, exist_ok=True)
        with open(path, "w") as f:
            json.dump(config, f, indent=2)
        print(f"Tokenizer saved → {path}")
        print(f"  Vocab size: {self.vocab_size}")
        print(f"  Sequence length: {2 + 2 * len(FEATURE_ORDER)} tokens")

    @classmethod
    def load(cls, path: str) -> "ExoplanetTokenizer":
        with open(path) as f:
            config = json.load(f)
        tok = cls()
        tok.bin_boundaries = config["bin_boundaries"]
        tok.use_log = config["use_log"]
        tok.cat_vocabs = config["cat_vocabs"]
        tok._build_offsets()
        return tok


# ── Scrub / validation ────────────────────────────────────────────────────────

def scrub_tokenizer(tok: ExoplanetTokenizer, df: pd.DataFrame) -> bool:
    """
    Run the tokenizer scrub checklist:
      1. Bin boundaries monotonically increasing
      2. No vocab id collisions
      3. Round-trip: encode → decode recovers value within ±1 bin width
      4. Every column in df encodes without [UNK] on >95% of non-null values
    Returns True if all checks pass.
    """
    print("\n── Tokenizer scrub ─────────────────────────────────────────────")
    passed = True

    # 1. Monotonic bin boundaries
    for feat, edges in tok.bin_boundaries.items():
        arr = np.array(edges)
        if not np.all(np.diff(arr) > 0):
            print(f"  ✗ FAIL: {feat} bin boundaries not strictly increasing")
            passed = False
        else:
            print(f"  ✓ {feat:<20} bins monotonic  ({len(edges)-1} bins, "
                  f"range [{edges[0]:.3g}, {edges[-1]:.3g}])")

    # 2. No vocab id collisions (check offsets don't overlap)
    val_start = tok._val_offset
    val_end = val_start + N_BINS
    for feat, offset in tok._cat_offset.items():
        n_cats = len(tok.cat_vocabs.get(feat, []))
        cat_end = offset + n_cats
        if offset < val_end:
            print(f"  ✗ FAIL: {feat} categorical tokens overlap with value bins")
            passed = False
    print(f"  ✓ No vocab id collisions")

    # 3. Round-trip test
    n_rt_fail = 0
    for feat in NUMERICAL_FEATURES:
        if feat not in tok.bin_boundaries:
            continue
        edges = tok.bin_boundaries[feat]
        bin_width = (edges[-1] - edges[0]) / N_BINS
        lo, hi = edges[0], edges[-1]
        if feat not in df.columns:
            continue
        sample = df[feat].dropna().sample(min(300, df[feat].notna().sum()), random_state=42)
        n_in_range = 0
        for v in sample:
            use_log = tok.use_log.get(feat, False)
            v_scaled = np.log10(v) if use_log else float(v)
            # skip outliers — clipping to boundary bin is expected behaviour
            if not (lo <= v_scaled <= hi):
                continue
            n_in_range += 1
            encoded = tok._encode_numerical(feat, v)
            decoded = tok._decode_numerical(encoded, feat)
            if decoded is None:
                n_rt_fail += 1
                continue
            dec_s = np.log10(decoded) if use_log else decoded
            if abs(v_scaled - dec_s) > bin_width * 1.5:
                n_rt_fail += 1
    if n_rt_fail == 0:
        print(f"  ✓ Round-trip test passed on all in-range sampled values")
    else:
        print(f"  ✗ Round-trip failures on in-range values: {n_rt_fail}")
        passed = False

    # 4. UNK rate per column
    df2 = df.copy()
    if "st_spectype" in df2.columns:
        df2["st_spectype_broad"] = df2["st_spectype"].apply(_broad_spectype)
    for feat in FEATURE_ORDER:
        if feat not in df2.columns:
            continue
        non_null = df2[feat].dropna()
        if len(non_null) == 0:
            continue
        unk_count = sum(
            1 for v in non_null
            if tok._encode_value(feat, v) == SPECIAL_TOKENS["[UNK]"]
        )
        unk_rate = unk_count / len(non_null)
        status = "✓" if unk_rate < 0.05 else "✗ FAIL"
        if unk_rate >= 0.05:
            passed = False
        print(f"  {status} {feat:<25} UNK rate: {unk_rate:.1%}  "
              f"({len(non_null):,} non-null values)")

    print("── Scrub complete ──────────────────────────────────────────────\n")
    return passed


# ── CLI entry point ───────────────────────────────────────────────────────────

if __name__ == "__main__":
    import sys

    BASE = Path(__file__).parent.parent
    data_path = BASE / "data" / "nasa_deduped.csv"
    config_path = BASE / "tokenizer_config.json"

    print(f"Loading data from {data_path} ...")
    df = pd.read_csv(data_path, low_memory=False)

    # Hold out solar system planets for Phase 6 validation — never seen during tokenizer fit
    SOLAR_SYSTEM = ["Earth", "Mars", "Jupiter", "Saturn", "Venus", "Uranus", "Neptune"]
    train_df = df[~df["pl_name"].isin(SOLAR_SYSTEM)].reset_index(drop=True)
    print(f"Training rows (solar system held out): {len(train_df):,}")

    print("\nBuilding tokenizer from training split ...")
    tok = ExoplanetTokenizer()
    tok.build(train_df)

    # Scrub
    ok = scrub_tokenizer(tok, train_df)
    if not ok:
        print("Scrub FAILED — fix issues before proceeding.")
        sys.exit(1)

    tok.save(str(config_path))

    # Spot-check: encode a fake "Earth-like" row and print the sequence
    print("\nSpot-check — encoding an Earth-like planet row:")
    earth_like = {
        "pl_orbper": 365.25,
        "pl_orbsmax": 1.0,
        "pl_orbeccen": 0.017,
        "pl_bmassj": 0.00315,   # Earth mass in Jupiter masses
        "pl_radj": 0.0892,       # Earth radius in Jupiter radii
        "pl_eqt": 255.0,
        "st_teff": 5778.0,
        "st_mass": 1.0,
        "st_rad": 1.0,
        "st_lum": 0.0,           # log10(1) = 0
        "st_met": 0.0,
        "discoverymethod": "Transit",
        "st_spectype": "G2 V",
    }
    encoded = tok.encode(earth_like)
    print(f"  Sequence length: {len(encoded['input_ids'])} tokens")
    print(f"  Vocab size: {tok.vocab_size}")
    print(f"  Token ids: {encoded['input_ids']}")

    # Decode each value back
    print("\n  Decoded values:")
    for i, feat in enumerate(FEATURE_ORDER):
        pos = 1 + i * 2 + 1
        token_id = encoded["input_ids"][pos]
        decoded = tok.decode_value(token_id, feat)
        original = earth_like.get(feat, "—")
        print(f"    {feat:<22} original={original!s:<12}  decoded={decoded}")
