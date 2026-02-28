"""
BART-style parameter generator for structured exoplanet parameter predictions.

Uses a sequence-to-sequence architecture to generate structured JSON parameter
predictions from known planet data. BART is used for text-to-text generation:
given known parameters as input text, generate JSON with predicted missing values.

This addresses Pitfall 6: BART targets structured text generation, not direct
numerical regression.
"""

import json
import re
import time
from typing import Any, Dict, List, Optional, Tuple

import numpy as np
import pandas as pd
import torch
import torch.nn as nn
from torch.utils.data import DataLoader, Dataset


# Fields that BART can generate predictions for
GENERATABLE_FIELDS = [
    "surface_pressure_atm",
    "albedo",
    "ocean_coverage_fraction",
    "cloud_coverage_fraction",
    "ice_coverage_fraction",
]

# Input features used to condition generation
CONDITIONING_FEATURES = [
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


def _record_to_input_text(row: pd.Series) -> str:
    """Convert a record's known features into a conditioning text prompt."""
    parts = []
    for feat in CONDITIONING_FEATURES:
        val = row.get(feat, np.nan)
        if pd.notna(val):
            parts.append(f"{feat}={val:.4g}")
    return "predict parameters: " + ", ".join(parts) if parts else "predict parameters: unknown"


def _record_to_target_text(row: pd.Series, target_fields: List[str]) -> str:
    """Convert target fields to a JSON string for training."""
    target = {}
    for field in target_fields:
        val = row.get(field, np.nan)
        if pd.notna(val):
            target[field] = round(float(val), 4)
    return json.dumps(target)


def _parse_generated_text(text: str) -> Dict[str, float]:
    """Parse generated text back into a dictionary of field values.

    Handles both clean JSON output and noisy model output by attempting
    JSON parse first, then falling back to regex extraction.
    """
    # Try direct JSON parse
    try:
        result = json.loads(text)
        if isinstance(result, dict):
            return {k: float(v) for k, v in result.items() if isinstance(v, (int, float))}
    except (json.JSONDecodeError, ValueError):
        pass

    # Fallback: extract key=value or key: value patterns
    result = {}
    for field in GENERATABLE_FIELDS:
        pattern = rf'{field}["\s]*[:=]\s*([0-9.eE+\-]+)'
        match = re.search(pattern, text)
        if match:
            try:
                result[field] = float(match.group(1))
            except ValueError:
                pass

    return result


class _SimpleVocab:
    """Character-level vocabulary for the seq2seq model."""

    def __init__(self):
        self.char_to_idx: Dict[str, int] = {"<pad>": 0, "<sos>": 1, "<eos>": 2, "<unk>": 3}
        self._next_id = 4

    def build(self, texts: List[str]) -> None:
        """Build vocabulary from texts."""
        for text in texts:
            for ch in text:
                if ch not in self.char_to_idx:
                    self.char_to_idx[ch] = self._next_id
                    self._next_id += 1

    def encode(self, text: str, max_len: int = 256) -> List[int]:
        """Encode text to character IDs."""
        ids = [self.char_to_idx.get("<sos>", 1)]
        for ch in text[:max_len - 2]:
            ids.append(self.char_to_idx.get(ch, self.char_to_idx["<unk>"]))
        ids.append(self.char_to_idx.get("<eos>", 2))
        while len(ids) < max_len:
            ids.append(0)
        return ids[:max_len]

    def decode(self, ids: List[int]) -> str:
        """Decode token IDs back to text."""
        idx_to_char = {v: k for k, v in self.char_to_idx.items()}
        chars = []
        for idx in ids:
            ch = idx_to_char.get(idx, "")
            if ch in ("<eos>", "<pad>"):
                break
            if ch not in ("<sos>", "<unk>"):
                chars.append(ch)
        return "".join(chars)

    @property
    def size(self) -> int:
        return self._next_id


class _Seq2SeqDataset(Dataset):
    """Dataset for seq2seq training."""

    def __init__(
        self,
        src_texts: List[str],
        tgt_texts: List[str],
        vocab: _SimpleVocab,
        max_src_len: int = 256,
        max_tgt_len: int = 128,
    ):
        self.src_ids = [vocab.encode(t, max_src_len) for t in src_texts]
        self.tgt_ids = [vocab.encode(t, max_tgt_len) for t in tgt_texts]

    def __len__(self) -> int:
        return len(self.src_ids)

    def __getitem__(self, idx: int) -> Tuple[torch.Tensor, torch.Tensor]:
        return (
            torch.tensor(self.src_ids[idx], dtype=torch.long),
            torch.tensor(self.tgt_ids[idx], dtype=torch.long),
        )


class _Seq2SeqModel(nn.Module):
    """Simplified encoder-decoder transformer following BART architecture.

    Uses a transformer encoder-decoder with character-level tokenization.
    This is a lightweight stand-in for the full BART model. For production
    benchmarking, replace with BartForConditionalGeneration from HuggingFace.
    """

    def __init__(
        self,
        vocab_size: int,
        embed_dim: int = 128,
        num_heads: int = 4,
        num_encoder_layers: int = 2,
        num_decoder_layers: int = 2,
        max_len: int = 256,
        dropout: float = 0.1,
    ):
        super().__init__()
        self.embed_dim = embed_dim
        self.embedding = nn.Embedding(vocab_size, embed_dim, padding_idx=0)
        self.pos_embedding = nn.Embedding(max_len, embed_dim)

        self.transformer = nn.Transformer(
            d_model=embed_dim,
            nhead=num_heads,
            num_encoder_layers=num_encoder_layers,
            num_decoder_layers=num_decoder_layers,
            dim_feedforward=embed_dim * 4,
            dropout=dropout,
            batch_first=True,
        )

        self.output_proj = nn.Linear(embed_dim, vocab_size)

    def forward(
        self, src: torch.Tensor, tgt: torch.Tensor
    ) -> torch.Tensor:
        src_len = src.size(1)
        tgt_len = tgt.size(1)

        src_pos = torch.arange(src_len, device=src.device).unsqueeze(0)
        tgt_pos = torch.arange(tgt_len, device=tgt.device).unsqueeze(0)

        src_emb = self.embedding(src) + self.pos_embedding(src_pos)
        tgt_emb = self.embedding(tgt) + self.pos_embedding(tgt_pos)

        # Causal mask for decoder
        tgt_mask = nn.Transformer.generate_square_subsequent_mask(
            tgt_len, device=src.device
        )

        src_padding_mask = src == 0
        tgt_padding_mask = tgt == 0

        output = self.transformer(
            src_emb,
            tgt_emb,
            tgt_mask=tgt_mask,
            src_key_padding_mask=src_padding_mask,
            tgt_key_padding_mask=tgt_padding_mask,
        )

        return self.output_proj(output)

    def generate(
        self, src: torch.Tensor, vocab: _SimpleVocab, max_len: int = 128
    ) -> List[int]:
        """Auto-regressive generation from encoded source."""
        self.eval()
        with torch.no_grad():
            src_len = src.size(1)
            src_pos = torch.arange(src_len, device=src.device).unsqueeze(0)
            src_emb = self.embedding(src) + self.pos_embedding(src_pos)

            # Encode source
            memory = self.transformer.encoder(src_emb)

            # Start with <sos>
            generated = [vocab.char_to_idx.get("<sos>", 1)]

            for _ in range(max_len):
                tgt = torch.tensor([generated], dtype=torch.long, device=src.device)
                tgt_len = tgt.size(1)
                tgt_pos = torch.arange(tgt_len, device=src.device).unsqueeze(0)
                tgt_emb = self.embedding(tgt) + self.pos_embedding(tgt_pos)

                tgt_mask = nn.Transformer.generate_square_subsequent_mask(
                    tgt_len, device=src.device
                )

                output = self.transformer.decoder(
                    tgt_emb, memory, tgt_mask=tgt_mask
                )
                logits = self.output_proj(output[:, -1, :])
                next_token = logits.argmax(dim=-1).item()

                if next_token == vocab.char_to_idx.get("<eos>", 2):
                    break
                generated.append(next_token)

            return generated


class BartParameterGenerator:
    """Use a BART-style model to generate structured exoplanet parameter predictions.

    Input: Known planet data as text prompt
    Output: JSON with predicted missing parameters

    BART is used for text-to-text: given known parameters as input text,
    generate JSON with predicted values. Better for structured output than
    direct numerical regression.
    """

    def __init__(self, model_name: str = "facebook/bart-base"):
        self.model_name = model_name
        self.device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
        self.model: Optional[_Seq2SeqModel] = None
        self.vocab = _SimpleVocab()
        self._inference_times: List[float] = []
        self._target_fields: List[str] = GENERATABLE_FIELDS

    def train(
        self,
        df: pd.DataFrame,
        target_fields: Optional[List[str]] = None,
        epochs: int = 30,
        batch_size: int = 16,
        lr: float = 1e-3,
    ) -> Dict[str, float]:
        """Train the generator on a DataFrame.

        Args:
            df: Training DataFrame with feature and target columns.
            target_fields: Fields to generate. Defaults to GENERATABLE_FIELDS.
            epochs: Number of training epochs.
            batch_size: Training batch size.
            lr: Learning rate.

        Returns:
            Dictionary with training metrics.
        """
        if target_fields is not None:
            self._target_fields = target_fields

        # Prepare text pairs
        src_texts = [_record_to_input_text(row) for _, row in df.iterrows()]
        tgt_texts = [
            _record_to_target_text(row, self._target_fields) for _, row in df.iterrows()
        ]

        # Build vocabulary
        self.vocab.build(src_texts + tgt_texts)

        # Create dataset
        dataset = _Seq2SeqDataset(src_texts, tgt_texts, self.vocab)
        dataloader = DataLoader(
            dataset, batch_size=batch_size, shuffle=True, drop_last=False
        )

        # Initialize model
        self.model = _Seq2SeqModel(
            vocab_size=self.vocab.size + 1,
        ).to(self.device)

        optimizer = torch.optim.AdamW(self.model.parameters(), lr=lr)
        criterion = nn.CrossEntropyLoss(ignore_index=0)

        self.model.train()
        total_loss = 0.0

        for epoch in range(epochs):
            epoch_loss = 0.0
            n_batches = 0

            for src, tgt in dataloader:
                src = src.to(self.device)
                tgt = tgt.to(self.device)

                # Teacher forcing: input is tgt[:-1], target is tgt[1:]
                tgt_input = tgt[:, :-1]
                tgt_output = tgt[:, 1:]

                optimizer.zero_grad()
                logits = self.model(src, tgt_input)
                loss = criterion(
                    logits.reshape(-1, logits.size(-1)),
                    tgt_output.reshape(-1),
                )
                loss.backward()
                optimizer.step()

                epoch_loss += loss.item()
                n_batches += 1

            total_loss = epoch_loss / max(n_batches, 1)

        return {"loss": total_loss}

    def predict(self, df_or_row) -> Dict[str, float]:
        """Generate parameter predictions for input data.

        Args:
            df_or_row: DataFrame or Series with conditioning features.

        Returns:
            Dictionary mapping field names to predicted float values.
        """
        if self.model is None:
            raise RuntimeError("Model not trained. Call train() first.")

        if isinstance(df_or_row, pd.Series):
            text = _record_to_input_text(df_or_row)
        elif isinstance(df_or_row, pd.DataFrame) and len(df_or_row) == 1:
            text = _record_to_input_text(df_or_row.iloc[0])
        else:
            text = _record_to_input_text(df_or_row)

        src_ids = self.vocab.encode(text)
        src_tensor = torch.tensor([src_ids], dtype=torch.long).to(self.device)

        t0 = time.perf_counter()
        generated_ids = self.model.generate(src_tensor, self.vocab)
        elapsed_ms = (time.perf_counter() - t0) * 1000
        self._inference_times.append(elapsed_ms)

        generated_text = self.vocab.decode(generated_ids)
        return _parse_generated_text(generated_text)

    def predict_batch(self, df: pd.DataFrame) -> List[Dict[str, float]]:
        """Generate predictions for a batch of records.

        Args:
            df: DataFrame with conditioning features.

        Returns:
            List of prediction dictionaries, one per row.
        """
        results = []
        for _, row in df.iterrows():
            results.append(self.predict(row))
        return results

    def get_latency_ms(self) -> float:
        """Return average inference latency in milliseconds."""
        if not self._inference_times:
            return 0.0
        return np.mean(self._inference_times)
