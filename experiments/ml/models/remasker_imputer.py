"""
Simplified masked autoencoder imputer for tabular data.

Inspired by ReMasker (Du et al., 2023): masks random features, trains an
autoencoder to reconstruct them, then uses the trained model for imputation.

This is a simplified implementation using PyTorch directly since the full
ReMasker package may have dependency issues (timm, hyperimpute). The core
idea is preserved: randomly mask subsets of features during training, teach
the model to reconstruct them, and at inference time use the trained
reconstruction ability to fill in genuinely missing values.

NOTE: This is a simplified version. For production use, consider the full
ReMasker package (github.com/tydusky/remasker) if dependencies install
cleanly.
"""

import time
from typing import Dict, List, Optional, Tuple

import numpy as np
import pandas as pd
import torch
import torch.nn as nn
from torch.utils.data import DataLoader, TensorDataset


# Default features for imputation
DEFAULT_FEATURES = [
    "mass_earth",
    "radius_earth",
    "density_gcc",
    "equilibrium_temp_k",
    "surface_gravity_g",
    "orbital_period_days",
    "semi_major_axis_au",
    "eccentricity",
    "star_temp_k",
    "star_radius_solar",
    "star_mass_solar",
    "star_metallicity",
]


class _MaskedAutoencoder(nn.Module):
    """Masked autoencoder for tabular data.

    Architecture:
    1. Input features (with some masked/zeroed)
    2. Encoder: maps input -> latent representation
    3. Decoder: maps latent -> reconstructed full feature vector
    4. Loss computed ONLY on masked positions (reconstruction objective)

    The mask indicator is concatenated with the input features so the model
    knows which features are observed vs. masked.
    """

    def __init__(
        self,
        input_dim: int,
        hidden_dim: int = 128,
        latent_dim: int = 64,
        num_layers: int = 3,
        dropout: float = 0.1,
    ):
        super().__init__()
        self.input_dim = input_dim

        # Encoder: input features + mask indicator -> latent
        encoder_layers = []
        prev_dim = input_dim * 2  # features + mask indicator
        for i in range(num_layers):
            out_dim = hidden_dim if i < num_layers - 1 else latent_dim
            encoder_layers.extend([
                nn.Linear(prev_dim, out_dim),
                nn.ReLU(),
                nn.Dropout(dropout),
            ])
            prev_dim = out_dim
        self.encoder = nn.Sequential(*encoder_layers)

        # Decoder: latent -> reconstructed features
        decoder_layers = []
        prev_dim = latent_dim
        for i in range(num_layers):
            out_dim = hidden_dim if i < num_layers - 1 else input_dim
            decoder_layers.append(nn.Linear(prev_dim, out_dim))
            if i < num_layers - 1:
                decoder_layers.extend([nn.ReLU(), nn.Dropout(dropout)])
            prev_dim = out_dim
        self.decoder = nn.Sequential(*decoder_layers)

    def forward(
        self, x: torch.Tensor, mask: torch.Tensor
    ) -> torch.Tensor:
        """Forward pass.

        Args:
            x: Input features (batch, input_dim) with masked values set to 0.
            mask: Binary mask (batch, input_dim), 1 = observed, 0 = masked.

        Returns:
            Reconstructed features (batch, input_dim).
        """
        # Concatenate features with mask indicator
        x_masked = x * mask  # Zero out masked features
        encoder_input = torch.cat([x_masked, mask], dim=1)

        latent = self.encoder(encoder_input)
        reconstructed = self.decoder(latent)
        return reconstructed


class MaskedAutoencoderImputer:
    """Simplified masked autoencoder for tabular imputation.

    Inspired by ReMasker (Du et al., 2023).
    Masks random features during training, trains autoencoder to reconstruct
    them, then uses the trained model to impute genuinely missing values.

    The key insight from ReMasker: training with random masking teaches the
    model robust feature relationships that generalize to imputing any
    subset of missing features at inference time.
    """

    def __init__(
        self,
        input_dim: Optional[int] = None,
        hidden_dim: int = 128,
        mask_ratio: float = 0.3,
        features: Optional[List[str]] = None,
    ):
        """Initialize the imputer.

        Args:
            input_dim: Number of input features. Auto-detected if None.
            hidden_dim: Hidden dimension size.
            mask_ratio: Fraction of features to mask during training.
            features: List of feature column names to use.
        """
        self.input_dim = input_dim
        self.hidden_dim = hidden_dim
        self.mask_ratio = mask_ratio
        self.features = features or DEFAULT_FEATURES
        self.device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
        self.model: Optional[_MaskedAutoencoder] = None
        self._means: Optional[np.ndarray] = None
        self._stds: Optional[np.ndarray] = None
        self._inference_times: List[float] = []

    def _normalize(
        self, data: np.ndarray, fit: bool = False
    ) -> np.ndarray:
        """Normalize features to zero mean, unit variance."""
        if fit:
            self._means = np.nanmean(data, axis=0)
            self._stds = np.nanstd(data, axis=0)
            self._stds[self._stds == 0] = 1.0

        return (data - self._means) / self._stds

    def _denormalize(self, data: np.ndarray) -> np.ndarray:
        """Reverse normalization."""
        return data * self._stds + self._means

    def train(
        self,
        df: pd.DataFrame,
        target_fields: Optional[List[str]] = None,
        epochs: int = 100,
        lr: float = 1e-3,
        batch_size: int = 32,
    ) -> Dict[str, float]:
        """Train the masked autoencoder on complete records.

        Args:
            df: Training DataFrame.
            target_fields: Not used directly (model learns all features).
                Kept for interface consistency with TabTransformerImputer.
            epochs: Number of training epochs.
            lr: Learning rate.
            batch_size: Batch size.

        Returns:
            Dictionary with training loss.
        """
        # Extract feature matrix
        available_features = [f for f in self.features if f in df.columns]
        self.features = available_features
        self.input_dim = len(available_features)

        data = df[available_features].values.astype(np.float64)

        # Use rows that have at least some data
        valid_mask = ~np.all(np.isnan(data), axis=1)
        data = data[valid_mask]

        if len(data) < 5:
            print("Warning: Too few valid records for training")
            return {"loss": float("nan")}

        # Fill NaN with column means for training data preparation
        col_means = np.nanmean(data, axis=0)
        for j in range(data.shape[1]):
            nan_mask = np.isnan(data[:, j])
            data[nan_mask, j] = col_means[j]

        # Normalize
        data_norm = self._normalize(data, fit=True)

        # Initialize model
        self.model = _MaskedAutoencoder(
            input_dim=self.input_dim,
            hidden_dim=self.hidden_dim,
        ).to(self.device)

        optimizer = torch.optim.AdamW(self.model.parameters(), lr=lr)

        # Training loop with random masking
        data_tensor = torch.tensor(data_norm, dtype=torch.float32)
        dataset = TensorDataset(data_tensor)
        dataloader = DataLoader(
            dataset, batch_size=batch_size, shuffle=True, drop_last=False
        )

        self.model.train()
        final_loss = 0.0

        for epoch in range(epochs):
            epoch_loss = 0.0
            n_batches = 0

            for (batch_x,) in dataloader:
                batch_x = batch_x.to(self.device)
                batch_size_actual = batch_x.size(0)

                # Random masking: for each sample, mask mask_ratio of features
                mask = torch.ones_like(batch_x)
                n_mask = max(1, int(self.input_dim * self.mask_ratio))
                for i in range(batch_size_actual):
                    mask_indices = torch.randperm(self.input_dim)[:n_mask]
                    mask[i, mask_indices] = 0.0

                optimizer.zero_grad()
                reconstructed = self.model(batch_x, mask)

                # Loss only on masked positions
                masked_positions = (mask == 0).float()
                if masked_positions.sum() > 0:
                    loss = (
                        ((reconstructed - batch_x) ** 2 * masked_positions).sum()
                        / masked_positions.sum()
                    )
                else:
                    loss = ((reconstructed - batch_x) ** 2).mean()

                loss.backward()
                optimizer.step()

                epoch_loss += loss.item()
                n_batches += 1

            final_loss = epoch_loss / max(n_batches, 1)

        return {"loss": final_loss}

    def predict(
        self,
        df_or_row,
        target_fields: Optional[List[str]] = None,
    ) -> Dict[str, float]:
        """Impute missing values using the trained autoencoder.

        Args:
            df_or_row: DataFrame or Series with input features (may have NaN).
            target_fields: Specific fields to return predictions for.
                If None, returns predictions for all features.

        Returns:
            Dictionary mapping field names to imputed values.
        """
        if self.model is None:
            raise RuntimeError("Model not trained. Call train() first.")

        if isinstance(df_or_row, pd.Series):
            df_or_row = pd.DataFrame([df_or_row])

        if target_fields is None:
            target_fields = self.features

        # Extract features
        data = np.full((len(df_or_row), len(self.features)), np.nan)
        for j, feat in enumerate(self.features):
            if feat in df_or_row.columns:
                data[:, j] = df_or_row[feat].values

        # Build mask (1 = observed, 0 = missing)
        mask_np = (~np.isnan(data)).astype(np.float32)

        # Fill NaN with means for model input
        data_filled = data.copy()
        for j in range(data.shape[1]):
            nan_positions = np.isnan(data_filled[:, j])
            data_filled[nan_positions, j] = self._means[j] if self._means is not None else 0.0

        # Normalize
        data_norm = self._normalize(data_filled)

        x = torch.tensor(data_norm, dtype=torch.float32).to(self.device)
        mask_tensor = torch.tensor(mask_np, dtype=torch.float32).to(self.device)

        self.model.eval()
        with torch.no_grad():
            t0 = time.perf_counter()
            reconstructed = self.model(x, mask_tensor)
            elapsed_ms = (time.perf_counter() - t0) * 1000
            self._inference_times.append(elapsed_ms)

        # Denormalize
        result_norm = reconstructed.cpu().numpy()
        result = self._denormalize(result_norm)

        # Return predictions for target fields
        predictions = {}
        for field in target_fields:
            if field in self.features:
                idx = self.features.index(field)
                predictions[field] = float(result[0, idx])

        return predictions

    def get_latency_ms(self) -> float:
        """Return average inference latency in milliseconds."""
        if not self._inference_times:
            return 0.0
        return np.mean(self._inference_times)
