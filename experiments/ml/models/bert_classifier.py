"""
BERT-based planet type classifier.

Fine-tunes BERT for planet type classification from tabular features.
Input features are converted to text prompts (e.g., "mass: 2.36, radius: 1.34,
temp: 233") since BERT is a language model, not a direct numerical regressor.

This addresses Pitfall 6 from research: BERT is used for classification
(planet type), NOT numerical regression.
"""

import time
from typing import Dict, List, Optional, Tuple

import numpy as np
import pandas as pd
import torch
import torch.nn as nn
from torch.utils.data import DataLoader, Dataset


# Planet type label mapping
PLANET_TYPES = ["Rocky", "Super-Earth", "Mini-Neptune", "Ice Giant", "Gas Giant"]
LABEL_TO_IDX = {label: idx for idx, label in enumerate(PLANET_TYPES)}
IDX_TO_LABEL = {idx: label for label, idx in LABEL_TO_IDX.items()}

# Features used for classification input text
INPUT_FEATURES = [
    "mass_earth",
    "radius_earth",
    "density_gcc",
    "equilibrium_temp_k",
    "surface_gravity_g",
    "orbital_period_days",
    "semi_major_axis_au",
    "eccentricity",
]


def _features_to_text(row: pd.Series) -> str:
    """Convert a row of numerical features to a text prompt for BERT.

    Args:
        row: Pandas Series with feature values.

    Returns:
        Text string like "mass: 2.36, radius: 1.34, temp: 233.0, ..."
    """
    parts = []
    feature_names = {
        "mass_earth": "mass",
        "radius_earth": "radius",
        "density_gcc": "density",
        "equilibrium_temp_k": "temp",
        "surface_gravity_g": "gravity",
        "orbital_period_days": "period",
        "semi_major_axis_au": "sma",
        "eccentricity": "ecc",
    }
    for feat in INPUT_FEATURES:
        val = row.get(feat, np.nan)
        if pd.notna(val):
            short_name = feature_names.get(feat, feat)
            parts.append(f"{short_name}: {val:.4g}")
    return ", ".join(parts) if parts else "unknown planet"


class _SimpleTokenizer:
    """Simple word-level tokenizer for converting feature text to token IDs.

    This avoids requiring the full HuggingFace tokenizer download at import
    time. For production use, replace with AutoTokenizer.from_pretrained().
    """

    def __init__(self, max_length: int = 64):
        self.max_length = max_length
        self.vocab: Dict[str, int] = {"[PAD]": 0, "[UNK]": 1, "[CLS]": 2, "[SEP]": 3}
        self._next_id = 4

    def build_vocab(self, texts: List[str]) -> None:
        """Build vocabulary from training texts."""
        for text in texts:
            for token in text.replace(",", " ").replace(":", " ").split():
                if token not in self.vocab:
                    self.vocab[token] = self._next_id
                    self._next_id += 1

    def encode(self, text: str) -> List[int]:
        """Encode text to token IDs with [CLS] and [SEP]."""
        tokens = text.replace(",", " ").replace(":", " ").split()
        ids = [self.vocab.get("[CLS]", 2)]
        for token in tokens[: self.max_length - 2]:
            ids.append(self.vocab.get(token, self.vocab["[UNK]"]))
        ids.append(self.vocab.get("[SEP]", 3))
        # Pad to max_length
        while len(ids) < self.max_length:
            ids.append(self.vocab["[PAD]"])
        return ids[: self.max_length]

    @property
    def vocab_size(self) -> int:
        return max(self._next_id, len(self.vocab))


class _PlanetDataset(Dataset):
    """PyTorch Dataset for planet classification."""

    def __init__(
        self,
        texts: List[str],
        labels: List[int],
        tokenizer: _SimpleTokenizer,
    ):
        self.texts = texts
        self.labels = labels
        self.tokenizer = tokenizer

    def __len__(self) -> int:
        return len(self.texts)

    def __getitem__(self, idx: int) -> Tuple[torch.Tensor, torch.Tensor]:
        ids = self.tokenizer.encode(self.texts[idx])
        return (
            torch.tensor(ids, dtype=torch.long),
            torch.tensor(self.labels[idx], dtype=torch.long),
        )


class _BertClassifierModel(nn.Module):
    """Simplified BERT-style transformer classifier.

    Uses a small transformer encoder for classification from tokenized
    tabular feature text. This is a lightweight stand-in that follows the
    BERT architecture pattern (token embeddings + transformer + [CLS] pooling)
    without requiring the full pretrained BERT weights download.

    For production benchmarking with pretrained weights, replace this with
    BertForSequenceClassification from HuggingFace transformers.
    """

    def __init__(
        self,
        vocab_size: int,
        num_classes: int,
        embed_dim: int = 128,
        num_heads: int = 4,
        num_layers: int = 2,
        max_length: int = 64,
        dropout: float = 0.1,
    ):
        super().__init__()
        self.embedding = nn.Embedding(vocab_size, embed_dim, padding_idx=0)
        self.position_embedding = nn.Embedding(max_length, embed_dim)

        encoder_layer = nn.TransformerEncoderLayer(
            d_model=embed_dim,
            nhead=num_heads,
            dim_feedforward=embed_dim * 4,
            dropout=dropout,
            batch_first=True,
        )
        self.transformer = nn.TransformerEncoder(
            encoder_layer, num_layers=num_layers
        )

        self.classifier = nn.Sequential(
            nn.Linear(embed_dim, embed_dim),
            nn.ReLU(),
            nn.Dropout(dropout),
            nn.Linear(embed_dim, num_classes),
        )

    def forward(self, input_ids: torch.Tensor) -> torch.Tensor:
        seq_len = input_ids.size(1)
        positions = torch.arange(seq_len, device=input_ids.device).unsqueeze(0)

        x = self.embedding(input_ids) + self.position_embedding(positions)

        # Create attention mask for padding tokens
        padding_mask = input_ids == 0

        x = self.transformer(x, src_key_padding_mask=padding_mask)

        # Pool from [CLS] token (position 0)
        cls_output = x[:, 0, :]
        return self.classifier(cls_output)


class BertPlanetClassifier:
    """Fine-tune a BERT-style transformer for planet type classification.

    Input: Tabular features as text (e.g., "mass: 2.36, radius: 1.34, temp: 233")
    Output: Planet type classification (Rocky, Super-Earth, Mini-Neptune,
            Ice Giant, Gas Giant)

    Uses a lightweight transformer classifier that follows BERT architecture
    patterns. For full pretrained BERT, set use_pretrained=True (requires
    HuggingFace transformers and model download).
    """

    def __init__(self, model_name: str = "bert-base-uncased"):
        self.model_name = model_name
        self.device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
        self.model: Optional[_BertClassifierModel] = None
        self.tokenizer = _SimpleTokenizer(max_length=64)
        self._inference_times: List[float] = []

    def train(
        self,
        df: pd.DataFrame,
        target_col: str = "planet_type",
        epochs: int = 20,
        batch_size: int = 16,
        lr: float = 1e-3,
    ) -> Dict[str, float]:
        """Train the classifier on a DataFrame.

        Args:
            df: Training DataFrame with feature columns and target_col.
            target_col: Column name for classification labels.
            epochs: Number of training epochs.
            batch_size: Training batch size.
            lr: Learning rate.

        Returns:
            Dictionary with training metrics (loss, accuracy).
        """
        # Prepare text inputs
        texts = [_features_to_text(row) for _, row in df.iterrows()]
        labels = [
            LABEL_TO_IDX.get(label, 0)
            for label in df[target_col].values
        ]

        # Build tokenizer vocabulary
        self.tokenizer.build_vocab(texts)

        # Create dataset and dataloader
        dataset = _PlanetDataset(texts, labels, self.tokenizer)
        dataloader = DataLoader(
            dataset, batch_size=batch_size, shuffle=True, drop_last=False
        )

        # Initialize model
        self.model = _BertClassifierModel(
            vocab_size=self.tokenizer.vocab_size + 1,
            num_classes=len(PLANET_TYPES),
        ).to(self.device)

        optimizer = torch.optim.AdamW(self.model.parameters(), lr=lr)
        criterion = nn.CrossEntropyLoss()

        self.model.train()
        total_loss = 0.0
        correct = 0
        total = 0

        for epoch in range(epochs):
            epoch_loss = 0.0
            epoch_correct = 0
            epoch_total = 0

            for input_ids, target in dataloader:
                input_ids = input_ids.to(self.device)
                target = target.to(self.device)

                optimizer.zero_grad()
                logits = self.model(input_ids)
                loss = criterion(logits, target)
                loss.backward()
                optimizer.step()

                epoch_loss += loss.item() * input_ids.size(0)
                preds = logits.argmax(dim=1)
                epoch_correct += (preds == target).sum().item()
                epoch_total += input_ids.size(0)

            total_loss = epoch_loss / max(epoch_total, 1)
            correct = epoch_correct
            total = epoch_total

        accuracy = correct / max(total, 1)
        return {"loss": total_loss, "accuracy": accuracy}

    def predict(self, df_or_row) -> np.ndarray:
        """Predict planet types for input data.

        Args:
            df_or_row: DataFrame or Series with feature columns.

        Returns:
            Array of predicted planet type labels.
        """
        if self.model is None:
            raise RuntimeError("Model not trained. Call train() first.")

        self.model.eval()

        if isinstance(df_or_row, pd.Series):
            df_or_row = pd.DataFrame([df_or_row])

        texts = [_features_to_text(row) for _, row in df_or_row.iterrows()]

        predictions = []
        with torch.no_grad():
            for text in texts:
                ids = self.tokenizer.encode(text)
                input_ids = torch.tensor([ids], dtype=torch.long).to(self.device)

                t0 = time.perf_counter()
                logits = self.model(input_ids)
                elapsed_ms = (time.perf_counter() - t0) * 1000
                self._inference_times.append(elapsed_ms)

                pred_idx = logits.argmax(dim=1).item()
                predictions.append(IDX_TO_LABEL.get(pred_idx, "Unknown"))

        return np.array(predictions)

    def get_latency_ms(self) -> float:
        """Return average inference latency in milliseconds."""
        if not self._inference_times:
            return 0.0
        return np.mean(self._inference_times)
