"""
Phase 4: Training loop.

Usage:
    python src_ml/train.py

Outputs:
    checkpoints/bert_imputer_best.pt   — best val-loss checkpoint
    checkpoints/training_log.csv       — per-epoch metrics
"""

import sys
import csv
import time
import random
from pathlib import Path

import numpy as np
import pandas as pd
import torch
import torch.nn as nn
from torch.utils.data import DataLoader

BASE = Path(__file__).parent.parent
sys.path.insert(0, str(Path(__file__).parent))

from tokenizer import ExoplanetTokenizer, NUMERICAL_FEATURES, CATEGORICAL_FEATURES, FEATURE_ORDER
from dataset  import ExoplanetDataset, collate_fn, make_splits, SOLAR_SYSTEM_PLANETS
from model    import ExoplanetBERT, ImputationLoss, build_model

# ── Config ────────────────────────────────────────────────────────────────────

SEED          = 42
EPOCHS        = 80
BATCH_SIZE    = 64
LR            = 1e-3
WEIGHT_DECAY  = 0.01
WARMUP_FRAC   = 0.10     # fraction of total steps used for linear warmup
SOLAR_COPIES  = 20       # upsampling for solar system planets
CHECKPOINT_DIR = BASE / "checkpoints"

# ── Reproducibility ───────────────────────────────────────────────────────────

def set_seed(seed):
    random.seed(seed)
    np.random.seed(seed)
    torch.manual_seed(seed)

# ── Scheduler ─────────────────────────────────────────────────────────────────

def make_scheduler(optimizer, total_steps: int, warmup_steps: int):
    def lr_lambda(step):
        if step < warmup_steps:
            return step / max(1, warmup_steps)
        progress = (step - warmup_steps) / max(1, total_steps - warmup_steps)
        # cosine decay to 5% of peak lr
        return max(0.05, 0.5 * (1 + np.cos(np.pi * progress)))
    return torch.optim.lr_scheduler.LambdaLR(optimizer, lr_lambda)

# ── Per-feature MAE ───────────────────────────────────────────────────────────

@torch.no_grad()
def compute_per_feature_mae(model, loader, tokenizer, device):
    """
    For each numerical feature, compute MAE in physical units on masked positions.
    This is the key accuracy metric — lower is better.
    """
    model.eval()
    val_offset = tokenizer._val_offset

    errors   = {f: [] for f in NUMERICAL_FEATURES}
    baseline = {f: [] for f in NUMERICAL_FEATURES}  # median imputation baseline

    # Precompute per-feature medians from the tokenizer bin centres
    # (we'll use the middle bin as a stand-in for the dataset median)
    median_bin = {}
    for feat in NUMERICAL_FEATURES:
        if feat in tokenizer.bin_boundaries:
            mid = len(tokenizer.bin_boundaries[feat]) // 2
            median_bin[feat] = tokenizer.decode_value(val_offset + mid // 2, feat)

    for batch in loader:
        input_ids    = batch["input_ids"].to(device)
        attn_mask    = batch["attention_mask"].to(device)
        target_ids   = batch["target_ids"]
        num_mask     = batch["numerical_mask"]

        out = model(input_ids, attn_mask)
        reg_logits = out["regression_logits"].cpu()  # (B, L, n_bins)

        pred_bins = reg_logits.argmax(dim=-1)  # (B, L)

        B, L = input_ids.shape
        for b in range(B):
            for pos in range(L):
                if not num_mask[b, pos]:
                    continue
                feat_idx = (pos - 2) // 2
                if not (0 <= feat_idx < len(FEATURE_ORDER)):
                    continue
                feat = FEATURE_ORDER[feat_idx]
                if feat not in NUMERICAL_FEATURES:
                    continue

                true_bin   = target_ids[b, pos].item()
                pred_bin   = pred_bins[b, pos].item()

                true_val = tokenizer.decode_value(val_offset + true_bin, feat)
                pred_val = tokenizer.decode_value(val_offset + pred_bin, feat)

                if true_val is None or pred_val is None:
                    continue

                errors[feat].append(abs(pred_val - true_val))

                med = median_bin.get(feat)
                if med is not None:
                    baseline[feat].append(abs(med - true_val))

    mae = {}
    for feat in NUMERICAL_FEATURES:
        if errors[feat]:
            mae[feat] = np.mean(errors[feat])
    return mae

# ── Training ──────────────────────────────────────────────────────────────────

def train():
    set_seed(SEED)
    CHECKPOINT_DIR.mkdir(exist_ok=True)
    device = torch.device("cuda" if torch.cuda.is_available() else
                          "mps"  if torch.backends.mps.is_available() else "cpu")
    print(f"Device: {device}")

    # Data
    tok = ExoplanetTokenizer.load(str(BASE / "tokenizer_config.json"))
    df  = pd.read_csv(BASE / "data" / "nasa_deduped.csv", low_memory=False)
    train_df, val_df, _ = make_splits(df, seed=SEED)

    train_ds = ExoplanetDataset(train_df, tok, mask_prob=0.25,
                                solar_upsample=SOLAR_COPIES, augment=True,  seed=SEED)
    val_ds   = ExoplanetDataset(val_df,   tok, mask_prob=0.25,
                                solar_upsample=0,            augment=False, seed=SEED)

    train_loader = DataLoader(train_ds, batch_size=BATCH_SIZE, shuffle=True,
                              collate_fn=collate_fn, num_workers=0)
    val_loader   = DataLoader(val_ds,   batch_size=BATCH_SIZE, shuffle=False,
                              collate_fn=collate_fn, num_workers=0)

    print(f"Train: {len(train_ds):,} samples  |  Val: {len(val_ds):,} samples")

    # Model
    model, cfg = build_model(tok)
    model.to(device)
    print(f"Model: {model.count_parameters():,} parameters")

    # Optimiser & scheduler
    optimizer = torch.optim.AdamW(model.parameters(), lr=LR, weight_decay=WEIGHT_DECAY)
    total_steps  = EPOCHS * len(train_loader)
    warmup_steps = int(total_steps * WARMUP_FRAC)
    scheduler = make_scheduler(optimizer, total_steps, warmup_steps)
    loss_fn   = ImputationLoss(lambda_cls=1.0, label_smoothing=0.05)

    best_val_loss = float("inf")
    log_rows = []

    print(f"\n{'Epoch':>5}  {'Train Loss':>10}  {'Val Loss':>10}  "
          f"{'mass MAE':>9}  {'radius MAE':>10}  {'temp MAE':>9}  {'LR':>8}  {'Time':>6}")
    print("-" * 85)

    for epoch in range(1, EPOCHS + 1):
        t0 = time.time()

        # ── Train ─────────────────────────────────────────────────────────────
        model.train()
        train_losses = []
        for batch in train_loader:
            input_ids  = batch["input_ids"].to(device)
            attn_mask  = batch["attention_mask"].to(device)
            target_ids = batch["target_ids"].to(device)
            num_mask   = batch["numerical_mask"].to(device)
            cat_masks  = {k: v.to(device) for k, v in batch["categorical_masks"].items()}

            pt_labels = batch["planet_type_labels"].to(device)
            optimizer.zero_grad()
            out = model(input_ids, attn_mask)
            losses = loss_fn(out, target_ids, num_mask, num_mask, cat_masks, pt_labels)
            losses["loss"].backward()
            nn.utils.clip_grad_norm_(model.parameters(), max_norm=1.0)
            optimizer.step()
            scheduler.step()
            train_losses.append(losses["loss"].item())

        # ── Validate ──────────────────────────────────────────────────────────
        model.eval()
        val_losses = []
        with torch.no_grad():
            for batch in val_loader:
                input_ids  = batch["input_ids"].to(device)
                attn_mask  = batch["attention_mask"].to(device)
                target_ids = batch["target_ids"].to(device)
                num_mask   = batch["numerical_mask"].to(device)
                cat_masks  = {k: v.to(device) for k, v in batch["categorical_masks"].items()}

                pt_labels = batch["planet_type_labels"].to(device)
                out = model(input_ids, attn_mask)
                losses = loss_fn(out, target_ids, num_mask, num_mask, cat_masks, pt_labels)
                val_losses.append(losses["loss"].item())

        train_loss = np.mean(train_losses)
        val_loss   = np.mean(val_losses)
        lr_now     = scheduler.get_last_lr()[0]
        elapsed    = time.time() - t0

        # Per-feature MAE every 10 epochs
        mass_mae = radius_mae = temp_mae = float("nan")
        if epoch % 10 == 0 or epoch == 1:
            mae = compute_per_feature_mae(model, val_loader, tok, device)
            mass_mae   = mae.get("pl_bmassj", float("nan"))
            radius_mae = mae.get("pl_radj",   float("nan"))
            temp_mae   = mae.get("pl_eqt",    float("nan"))

        print(f"{epoch:>5}  {train_loss:>10.4f}  {val_loss:>10.4f}  "
              f"{mass_mae:>9.4f}  {radius_mae:>10.4f}  {temp_mae:>9.1f}  "
              f"{lr_now:>8.2e}  {elapsed:>5.1f}s")

        log_rows.append({
            "epoch": epoch, "train_loss": train_loss, "val_loss": val_loss,
            "mass_mae": mass_mae, "radius_mae": radius_mae, "temp_mae": temp_mae,
            "lr": lr_now,
        })

        # Save best checkpoint
        if val_loss < best_val_loss:
            best_val_loss = val_loss
            ckpt = {
                "epoch": epoch,
                "model_state_dict": model.state_dict(),
                "optimizer_state_dict": optimizer.state_dict(),
                "val_loss": val_loss,
                "cfg": cfg,
            }
            torch.save(ckpt, CHECKPOINT_DIR / "bert_imputer_best.pt")

    # Save training log
    log_path = CHECKPOINT_DIR / "training_log.csv"
    with open(log_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=log_rows[0].keys())
        writer.writeheader()
        writer.writerows(log_rows)

    print(f"\nTraining complete.")
    print(f"Best val loss: {best_val_loss:.4f}")
    print(f"Checkpoint: {CHECKPOINT_DIR / 'bert_imputer_best.pt'}")
    print(f"Log:        {log_path}")


if __name__ == "__main__":
    train()
