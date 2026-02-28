# Exoplanet ML Benchmarking

Standalone Python harness for evaluating ML models on exoplanet parameter
prediction. Compares BERT, BART, TabTransformer, and a ReMasker-inspired
masked autoencoder against each other and against Claude (Bedrock) for
different inference tasks.

## Setup

```bash
cd experiments/ml
python -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

## Run

```bash
# Using cached exoplanet data from the C++ pipeline
python benchmark.py --cache-dir ../../.cache/fused/

# Using synthetic data (no cache needed)
python benchmark.py --synthetic

# Run specific models only
python benchmark.py --synthetic --models bert tab_transformer

# Generate more synthetic records
python benchmark.py --synthetic --n-synthetic 500
```

## Output

Results are written to `results/comparison.md` with:
- Classification accuracy (BERT for planet type)
- Numerical imputation error (TabTransformer and MaskedAutoencoder for mass, radius, temperature)
- Structured generation quality (BART for atmosphere parameters)
- Cost analysis comparing all models against Claude (Bedrock)
- Recommendations for which model to use in different scenarios

## Models

| Model | Task | Input | Output |
|-------|------|-------|--------|
| BERT Classifier | Classification | Tabular features as text | Planet type (Rocky, Super-Earth, etc.) |
| BART Generator | Structured generation | Known parameters as text | JSON with predicted atmosphere params |
| TabTransformer | Numerical imputation | Tabular features (mixed categorical + numerical) | Imputed numerical values |
| MaskedAutoencoder | Numerical imputation | Tabular features with masking | Reconstructed/imputed values |

## Architecture

```
experiments/ml/
  benchmark.py          # Main orchestrator
  data_loader.py        # Loads .cache/fused/*.json into DataFrame
  metrics.py            # MAE, RMSE, R2, accuracy, F1
  requirements.txt      # Python dependencies
  README.md             # This file
  models/
    __init__.py
    bert_classifier.py  # BERT-style planet type classifier
    bart_generator.py   # BART-style parameter generator
    tab_transformer.py  # TabTransformer numerical imputer
    remasker_imputer.py # Masked autoencoder imputer
  results/              # Generated at runtime
    comparison.md       # Comparison document
```

## Data

The harness reads JSON files from `.cache/fused/` produced by the C++ data
pipeline (`ExoplanetData::toJson()`). If the cache is empty or unavailable,
it generates synthetic test data with physically plausible distributions.

Note: Results from synthetic data are for pipeline testing only and are NOT
scientifically meaningful.
