"""
Evaluation metrics for the exoplanet ML benchmarking harness.

Provides MAE, RMSE, R2, MAPE for numerical regression tasks and
accuracy/F1 for classification tasks, plus markdown table formatting.
"""

from typing import Any, Dict, List, Optional

import numpy as np
from sklearn.metrics import (
    accuracy_score,
    f1_score,
    mean_absolute_error,
    mean_squared_error,
    r2_score,
)


def compute_metrics(
    y_true: np.ndarray,
    y_pred: np.ndarray,
    field_name: str,
) -> Dict[str, Any]:
    """Compute MAE, RMSE, R2, and MAPE for a single numerical field.

    Args:
        y_true: Ground truth values.
        y_pred: Predicted values.
        field_name: Name of the field being evaluated (for labeling).

    Returns:
        Dictionary with field_name, mae, rmse, r2, mape.
    """
    y_true = np.asarray(y_true, dtype=float)
    y_pred = np.asarray(y_pred, dtype=float)

    # Filter out NaN pairs
    valid = ~(np.isnan(y_true) | np.isnan(y_pred))
    y_true = y_true[valid]
    y_pred = y_pred[valid]

    if len(y_true) == 0:
        return {
            "field": field_name,
            "n_samples": 0,
            "mae": np.nan,
            "rmse": np.nan,
            "r2": np.nan,
            "mape": np.nan,
        }

    mae = mean_absolute_error(y_true, y_pred)
    rmse = np.sqrt(mean_squared_error(y_true, y_pred))
    r2 = r2_score(y_true, y_pred) if len(y_true) > 1 else np.nan

    # MAPE: avoid division by zero
    nonzero = y_true != 0
    if nonzero.any():
        mape = np.mean(np.abs((y_true[nonzero] - y_pred[nonzero]) / y_true[nonzero])) * 100
    else:
        mape = np.nan

    return {
        "field": field_name,
        "n_samples": len(y_true),
        "mae": mae,
        "rmse": rmse,
        "r2": r2,
        "mape": mape,
    }


def compute_classification_metrics(
    y_true: np.ndarray,
    y_pred: np.ndarray,
    field_name: str,
) -> Dict[str, Any]:
    """Compute accuracy and F1 for a classification task.

    Args:
        y_true: Ground truth labels.
        y_pred: Predicted labels.
        field_name: Name of the field being evaluated.

    Returns:
        Dictionary with field_name, accuracy, f1_macro, f1_weighted.
    """
    y_true = np.asarray(y_true)
    y_pred = np.asarray(y_pred)

    if len(y_true) == 0:
        return {
            "field": field_name,
            "n_samples": 0,
            "accuracy": np.nan,
            "f1_macro": np.nan,
            "f1_weighted": np.nan,
        }

    accuracy = accuracy_score(y_true, y_pred)
    f1_mac = f1_score(y_true, y_pred, average="macro", zero_division=0)
    f1_wt = f1_score(y_true, y_pred, average="weighted", zero_division=0)

    return {
        "field": field_name,
        "n_samples": len(y_true),
        "accuracy": accuracy,
        "f1_macro": f1_mac,
        "f1_weighted": f1_wt,
    }


def format_metrics_table(
    all_results: List[Dict[str, Any]],
    task_type: str = "regression",
) -> str:
    """Format evaluation results into a markdown table.

    Args:
        all_results: List of metric dictionaries from compute_metrics or
            compute_classification_metrics.
        task_type: Either 'regression' or 'classification'.

    Returns:
        Markdown-formatted table string.
    """
    if not all_results:
        return "_No results to display._\n"

    if task_type == "classification":
        header = "| Model | N | Accuracy | F1 (macro) | F1 (weighted) |"
        separator = "|-------|---|----------|------------|---------------|"
        rows = []
        for r in all_results:
            rows.append(
                f"| {r.get('model', r.get('field', 'N/A'))} "
                f"| {r.get('n_samples', 'N/A')} "
                f"| {_fmt(r.get('accuracy'))} "
                f"| {_fmt(r.get('f1_macro'))} "
                f"| {_fmt(r.get('f1_weighted'))} |"
            )
    else:
        header = "| Model | Field | N | MAE | RMSE | R2 | MAPE (%) |"
        separator = "|-------|-------|---|-----|------|----|----------|"
        rows = []
        for r in all_results:
            rows.append(
                f"| {r.get('model', 'N/A')} "
                f"| {r.get('field', 'N/A')} "
                f"| {r.get('n_samples', 'N/A')} "
                f"| {_fmt(r.get('mae'))} "
                f"| {_fmt(r.get('rmse'))} "
                f"| {_fmt(r.get('r2'))} "
                f"| {_fmt(r.get('mape'))} |"
            )

    return "\n".join([header, separator] + rows) + "\n"


def format_latency_table(
    latency_results: List[Dict[str, Any]],
) -> str:
    """Format latency and cost results into a markdown table.

    Args:
        latency_results: List of dicts with model, avg_inference_ms,
            train_time_s.

    Returns:
        Markdown-formatted table string.
    """
    header = "| Model | Avg Inference (ms) | Training Time (s) |"
    separator = "|-------|--------------------|--------------------|"
    rows = []
    for r in latency_results:
        rows.append(
            f"| {r.get('model', 'N/A')} "
            f"| {_fmt(r.get('avg_inference_ms'))} "
            f"| {_fmt(r.get('train_time_s'))} |"
        )
    return "\n".join([header, separator] + rows) + "\n"


def _fmt(value: Optional[float], decimals: int = 4) -> str:
    """Format a numeric value for display, handling NaN."""
    if value is None or (isinstance(value, float) and np.isnan(value)):
        return "N/A"
    if isinstance(value, int):
        return str(value)
    return f"{value:.{decimals}f}"
