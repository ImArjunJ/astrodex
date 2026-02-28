"""
Exoplanet ML model wrappers for benchmarking.

Each model provides a consistent interface:
    - __init__(): Initialize model architecture and load pretrained weights
    - train(df, ...): Train on a pandas DataFrame
    - predict(input): Run inference on a single record or batch
    - get_latency_ms(): Return average inference latency

Models:
    - BertPlanetClassifier: BERT for planet type classification
    - BartParameterGenerator: BART for structured parameter generation
    - TabTransformerImputer: TabTransformer for numerical field imputation
    - MaskedAutoencoderImputer: Simplified ReMasker for numerical imputation
"""
