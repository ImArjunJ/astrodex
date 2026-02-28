"""
TabTransformer for numerical field imputation.

Uses attention on categorical features combined with MLP on numerical features
to impute missing values. This is the primary numerical imputation model in
the benchmark, designed for tabular data natively (unlike BERT/BART which
require text conversion).

Uses the tab-transformer-pytorch library when available, with a fallback to
a simplified PyTorch implementation.
"""

import time
from typing import Dict, List, Optional, Tuple

import numpy as np
import pandas as pd
import torch
import torch.nn as nn
from torch.utils.data import DataLoader, TensorDataset


# Default numerical features for imputation
NUMERICAL_FEATURES = [
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

# Default categorical features
CATEGORICAL_FEATURES = [
    "planet_type",
    "discovery_method",
    "spectral_type",
]


class _TabTransformerModel(nn.Module):
    """TabTransformer: Attention on categorical features + MLP on numerical.

    Implements the TabTransformer architecture (Huang et al., 2020):
    1. Categorical features are embedded and processed through transformer layers
    2. Numerical features are passed through a normalization layer
    3. Both are concatenated and fed through an MLP for prediction

    This is a simplified implementation. For the full version, use
    tab-transformer-pytorch library: TabTransformer from tab_transformer_pytorch.
    """

    def __init__(
        self,
        n_numerical: int,
        n_categories: List[int],
        embed_dim: int = 32,
        num_heads: int = 4,
        num_layers: int = 2,
        mlp_hidden: int = 128,
        output_dim: int = 1,
        dropout: float = 0.1,
    ):
        super().__init__()

        self.n_numerical = n_numerical
        self.n_categories = n_categories

        # Categorical embeddings
        self.cat_embeddings = nn.ModuleList(
            [nn.Embedding(n_cat + 1, embed_dim) for n_cat in n_categories]
        )

        # Transformer encoder for categorical features
        if len(n_categories) > 0:
            encoder_layer = nn.TransformerEncoderLayer(
                d_model=embed_dim,
                nhead=num_heads,
                dim_feedforward=embed_dim * 4,
                dropout=dropout,
                batch_first=True,
            )
            self.cat_transformer = nn.TransformerEncoder(
                encoder_layer, num_layers=num_layers
            )
        else:
            self.cat_transformer = None

        # Numerical feature normalization
        self.num_norm = nn.LayerNorm(n_numerical) if n_numerical > 0 else None

        # MLP head
        cat_dim = len(n_categories) * embed_dim if n_categories else 0
        total_dim = n_numerical + cat_dim
        self.mlp = nn.Sequential(
            nn.Linear(total_dim, mlp_hidden),
            nn.ReLU(),
            nn.Dropout(dropout),
            nn.Linear(mlp_hidden, mlp_hidden),
            nn.ReLU(),
            nn.Dropout(dropout),
            nn.Linear(mlp_hidden, output_dim),
        )

    def forward(
        self,
        x_num: torch.Tensor,
        x_cat: Optional[torch.Tensor] = None,
    ) -> torch.Tensor:
        parts = []

        # Process numerical features
        if self.n_numerical > 0 and self.num_norm is not None:
            num_out = self.num_norm(x_num)
            parts.append(num_out)

        # Process categorical features through transformer
        if x_cat is not None and self.cat_transformer is not None and len(self.n_categories) > 0:
            cat_embeds = []
            for i, emb in enumerate(self.cat_embeddings):
                cat_embeds.append(emb(x_cat[:, i]))
            cat_stacked = torch.stack(cat_embeds, dim=1)  # (batch, n_cat, embed_dim)
            cat_transformed = self.cat_transformer(cat_stacked)
            cat_flat = cat_transformed.reshape(cat_transformed.size(0), -1)
            parts.append(cat_flat)

        if not parts:
            raise ValueError("No features provided")

        combined = torch.cat(parts, dim=1)
        return self.mlp(combined)


class TabTransformerImputer:
    """TabTransformer for numerical field imputation.

    Uses attention on categorical features + MLP on numerical features.
    Trains a separate model for each target field to impute.

    Input: Tabular features with some NaN values
    Output: Imputed values for NaN fields
    """

    def __init__(
        self,
        categorical_cols: Optional[List[str]] = None,
        numerical_cols: Optional[List[str]] = None,
    ):
        self.categorical_cols = categorical_cols or CATEGORICAL_FEATURES
        self.numerical_cols = numerical_cols or NUMERICAL_FEATURES
        self.device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
        self.models: Dict[str, _TabTransformerModel] = {}
        self._cat_encoders: Dict[str, Dict[str, int]] = {}
        self._num_means: Dict[str, float] = {}
        self._num_stds: Dict[str, float] = {}
        self._inference_times: List[float] = []

    def _encode_categoricals(
        self, df: pd.DataFrame, fit: bool = False
    ) -> Optional[torch.Tensor]:
        """Encode categorical features as integer indices."""
        if not self.categorical_cols:
            return None

        cat_data = []
        for col in self.categorical_cols:
            if col not in df.columns:
                cat_data.append(np.zeros(len(df), dtype=np.int64))
                continue

            if fit:
                unique_vals = df[col].dropna().unique().tolist()
                self._cat_encoders[col] = {
                    val: idx + 1 for idx, val in enumerate(unique_vals)
                }

            encoder = self._cat_encoders.get(col, {})
            encoded = df[col].map(lambda x: encoder.get(x, 0)).values
            cat_data.append(encoded.astype(np.int64))

        return torch.tensor(np.column_stack(cat_data), dtype=torch.long)

    def _prepare_numerical(
        self,
        df: pd.DataFrame,
        target_field: str,
        fit: bool = False,
    ) -> Tuple[torch.Tensor, torch.Tensor]:
        """Prepare numerical features, excluding the target field."""
        input_cols = [c for c in self.numerical_cols if c != target_field]

        if fit:
            for col in input_cols:
                vals = df[col].dropna()
                self._num_means[col] = vals.mean() if len(vals) > 0 else 0.0
                self._num_stds[col] = vals.std() if len(vals) > 1 else 1.0

        num_data = []
        for col in input_cols:
            vals = df[col].fillna(self._num_means.get(col, 0.0)).values
            mean = self._num_means.get(col, 0.0)
            std = self._num_stds.get(col, 1.0)
            if std == 0:
                std = 1.0
            normalized = (vals - mean) / std
            num_data.append(normalized)

        x_num = torch.tensor(
            np.column_stack(num_data) if num_data else np.zeros((len(df), 1)),
            dtype=torch.float32,
        )

        # Target
        y = torch.tensor(df[target_field].values, dtype=torch.float32)

        return x_num, y

    def train(
        self,
        df: pd.DataFrame,
        target_fields: Optional[List[str]] = None,
        epochs: int = 50,
        lr: float = 1e-3,
        batch_size: int = 32,
    ) -> Dict[str, float]:
        """Train imputation models for each target field.

        Args:
            df: Training DataFrame (should have no NaN in target fields).
            target_fields: Fields to train imputers for. Defaults to
                mass_earth, radius_earth, equilibrium_temp_k.
            epochs: Number of training epochs.
            lr: Learning rate.
            batch_size: Batch size.

        Returns:
            Dictionary with training loss per field.
        """
        if target_fields is None:
            target_fields = ["mass_earth", "radius_earth", "equilibrium_temp_k"]

        # Encode categoricals (fit on full data)
        x_cat = self._encode_categoricals(df, fit=True)

        losses = {}
        for field in target_fields:
            if field not in df.columns:
                print(f"Warning: Field {field} not in DataFrame, skipping")
                continue

            # Filter rows with non-NaN target
            valid_mask = df[field].notna()
            valid_df = df[valid_mask].copy()
            if len(valid_df) < 5:
                print(f"Warning: Too few samples for {field} ({len(valid_df)}), skipping")
                continue

            x_num, y = self._prepare_numerical(valid_df, field, fit=True)
            x_cat_valid = x_cat[valid_mask.values] if x_cat is not None else None

            # Get category sizes
            n_categories = []
            if x_cat_valid is not None:
                for col in self.categorical_cols:
                    n_categories.append(
                        max(len(self._cat_encoders.get(col, {})) + 1, 2)
                    )

            n_numerical = x_num.size(1)

            model = _TabTransformerModel(
                n_numerical=n_numerical,
                n_categories=n_categories,
                output_dim=1,
            ).to(self.device)

            optimizer = torch.optim.AdamW(model.parameters(), lr=lr)
            criterion = nn.MSELoss()

            # Create DataLoader
            x_num_d = x_num.to(self.device)
            y_d = y.to(self.device).unsqueeze(1)
            if x_cat_valid is not None:
                x_cat_d = x_cat_valid.to(self.device)
                dataset = TensorDataset(x_num_d, x_cat_d, y_d)
            else:
                dataset = TensorDataset(x_num_d, y_d)

            dataloader = DataLoader(
                dataset, batch_size=batch_size, shuffle=True, drop_last=False
            )

            model.train()
            final_loss = 0.0

            for epoch in range(epochs):
                epoch_loss = 0.0
                n_batches = 0

                for batch in dataloader:
                    if x_cat_valid is not None:
                        b_num, b_cat, b_y = batch
                    else:
                        b_num, b_y = batch
                        b_cat = None

                    optimizer.zero_grad()
                    pred = model(b_num, b_cat)
                    loss = criterion(pred, b_y)
                    loss.backward()
                    optimizer.step()

                    epoch_loss += loss.item()
                    n_batches += 1

                final_loss = epoch_loss / max(n_batches, 1)

            self.models[field] = model
            losses[field] = final_loss

        return losses

    def predict(self, df_or_row, target_fields: Optional[List[str]] = None) -> Dict[str, float]:
        """Predict missing values for target fields.

        Args:
            df_or_row: DataFrame or Series with input features.
            target_fields: Fields to predict. Defaults to all trained fields.

        Returns:
            Dictionary mapping field names to predicted values.
        """
        if not self.models:
            raise RuntimeError("No models trained. Call train() first.")

        if isinstance(df_or_row, pd.Series):
            df_or_row = pd.DataFrame([df_or_row])

        if target_fields is None:
            target_fields = list(self.models.keys())

        predictions = {}
        for field in target_fields:
            if field not in self.models:
                continue

            model = self.models[field]
            model.eval()

            x_num, _ = self._prepare_numerical(df_or_row, field, fit=False)
            x_cat = self._encode_categoricals(df_or_row, fit=False)

            x_num_d = x_num.to(self.device)
            x_cat_d = x_cat.to(self.device) if x_cat is not None else None

            with torch.no_grad():
                t0 = time.perf_counter()
                pred = model(x_num_d, x_cat_d)
                elapsed_ms = (time.perf_counter() - t0) * 1000
                self._inference_times.append(elapsed_ms)

            predictions[field] = pred.squeeze().cpu().item()

        return predictions

    def get_latency_ms(self) -> float:
        """Return average inference latency in milliseconds."""
        if not self._inference_times:
            return 0.0
        return np.mean(self._inference_times)
