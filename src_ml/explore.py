"""
Phase 1: Data Acquisition & Exploration
Produces:
  - data/nasa_deduped.csv     — one best row per planet (highest measurement coverage)
  - data/missingness.csv      — per-column fill rate
  - data/correlations.csv     — Pearson correlation matrix on log-scaled numerics
  - prints a summary to stdout
"""

import pandas as pd
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import os, warnings
warnings.filterwarnings("ignore")

DATA_DIR = os.path.join(os.path.dirname(__file__), "..", "data")

NUMERICAL_COLS = [
    "pl_orbper", "pl_orbsmax", "pl_orbeccen",
    "pl_bmassj", "pl_radj", "pl_dens",
    "pl_eqt", "pl_insol",
    "st_teff", "st_mass", "st_rad", "st_lum", "st_met",
    "pl_orbincl",
]

CATEGORICAL_COLS = ["discoverymethod", "st_spectype"]

ALL_FEATURE_COLS = NUMERICAL_COLS + CATEGORICAL_COLS


def load_and_deduplicate(path: str) -> pd.DataFrame:
    """
    NASA ps table has multiple rows per planet (one per reference paper).
    Keep the row with the most non-null values across our feature columns.
    """
    df = pd.read_csv(path, low_memory=False)
    print(f"Raw rows: {len(df):,}  |  Unique planets: {df['pl_name'].nunique():,}")

    # Score each row by how many feature columns are non-null
    feature_cols_present = [c for c in ALL_FEATURE_COLS if c in df.columns]
    df["_coverage"] = df[feature_cols_present].notna().sum(axis=1)
    df = (
        df.sort_values("_coverage", ascending=False)
          .drop_duplicates(subset="pl_name", keep="first")
          .drop(columns=["_coverage"])
          .reset_index(drop=True)
    )
    print(f"After dedup (best-coverage row per planet): {len(df):,} planets\n")
    return df


def missingness_report(df: pd.DataFrame) -> pd.DataFrame:
    """Compute fill rate for each feature column."""
    cols = [c for c in ALL_FEATURE_COLS if c in df.columns]
    total = len(df)
    records = []
    for c in cols:
        n_filled = df[c].notna().sum()
        records.append({
            "column": c,
            "n_filled": int(n_filled),
            "fill_rate": round(n_filled / total, 4),
            "type": "numerical" if c in NUMERICAL_COLS else "categorical",
        })
    report = pd.DataFrame(records).sort_values("fill_rate", ascending=False)
    return report


def plot_missingness(report: pd.DataFrame, out_path: str):
    fig, ax = plt.subplots(figsize=(10, 6))
    colors = ["#4C9BE8" if t == "numerical" else "#E88A4C" for t in report["type"]]
    ax.barh(report["column"], report["fill_rate"], color=colors)
    ax.axvline(0.2, color="red", linestyle="--", linewidth=1, label="20% threshold")
    ax.set_xlabel("Fill rate (fraction of planets with value)")
    ax.set_title("NASA Exoplanet Archive — Feature Coverage")
    ax.legend()
    plt.tight_layout()
    plt.savefig(out_path, dpi=150)
    plt.close()
    print(f"Saved missingness plot → {out_path}")


def log_scale_df(df: pd.DataFrame, cols: list) -> pd.DataFrame:
    """Log₁₀-scale positive numerical columns. Columns with negatives get skipped."""
    df_log = df[cols].copy()
    for c in cols:
        series = df_log[c].dropna()
        if (series > 0).all():
            df_log[c] = np.log10(df_log[c])
        # columns with zeros/negatives (e.g. st_met, pl_orbeccen) left as-is
    return df_log


def plot_correlations(df: pd.DataFrame, cols: list, out_path: str) -> pd.DataFrame:
    df_log = log_scale_df(df, cols)
    corr = df_log.corr(method="pearson")

    fig, ax = plt.subplots(figsize=(10, 8))
    im = ax.imshow(corr, cmap="RdBu_r", vmin=-1, vmax=1)
    ax.set_xticks(range(len(cols)))
    ax.set_yticks(range(len(cols)))
    ax.set_xticklabels(cols, rotation=45, ha="right", fontsize=8)
    ax.set_yticklabels(cols, fontsize=8)
    plt.colorbar(im, ax=ax, label="Pearson r (log-scaled values)")
    ax.set_title("Feature Correlation Matrix")
    plt.tight_layout()
    plt.savefig(out_path, dpi=150)
    plt.close()
    print(f"Saved correlation plot → {out_path}")
    return corr


def plot_key_scatter(df: pd.DataFrame, out_path: str):
    """Mass-radius and equilibrium temp vs orbital period scatter plots."""
    fig, axes = plt.subplots(1, 2, figsize=(12, 5))

    # Mass-radius
    ax = axes[0]
    subset = df[["pl_bmassj", "pl_radj"]].dropna()
    ax.scatter(np.log10(subset["pl_bmassj"]), subset["pl_radj"],
               alpha=0.3, s=10, c="#4C9BE8")
    ax.set_xlabel("log₁₀(Mass / Mⱼ)")
    ax.set_ylabel("Radius (Rⱼ)")
    ax.set_title(f"Mass vs Radius  (n={len(subset):,})")

    # Equilibrium temp vs orbital period
    ax = axes[1]
    subset2 = df[["pl_eqt", "pl_orbper"]].dropna()
    subset2 = subset2[subset2["pl_orbper"] > 0]
    ax.scatter(np.log10(subset2["pl_orbper"]), subset2["pl_eqt"],
               alpha=0.3, s=10, c="#E88A4C")
    ax.set_xlabel("log₁₀(Orbital Period / days)")
    ax.set_ylabel("Equilibrium Temperature (K)")
    ax.set_title(f"Orbital Period vs Eq. Temp  (n={len(subset2):,})")

    plt.tight_layout()
    plt.savefig(out_path, dpi=150)
    plt.close()
    print(f"Saved scatter plots → {out_path}")


def categorical_summary(df: pd.DataFrame):
    for col in CATEGORICAL_COLS:
        if col not in df.columns:
            continue
        vc = df[col].value_counts()
        print(f"\n{col} ({df[col].notna().sum()} filled):")
        for val, cnt in vc.head(10).items():
            pct = cnt / len(df) * 100
            print(f"  {val:<30} {cnt:>5}  ({pct:.1f}%)")


def main():
    raw_path = os.path.join(DATA_DIR, "nasa_archive.csv")
    deduped_path = os.path.join(DATA_DIR, "nasa_deduped.csv")

    df = load_and_deduplicate(raw_path)
    df.to_csv(deduped_path, index=False)
    print(f"Saved deduped archive → {deduped_path}\n")

    # --- Missingness report ---
    report = missingness_report(df)
    miss_path = os.path.join(DATA_DIR, "missingness.csv")
    report.to_csv(miss_path, index=False)

    print("=" * 55)
    print(f"{'Column':<20} {'Fill Rate':>10}  {'N filled':>8}")
    print("-" * 55)
    for _, row in report.iterrows():
        bar = "█" * int(row["fill_rate"] * 30)
        flag = " ⚠ <20%" if row["fill_rate"] < 0.2 else ""
        print(f"{row['column']:<20} {row['fill_rate']:>9.1%}  {row['n_filled']:>8,}  {bar}{flag}")
    print("=" * 55)

    # Columns above 20% threshold (usable for training)
    usable = report[report["fill_rate"] >= 0.2]["column"].tolist()
    print(f"\nUsable columns (>20% fill): {usable}\n")

    # --- Plots ---
    plot_missingness(report, os.path.join(DATA_DIR, "missingness.png"))
    num_cols_present = [c for c in NUMERICAL_COLS if c in df.columns and df[c].notna().sum() > 100]
    corr = plot_correlations(df, num_cols_present, os.path.join(DATA_DIR, "correlations.png"))
    corr.to_csv(os.path.join(DATA_DIR, "correlations.csv"))
    plot_key_scatter(df, os.path.join(DATA_DIR, "scatter.png"))

    # --- Categorical breakdown ---
    categorical_summary(df)

    # --- Strongest correlations summary ---
    print("\nTop 10 strongest feature correlations (absolute Pearson r, log-scaled):")
    corr_abs = corr.abs()
    pairs = []
    cols_c = corr_abs.columns.tolist()
    for i in range(len(cols_c)):
        for j in range(i + 1, len(cols_c)):
            pairs.append((cols_c[i], cols_c[j], corr_abs.iloc[i, j]))
    pairs.sort(key=lambda x: x[2], reverse=True)
    for a, b, r in pairs[:10]:
        print(f"  {a:<18} ↔  {b:<18}  r = {r:.3f}")

    print("\nPhase 1 complete. Check data/ for outputs.")


if __name__ == "__main__":
    main()
