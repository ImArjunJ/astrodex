"""
Phase 3: Double-headed BERT for exoplanet parameter imputation.

Architecture:
  - Token embedding (vocab_size → d_model)
  - Learned positional embedding (seq_len → d_model)
  - N transformer encoder layers (standard BERT-style)
  - Regression head: predicts bin index (float) at masked numerical positions
  - Classification head: predicts category logits at masked categorical positions

The model operates entirely in token space. Decoding back to physical units
is handled by ExoplanetTokenizer.decode_value().
"""

import math
import torch
import torch.nn as nn
from dataclasses import dataclass


PLANET_TYPES = [
    "EarthLike", "OceanWorld", "IceWorld", "DesertWorld",
    "LavaWorld", "SuperEarth", "MiniNeptune", "GasGiant", "HotJupiter"
]

@dataclass
class BertConfig:
    vocab_size: int        # from tokenizer.vocab_size
    seq_len: int           # fixed: 2 + 2 * n_features (34 for our feature set)
    d_model: int = 128     # embedding dimension
    n_heads: int = 4       # attention heads (d_model must be divisible)
    n_layers: int = 4      # transformer encoder layers
    d_ff: int = 512        # feed-forward hidden dimension
    dropout: float = 0.1
    n_numerical: int = 14  # number of numerical features
    n_bins: int = 200      # must match tokenizer N_BINS
    n_planet_types: int = 9  # EarthLike … HotJupiter
    # categorical output sizes — set after tokenizer is loaded
    cat_vocab_sizes: dict = None  # {feature_name: n_categories}


class ExoplanetBERT(nn.Module):
    """
    Double-headed BERT for astronomical data imputation.

    Inputs:
        input_ids:      (B, seq_len)  token ids — [MASK] where value is unknown
        attention_mask: (B, seq_len)  1 for real tokens, 0 for padding

    Outputs (dict):
        "regression_logits":     (B, seq_len, n_bins)   — bin scores at every position
        "classification_logits": {feat: (B, seq_len, n_cats)} — category scores
    """

    def __init__(self, cfg: BertConfig):
        super().__init__()
        self.cfg = cfg

        # ── Embeddings ───────────────────────────────────────────────────────
        self.token_emb = nn.Embedding(cfg.vocab_size, cfg.d_model, padding_idx=0)
        self.pos_emb   = nn.Embedding(cfg.seq_len, cfg.d_model)
        self.emb_norm  = nn.LayerNorm(cfg.d_model)
        self.emb_drop  = nn.Dropout(cfg.dropout)

        # ── Transformer encoder ──────────────────────────────────────────────
        encoder_layer = nn.TransformerEncoderLayer(
            d_model=cfg.d_model,
            nhead=cfg.n_heads,
            dim_feedforward=cfg.d_ff,
            dropout=cfg.dropout,
            activation="gelu",
            batch_first=True,
            norm_first=True,   # Pre-LN: more stable training on small datasets
        )
        self.encoder = nn.TransformerEncoder(encoder_layer, num_layers=cfg.n_layers)

        # ── Regression head (numerical features → bin index) ─────────────────
        # Shared projection, outputs a score over all N_BINS bins
        self.regression_head = nn.Sequential(
            nn.Linear(cfg.d_model, cfg.d_model),
            nn.GELU(),
            nn.LayerNorm(cfg.d_model),
            nn.Linear(cfg.d_model, cfg.n_bins),
        )

        # ── Classification head (categorical features → category logits) ─────
        # One small linear per categorical feature
        self.classification_heads = nn.ModuleDict()
        if cfg.cat_vocab_sizes:
            for feat, n_cats in cfg.cat_vocab_sizes.items():
                self.classification_heads[feat] = nn.Linear(cfg.d_model, n_cats)

        # ── Planet type head (sentence-level, uses [CLS] token) ──────────────
        # Predicts: EarthLike / OceanWorld / IceWorld / DesertWorld / LavaWorld
        #           SuperEarth / MiniNeptune / GasGiant / HotJupiter
        # This is the label the C++ renderer uses directly via PlanetTypes.hpp
        self.planet_type_head = nn.Sequential(
            nn.Linear(cfg.d_model, cfg.d_model // 2),
            nn.GELU(),
            nn.Dropout(cfg.dropout),
            nn.Linear(cfg.d_model // 2, cfg.n_planet_types),
        )

        self._init_weights()

    def _init_weights(self):
        for module in self.modules():
            if isinstance(module, nn.Linear):
                nn.init.trunc_normal_(module.weight, std=0.02)
                if module.bias is not None:
                    nn.init.zeros_(module.bias)
            elif isinstance(module, nn.Embedding):
                nn.init.trunc_normal_(module.weight, std=0.02)

    def forward(self, input_ids: torch.Tensor,
                attention_mask: torch.Tensor | None = None) -> dict:
        B, L = input_ids.shape

        positions = torch.arange(L, device=input_ids.device).unsqueeze(0)  # (1, L)
        x = self.token_emb(input_ids) + self.pos_emb(positions)
        x = self.emb_drop(self.emb_norm(x))

        # TransformerEncoder expects src_key_padding_mask as BoolTensor where
        # True = ignore. Our attention_mask is 1=keep, 0=pad — invert it.
        pad_mask = None
        if attention_mask is not None:
            pad_mask = (attention_mask == 0)

        x = self.encoder(x, src_key_padding_mask=pad_mask)  # (B, L, d_model)

        # Regression head: apply at every position, loss will select masked ones
        reg_logits = self.regression_head(x)  # (B, L, n_bins)

        # Classification heads: one per categorical feature
        cls_logits = {
            feat: head(x)
            for feat, head in self.classification_heads.items()
        }

        # Planet type: read from [CLS] position (index 0)
        planet_type_logits = self.planet_type_head(x[:, 0, :])  # (B, n_planet_types)

        return {
            "regression_logits": reg_logits,
            "classification_logits": cls_logits,
            "planet_type_logits": planet_type_logits,
            "hidden_states": x,
        }

    def count_parameters(self) -> int:
        return sum(p.numel() for p in self.parameters() if p.requires_grad)


# ── Loss ──────────────────────────────────────────────────────────────────────

class ImputationLoss(nn.Module):
    """
    Combined loss for the double-headed model.

    For numerical positions:  cross-entropy over bins (treats regression as
        classification over ordered bins — simpler and more stable than MSE
        directly on bin indices for small datasets).

    For categorical positions: cross-entropy over category vocabulary.

    Only computes loss on [MASK] positions (target_ids != -100).
    """

    def __init__(self, lambda_cls: float = 1.0, label_smoothing: float = 0.05):
        super().__init__()
        self.lambda_cls = lambda_cls
        # label smoothing helps on small datasets — reduces overconfidence
        self.ce = nn.CrossEntropyLoss(ignore_index=-100, label_smoothing=label_smoothing)

    def forward(self, outputs: dict, target_ids: torch.Tensor,
                mask_positions: torch.Tensor,
                numerical_mask: torch.Tensor,
                categorical_masks: dict[str, torch.Tensor],
                planet_type_labels: torch.Tensor | None = None) -> dict:
        """
        Args:
            outputs:           model forward() output dict
            target_ids:        (B, L) — ground truth token ids, -100 for non-masked
            mask_positions:    (B, L) bool — True where position is [MASK]
            numerical_mask:    (B, L) bool — True at masked *numerical* value positions
            categorical_masks: {feat: (B, L) bool} — True at masked *categorical* positions
        """
        reg_logits = outputs["regression_logits"]   # (B, L, n_bins)
        cls_logits = outputs["classification_logits"]

        # ── Regression loss ───────────────────────────────────────────────────
        # Target: the bin index of the true value, derived from true token id
        # token id = val_offset + bin_idx, so bin_idx = token_id - val_offset
        # We pass target_ids directly — CE ignores -100
        if numerical_mask.any():
            # Zero out categorical targets at numerical positions (keep -100 elsewhere)
            num_targets = target_ids.clone()
            num_targets[~numerical_mask] = -100
            # Convert absolute token id → bin index for CE
            # (done in dataset — target_ids for numerical positions already store bin idx)
            reg_loss = self.ce(
                reg_logits.view(-1, reg_logits.size(-1)),
                num_targets.view(-1),
            )
        else:
            reg_loss = torch.tensor(0.0, device=reg_logits.device)

        # ── Classification loss ───────────────────────────────────────────────
        cls_loss = torch.tensor(0.0, device=reg_logits.device)
        n_cls_terms = 0
        for feat, logits in cls_logits.items():
            if feat not in categorical_masks:
                continue
            cat_mask = categorical_masks[feat]
            if not cat_mask.any():
                continue
            cat_targets = target_ids.clone()
            cat_targets[~cat_mask] = -100
            cls_loss = cls_loss + self.ce(
                logits.view(-1, logits.size(-1)),
                cat_targets.view(-1),
            )
            n_cls_terms += 1

        if n_cls_terms > 0:
            cls_loss = cls_loss / n_cls_terms

        # ── Planet type loss (sentence-level CE on [CLS]) ────────────────────
        pt_loss = torch.tensor(0.0, device=reg_logits.device)
        if planet_type_labels is not None:
            pt_logits = outputs["planet_type_logits"]  # (B, n_planet_types)
            # -100 means label unknown for this sample — CE ignores it
            pt_loss = self.ce(pt_logits, planet_type_labels)

        total = reg_loss + self.lambda_cls * cls_loss + self.lambda_cls * pt_loss

        return {
            "loss": total,
            "regression_loss": reg_loss,
            "classification_loss": cls_loss,
            "planet_type_loss": pt_loss,
        }


# ── Factory ───────────────────────────────────────────────────────────────────

def build_model(tokenizer) -> tuple[ExoplanetBERT, BertConfig]:
    """Construct model and config from a fitted ExoplanetTokenizer."""
    from tokenizer import FEATURE_ORDER, CATEGORICAL_FEATURES, N_BINS

    cat_vocab_sizes = {
        feat: len(tokenizer.cat_vocabs.get(feat, []))
        for feat in CATEGORICAL_FEATURES
        if feat in tokenizer.cat_vocabs
    }

    cfg = BertConfig(
        vocab_size=tokenizer.vocab_size,
        seq_len=2 + 2 * len(FEATURE_ORDER),
        d_model=128,
        n_heads=4,
        n_layers=4,
        d_ff=512,
        dropout=0.1,
        n_numerical=len(FEATURE_ORDER) - len(CATEGORICAL_FEATURES),
        n_bins=N_BINS,
        cat_vocab_sizes=cat_vocab_sizes,
    )

    model = ExoplanetBERT(cfg)
    return model, cfg


# ── Smoke test ────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    import sys, json
    from pathlib import Path

    BASE = Path(__file__).parent.parent
    sys.path.insert(0, str(Path(__file__).parent))
    from tokenizer import ExoplanetTokenizer, FEATURE_ORDER, CATEGORICAL_FEATURES, N_BINS

    tok = ExoplanetTokenizer.load(str(BASE / "tokenizer_config.json"))

    model, cfg = build_model(tok)
    model.eval()

    print(f"Model config:")
    print(f"  vocab_size  = {cfg.vocab_size}")
    print(f"  seq_len     = {cfg.seq_len}")
    print(f"  d_model     = {cfg.d_model}")
    print(f"  n_layers    = {cfg.n_layers}")
    print(f"  n_heads     = {cfg.n_heads}")
    print(f"  n_bins      = {cfg.n_bins}")
    print(f"  cat_heads   = {list(cfg.cat_vocab_sizes.keys())}")
    print(f"  Parameters  = {model.count_parameters():,}")

    # Fake batch of 4 planets
    B, L = 4, cfg.seq_len
    input_ids    = torch.randint(0, cfg.vocab_size, (B, L))
    attn_mask    = torch.ones(B, L, dtype=torch.long)
    target_ids   = torch.full((B, L), -100, dtype=torch.long)

    with torch.no_grad():
        out = model(input_ids, attn_mask)

    reg = out["regression_logits"]
    print(f"\nForward pass OK:")
    print(f"  Input shape:              {list(input_ids.shape)}")
    print(f"  Regression logits shape:  {list(reg.shape)}   (B, seq_len, n_bins)")
    for feat, logits in out["classification_logits"].items():
        print(f"  Classification '{feat}' shape: {list(logits.shape)}")

    # Check loss runs
    loss_fn = ImputationLoss()
    numerical_mask    = torch.zeros(B, L, dtype=torch.bool)
    categorical_masks = {feat: torch.zeros(B, L, dtype=torch.bool)
                         for feat in CATEGORICAL_FEATURES}

    losses = loss_fn(out, target_ids, numerical_mask, numerical_mask, categorical_masks)
    print(f"\nLoss (all-zero masks → 0 terms):")
    print(f"  total={losses['loss'].item():.4f}  "
          f"reg={losses['regression_loss'].item():.4f}  "
          f"cls={losses['classification_loss'].item():.4f}")

    print("\nPhase 3 smoke test passed ✓")
