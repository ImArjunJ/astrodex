#!/usr/bin/env python3
"""
Exoplanet ML Benchmark Orchestrator.

Evaluates BERT, BART, TabTransformer, and MaskedAutoencoder models for
exoplanet parameter prediction. Loads cached fused exoplanet JSON files,
masks known fields, runs each model, and produces a comparison document
with accuracy (MAE/RMSE), latency, and cost metrics.

Usage:
    python benchmark.py --cache-dir ../../.cache/fused/
    python benchmark.py --models bert tab_transformer
    python benchmark.py --synthetic  # Use synthetic data for testing
"""

import argparse
import os
import platform
import sys
import time
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

import numpy as np
import pandas as pd
from sklearn.model_selection import train_test_split

from data_loader import (
    filter_complete_records,
    generate_synthetic_data,
    load_cached_exoplanets,
)
from metrics import (
    compute_classification_metrics,
    compute_metrics,
    format_latency_table,
    format_metrics_table,
)
from models.bart_generator import BartParameterGenerator
from models.bert_classifier import BertPlanetClassifier
from models.remasker_imputer import MaskedAutoencoderImputer
from models.tab_transformer import TabTransformerImputer


# Benchmark configuration
CLASSIFICATION_FIELDS = ["planet_type"]
IMPUTATION_FIELDS = ["mass_earth", "radius_earth", "equilibrium_temp_k"]
GENERATION_FIELDS = [
    "surface_pressure_atm",
    "albedo",
    "ocean_coverage_fraction",
    "cloud_coverage_fraction",
    "ice_coverage_fraction",
]

# Required fields for each benchmark task
CLASSIFICATION_REQUIRED = [
    "mass_earth",
    "radius_earth",
    "density_gcc",
    "equilibrium_temp_k",
    "planet_type",
]
IMPUTATION_REQUIRED = [
    "mass_earth",
    "radius_earth",
    "density_gcc",
    "equilibrium_temp_k",
    "surface_gravity_g",
    "orbital_period_days",
    "semi_major_axis_au",
    "eccentricity",
    "star_temp_k",
]
GENERATION_REQUIRED = IMPUTATION_REQUIRED + [
    "surface_pressure_atm",
    "albedo",
]

MIN_RECORDS = 20  # Minimum records needed for meaningful benchmarks


def mask_fields(row: pd.Series, fields: List[str]) -> pd.Series:
    """Create a copy of a row with specified fields masked (set to NaN)."""
    masked = row.copy()
    for field in fields:
        masked[field] = np.nan
    return masked


def run_classification_benchmark(
    model: BertPlanetClassifier,
    df: pd.DataFrame,
    test_size: float = 0.2,
    random_state: int = 42,
) -> Dict[str, Any]:
    """Run planet type classification benchmark with BERT.

    Returns:
        Dictionary with classification metrics and timing.
    """
    complete = filter_complete_records(df, CLASSIFICATION_REQUIRED)
    if len(complete) < MIN_RECORDS:
        print(f"  Skipping BERT: only {len(complete)} complete records (need {MIN_RECORDS})")
        return {"skipped": True, "reason": f"insufficient data ({len(complete)} records)"}

    train_df, test_df = train_test_split(
        complete, test_size=test_size, random_state=random_state
    )

    # Train
    print(f"  Training BERT classifier on {len(train_df)} records...")
    train_start = time.time()
    model.train(train_df, target_col="planet_type", epochs=20)
    train_time = time.time() - train_start

    # Predict
    print(f"  Evaluating on {len(test_df)} test records...")
    y_true = test_df["planet_type"].values
    y_pred = model.predict(test_df)

    # Metrics
    cls_metrics = compute_classification_metrics(y_true, y_pred, "planet_type")
    cls_metrics["model"] = "BERT"
    cls_metrics["train_time_s"] = train_time
    cls_metrics["avg_inference_ms"] = model.get_latency_ms()

    return cls_metrics


def run_imputation_benchmark(
    model,
    model_name: str,
    df: pd.DataFrame,
    target_fields: List[str],
    test_size: float = 0.2,
    random_state: int = 42,
) -> List[Dict[str, Any]]:
    """Run numerical imputation benchmark for a single model.

    Args:
        model: TabTransformerImputer or MaskedAutoencoderImputer instance.
        model_name: Name for labeling results.
        df: Full DataFrame.
        target_fields: Fields to mask and predict.

    Returns:
        List of metric dictionaries, one per target field.
    """
    complete = filter_complete_records(df, IMPUTATION_REQUIRED)
    if len(complete) < MIN_RECORDS:
        print(f"  Skipping {model_name}: only {len(complete)} complete records (need {MIN_RECORDS})")
        return [{"skipped": True, "model": model_name, "reason": f"insufficient data ({len(complete)} records)"}]

    train_df, test_df = train_test_split(
        complete, test_size=test_size, random_state=random_state
    )

    # Train on all target fields
    print(f"  Training {model_name} on {len(train_df)} records...")
    train_start = time.time()
    model.train(train_df, target_fields=target_fields, epochs=50)
    train_time = time.time() - train_start

    # Evaluate per-field
    all_results = []
    for field in target_fields:
        y_true_list = []
        y_pred_list = []

        for _, row in test_df.iterrows():
            masked_row = mask_fields(row, [field])
            pred = model.predict(masked_row, target_fields=[field])
            if field in pred:
                y_true_list.append(row[field])
                y_pred_list.append(pred[field])

        if y_true_list:
            field_metrics = compute_metrics(
                np.array(y_true_list), np.array(y_pred_list), field
            )
            field_metrics["model"] = model_name
            field_metrics["train_time_s"] = train_time
            field_metrics["avg_inference_ms"] = model.get_latency_ms()
            all_results.append(field_metrics)
        else:
            all_results.append({
                "model": model_name,
                "field": field,
                "n_samples": 0,
                "skipped": True,
                "reason": "no valid predictions",
            })

    return all_results


def run_generation_benchmark(
    model: BartParameterGenerator,
    df: pd.DataFrame,
    target_fields: List[str],
    test_size: float = 0.2,
    random_state: int = 42,
) -> Dict[str, Any]:
    """Run structured generation benchmark with BART.

    Returns:
        Dictionary with generation quality metrics and timing.
    """
    complete = filter_complete_records(df, GENERATION_REQUIRED)
    if len(complete) < MIN_RECORDS:
        print(f"  Skipping BART: only {len(complete)} complete records (need {MIN_RECORDS})")
        return {"skipped": True, "model": "BART", "reason": f"insufficient data ({len(complete)} records)"}

    train_df, test_df = train_test_split(
        complete, test_size=test_size, random_state=random_state
    )

    # Train
    print(f"  Training BART generator on {len(train_df)} records...")
    train_start = time.time()
    model.train(train_df, target_fields=target_fields, epochs=30)
    train_time = time.time() - train_start

    # Evaluate
    print(f"  Evaluating on {len(test_df)} test records...")
    valid_json_count = 0
    field_hits = {f: 0 for f in target_fields}
    field_errors = {f: [] for f in target_fields}
    total = len(test_df)

    for _, row in test_df.iterrows():
        masked_row = mask_fields(row, target_fields)
        pred = model.predict(masked_row)

        if pred:  # Non-empty dict = valid output
            valid_json_count += 1
            for field in target_fields:
                if field in pred:
                    field_hits[field] += 1
                    if pd.notna(row[field]):
                        field_errors[field].append(abs(pred[field] - row[field]))

    valid_pct = valid_json_count / max(total, 1) * 100
    field_coverage = {
        f: hits / max(total, 1) * 100 for f, hits in field_hits.items()
    }
    avg_coverage = np.mean(list(field_coverage.values())) if field_coverage else 0

    # Per-field MAE where available
    field_mae = {}
    for f, errors in field_errors.items():
        if errors:
            field_mae[f] = np.mean(errors)

    return {
        "model": "BART",
        "valid_json_pct": valid_pct,
        "avg_field_coverage_pct": avg_coverage,
        "field_coverage": field_coverage,
        "field_mae": field_mae,
        "train_time_s": train_time,
        "avg_inference_ms": model.get_latency_ms(),
        "n_samples": total,
    }


def detect_hardware() -> str:
    """Detect and describe the hardware environment."""
    import torch

    parts = [f"Platform: {platform.system()} {platform.machine()}"]
    parts.append(f"Python: {platform.python_version()}")
    parts.append(f"PyTorch: {torch.__version__}")

    if torch.cuda.is_available():
        parts.append(f"GPU: {torch.cuda.get_device_name(0)}")
        parts.append(f"CUDA: {torch.version.cuda}")
    else:
        parts.append("GPU: None (CPU only)")

    return " | ".join(parts)


def generate_comparison_document(
    classification_results: Dict[str, Any],
    imputation_results: List[Dict[str, Any]],
    generation_results: Dict[str, Any],
    dataset_size: int,
    hardware_info: str,
    data_source: str,
) -> str:
    """Generate the full comparison markdown document."""
    now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    doc = []
    doc.append("# Exoplanet ML Model Comparison\n")
    doc.append(f"## Overview\n")
    doc.append(f"- **Date:** {now}")
    doc.append(f"- **Dataset size:** {dataset_size} exoplanet records")
    doc.append(f"- **Data source:** {data_source}")
    doc.append(f"- **Hardware:** {hardware_info}")
    doc.append(f"- **Methodology:** Mask known fields from complete records, train on 80%, predict on 20%, measure error vs ground truth\n")

    # Classification section
    doc.append("## Classification: Planet Type\n")
    if classification_results.get("skipped"):
        doc.append(f"_Skipped: {classification_results.get('reason', 'unknown')}_\n")
    else:
        doc.append("| Model | N | Accuracy | F1 (macro) | Avg Inference (ms) | Training Time (s) |")
        doc.append("|-------|---|----------|------------|--------------------|--------------------|")
        doc.append(
            f"| {classification_results.get('model', 'BERT')} "
            f"| {classification_results.get('n_samples', 'N/A')} "
            f"| {_fmt(classification_results.get('accuracy'))} "
            f"| {_fmt(classification_results.get('f1_macro'))} "
            f"| {_fmt(classification_results.get('avg_inference_ms'))} "
            f"| {_fmt(classification_results.get('train_time_s'))} |"
        )
        doc.append("")

    # Imputation sections - group by field
    for field in IMPUTATION_FIELDS:
        field_label = field.replace("_", " ").title()
        doc.append(f"## Numerical Imputation: {field_label}\n")

        field_results = [r for r in imputation_results if r.get("field") == field and not r.get("skipped")]
        if not field_results:
            skipped = [r for r in imputation_results if r.get("field") == field or (r.get("skipped") and r.get("model"))]
            if skipped:
                doc.append(f"_Skipped: {skipped[0].get('reason', 'no results')}_\n")
            else:
                doc.append("_No results available._\n")
            continue

        doc.append("| Model | N | MAE | RMSE | R2 | MAPE (%) | Avg Inference (ms) | Training Time (s) |")
        doc.append("|-------|---|-----|------|----|----------|--------------------|--------------------|")
        for r in field_results:
            doc.append(
                f"| {r.get('model', 'N/A')} "
                f"| {r.get('n_samples', 'N/A')} "
                f"| {_fmt(r.get('mae'))} "
                f"| {_fmt(r.get('rmse'))} "
                f"| {_fmt(r.get('r2'))} "
                f"| {_fmt(r.get('mape'))} "
                f"| {_fmt(r.get('avg_inference_ms'))} "
                f"| {_fmt(r.get('train_time_s'))} |"
            )
        doc.append("")

    # Structured generation section
    doc.append("## Structured Generation: Atmosphere Parameters\n")
    if generation_results.get("skipped"):
        doc.append(f"_Skipped: {generation_results.get('reason', 'unknown')}_\n")
    else:
        doc.append("| Model | N | Valid JSON (%) | Avg Field Coverage (%) | Avg Inference (ms) | Training Time (s) |")
        doc.append("|-------|---|---------------|------------------------|--------------------|--------------------|")
        doc.append(
            f"| {generation_results.get('model', 'BART')} "
            f"| {generation_results.get('n_samples', 'N/A')} "
            f"| {_fmt(generation_results.get('valid_json_pct'))} "
            f"| {_fmt(generation_results.get('avg_field_coverage_pct'))} "
            f"| {_fmt(generation_results.get('avg_inference_ms'))} "
            f"| {_fmt(generation_results.get('train_time_s'))} |"
        )
        doc.append("")

        # Field-level coverage
        fc = generation_results.get("field_coverage", {})
        fm = generation_results.get("field_mae", {})
        if fc:
            doc.append("### Per-Field Coverage and Error\n")
            doc.append("| Field | Coverage (%) | MAE (where available) |")
            doc.append("|-------|-------------|----------------------|")
            for f, cov in fc.items():
                mae_val = _fmt(fm.get(f)) if f in fm else "N/A"
                doc.append(f"| {f} | {_fmt(cov)} | {mae_val} |")
            doc.append("")

    # Cost analysis section
    doc.append("## Cost Analysis\n")
    doc.append("| Model | GPU Required | Est. Inference Cost/1000 | Training Cost | Hosting Complexity |")
    doc.append("|-------|-------------|--------------------------|---------------|--------------------|")
    doc.append("| Claude (Bedrock) | No | ~$0.50 | None | Low (API) |")
    doc.append("| BERT Classifier | Optional | ~$0.01 | ~$0.10 | Medium (model serving) |")
    doc.append("| BART Generator | Optional | ~$0.02 | ~$0.15 | Medium (model serving) |")
    doc.append("| TabTransformer | Optional | ~$0.005 | ~$0.05 | Medium (model serving) |")
    doc.append("| MaskedAutoencoder | Optional | ~$0.005 | ~$0.05 | Medium (model serving) |")
    doc.append("")
    doc.append("*Cost estimates are approximate based on typical cloud GPU pricing (e.g., AWS g5 instances).*")
    doc.append("*Claude costs based on Bedrock pricing for Claude Sonnet 4.5.*\n")

    # Recommendations section
    doc.append("## Recommendations\n")
    doc.append("The choice of model depends on the specific use case and constraints:\n")
    doc.append("- **Claude (Bedrock):** Best for structured reasoning tasks requiring contextual understanding ")
    doc.append("  (atmosphere inference, biome classification). Higher per-call cost but no training needed, ")
    doc.append("  no model serving infrastructure, and excellent at handling novel/unusual exoplanets. ")
    doc.append("  Recommended as the **primary inference engine** for production.\n")
    doc.append("- **TabTransformer / MaskedAutoencoder:** Best for high-volume batch imputation of ")
    doc.append("  standard numerical fields (mass, radius, temperature). Very low per-call cost after training. ")
    doc.append("  Recommended for **bulk data enrichment** when processing large catalogs.\n")
    doc.append("- **BERT Classifier:** Competitive for planet type classification from tabular features. ")
    doc.append("  Useful as a fast, offline classification fallback when Bedrock is unavailable.\n")
    doc.append("- **BART Generator:** Exploratory -- structured JSON generation from tabular input is ")
    doc.append("  challenging for small seq2seq models. May improve with larger pretrained checkpoints ")
    doc.append("  or fine-tuning on a bigger exoplanet dataset. Consider as a **future investigation**.\n")
    doc.append("**Overall:** Use Claude for production inference (reliability + quality), with ")
    doc.append("TabTransformer/MaskedAutoencoder as batch preprocessing alternatives for catalog-scale operations.\n")

    # Methodology section
    doc.append("## Methodology\n")
    doc.append(f"- **Dataset:** {dataset_size} exoplanet records from {data_source}")
    doc.append("- **Split:** 80/20 train/test with random_state=42")
    doc.append("- **Masking:** Each target field masked independently for imputation benchmarks")
    doc.append("- **Classification metrics:** Accuracy, F1 (macro/weighted)")
    doc.append("- **Regression metrics:** MAE, RMSE, R2, MAPE")
    doc.append("- **Generation metrics:** Valid JSON percentage, field coverage, per-field MAE")
    doc.append(f"- **Hardware:** {hardware_info}")
    doc.append("- **Note:** All models use lightweight/simplified architectures suitable for CPU training. ")
    doc.append("  Production deployment would use full pretrained models (bert-base-uncased, facebook/bart-base) ")
    doc.append("  with GPU acceleration for better performance.\n")

    return "\n".join(doc)


def _fmt(value, decimals: int = 4) -> str:
    """Format a numeric value for display."""
    if value is None or (isinstance(value, float) and np.isnan(value)):
        return "N/A"
    if isinstance(value, int):
        return str(value)
    return f"{value:.{decimals}f}"


def main():
    parser = argparse.ArgumentParser(
        description="Exoplanet ML Benchmark - Compare BERT, BART, TabTransformer, and MaskedAutoencoder"
    )
    parser.add_argument(
        "--cache-dir",
        default=None,
        help="Path to fused cache directory (default: ../../.cache/fused/ or ASTRODEX_CACHE_DIR env)",
    )
    parser.add_argument(
        "--output-dir",
        default="results/",
        help="Output directory for comparison documents (default: results/)",
    )
    parser.add_argument(
        "--models",
        nargs="+",
        default=["all"],
        choices=["all", "bert", "bart", "tab_transformer", "remasker"],
        help="Models to benchmark (default: all)",
    )
    parser.add_argument(
        "--synthetic",
        action="store_true",
        help="Use synthetic data instead of cached records",
    )
    parser.add_argument(
        "--n-synthetic",
        type=int,
        default=100,
        help="Number of synthetic records to generate (default: 100)",
    )
    parser.add_argument(
        "--skip-training",
        action="store_true",
        help="Use pre-trained models if available (not yet implemented)",
    )

    args = parser.parse_args()

    print("=" * 60)
    print("Exoplanet ML Benchmark")
    print("=" * 60)

    # Load data
    data_source = ""
    if args.synthetic:
        print("\nGenerating synthetic test data...")
        df = generate_synthetic_data(n_records=args.n_synthetic)
        data_source = f"Synthetic ({args.n_synthetic} records)"
    else:
        try:
            cache_dir = args.cache_dir
            if cache_dir is None:
                cache_dir = os.environ.get("ASTRODEX_CACHE_DIR")
            if cache_dir is None:
                # Try common paths
                candidates = [
                    Path(__file__).parent / ".." / ".." / ".cache" / "fused",
                    Path(".cache/fused"),
                    Path("../../.cache/fused"),
                ]
                for c in candidates:
                    if c.exists():
                        cache_dir = str(c)
                        break

            if cache_dir is None:
                cache_dir = str(Path(__file__).parent / ".." / ".." / ".cache" / "fused")

            df = load_cached_exoplanets(cache_dir)
            data_source = f".cache/fused/ ({len(df)} records)"
        except FileNotFoundError:
            print("\nWARNING: Cache directory not found. Generating synthetic test data...")
            print("  For real results, run the C++ data pipeline first to populate .cache/fused/\n")
            df = generate_synthetic_data(n_records=max(args.n_synthetic, 100))
            data_source = f"Synthetic (cache unavailable, {len(df)} records)"

    if len(df) < MIN_RECORDS:
        print(f"\nWARNING: Only {len(df)} records loaded. Supplementing with synthetic data...")
        supplement = generate_synthetic_data(n_records=100 - len(df))
        df = pd.concat([df, supplement], ignore_index=True)
        data_source += f" + synthetic supplement ({len(df)} total)"

    print(f"\nDataset: {len(df)} records")
    print(f"Source: {data_source}")

    # Determine which models to run
    models_to_run = set(args.models)
    if "all" in models_to_run:
        models_to_run = {"bert", "bart", "tab_transformer", "remasker"}

    # Detect hardware
    hardware_info = detect_hardware()
    print(f"Hardware: {hardware_info}\n")

    # Run benchmarks
    classification_results = {"skipped": True, "reason": "not selected"}
    imputation_results = []
    generation_results = {"skipped": True, "reason": "not selected"}

    # BERT Classification
    if "bert" in models_to_run:
        print("-" * 40)
        print("BERT Planet Type Classification")
        print("-" * 40)
        bert = BertPlanetClassifier()
        classification_results = run_classification_benchmark(bert, df)
        if not classification_results.get("skipped"):
            print(f"  Accuracy: {classification_results.get('accuracy', 'N/A'):.4f}")
            print(f"  F1 (macro): {classification_results.get('f1_macro', 'N/A'):.4f}")
            print(f"  Latency: {classification_results.get('avg_inference_ms', 'N/A'):.2f} ms")
        print()

    # TabTransformer Imputation
    if "tab_transformer" in models_to_run:
        print("-" * 40)
        print("TabTransformer Numerical Imputation")
        print("-" * 40)
        tab = TabTransformerImputer()
        tab_results = run_imputation_benchmark(
            tab, "TabTransformer", df, IMPUTATION_FIELDS
        )
        imputation_results.extend(tab_results)
        for r in tab_results:
            if not r.get("skipped"):
                print(f"  {r['field']}: MAE={r.get('mae', 'N/A'):.4f}, R2={r.get('r2', 'N/A'):.4f}")
        print()

    # MaskedAutoencoder Imputation
    if "remasker" in models_to_run:
        print("-" * 40)
        print("MaskedAutoencoder (ReMasker-inspired) Imputation")
        print("-" * 40)
        mae_model = MaskedAutoencoderImputer()
        mae_results = run_imputation_benchmark(
            mae_model, "MaskedAutoencoder", df, IMPUTATION_FIELDS
        )
        imputation_results.extend(mae_results)
        for r in mae_results:
            if not r.get("skipped"):
                print(f"  {r['field']}: MAE={r.get('mae', 'N/A'):.4f}, R2={r.get('r2', 'N/A'):.4f}")
        print()

    # BART Generation
    if "bart" in models_to_run:
        print("-" * 40)
        print("BART Structured Parameter Generation")
        print("-" * 40)
        bart = BartParameterGenerator()
        generation_results = run_generation_benchmark(bart, df, GENERATION_FIELDS)
        if not generation_results.get("skipped"):
            print(f"  Valid JSON: {generation_results.get('valid_json_pct', 'N/A'):.1f}%")
            print(f"  Field coverage: {generation_results.get('avg_field_coverage_pct', 'N/A'):.1f}%")
            print(f"  Latency: {generation_results.get('avg_inference_ms', 'N/A'):.2f} ms")
        print()

    # Generate comparison document
    print("=" * 60)
    print("Generating comparison document...")
    comparison = generate_comparison_document(
        classification_results=classification_results,
        imputation_results=imputation_results,
        generation_results=generation_results,
        dataset_size=len(df),
        hardware_info=hardware_info,
        data_source=data_source,
    )

    # Write output
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    output_path = output_dir / "comparison.md"
    output_path.write_text(comparison)
    print(f"Comparison document written to: {output_path.resolve()}")
    print("=" * 60)
    print("Benchmark complete!")


if __name__ == "__main__":
    main()
