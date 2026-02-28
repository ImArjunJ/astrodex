# Model Architecture Recommendations for Astrodex

## The Problem
Given partial exoplanet observational data (mass, radius, temperature, orbital period, host star type), infer missing parameters for visualization (atmosphere composition, surface type, ocean coverage, biome, colors).

---

## Recommended Architecture Stack

### Tier 1: Production (Claude/Bedrock) — Already Integrated
- **Model**: Claude via AWS Bedrock
- **Task**: Full structured inference from partial data → JSON output
- **Strengths**: Best scientific reasoning, handles novel combinations, structured output, confidence levels
- **Weakness**: API latency (~2-5s), cost per query, requires internet
- **Status**: Built (`InferenceEngine`, `PromptTemplates`), needs wiring

### Tier 2: Experimental — Worth Benchmarking

#### BERT (Encoder-Only)
- **HF Model**: `bert-base-uncased` or `allenai/scibert_scivocab_uncased` (science-trained)
- **Task**: Planet type classification (Rocky / Super-Earth / Gas Giant / Ice Giant / etc.)
- **Approach**: Fine-tune with LoRA adapter on exoplanet catalog labeled data
- **Strengths**: Fast (~10ms inference), runs locally, good for categorical output
- **Weakness**: Cannot generate structured text or numerical predictions
- **Relevant**: [bert-lora-adapter-finetuning](https://github.com/Ofekirsh/bert-lora-adapter-finetuning)

#### BART (Encoder-Decoder)
- **HF Model**: `facebook/bart-base` or `facebook/bart-large`
- **Task**: Text-to-structured-data (description → parameter JSON)
- **Approach**: Fine-tune on pairs of (partial data description → full parameter JSON)
- **Strengths**: Can generate structured output, moderate quality
- **Weakness**: Needs substantial training data, weaker reasoning than Claude
- **Note**: BARTpho is Vietnamese-specific — not relevant for this task

#### TabTransformer
- **HF Reference**: [TabTransformer paper](https://arxiv.org/abs/2012.06678)
- **Task**: Tabular regression — predict missing numerical columns from known ones
- **Approach**: Train on complete exoplanet records, mask columns, predict
- **Strengths**: Purpose-built for tabular data, handles mixed types
- **Weakness**: Needs training data, less interpretable than LLM reasoning
- **Implementation**: PyTorch, can be served via ONNX for C++ integration

#### ReMasker (Masked Autoencoding for Tabular Data)
- **Paper**: [ReMasker: Imputing Tabular Data with Masked Autoencoding](https://huggingface.co/papers/2309.13793)
- **Task**: Missing value imputation in tabular datasets
- **Approach**: Masked autoencoder trained on exoplanet catalog
- **Strengths**: State-of-the-art for tabular imputation, handles MCAR/MAR/MNAR
- **Weakness**: Research-grade, less production-ready

#### T5 / Flan-T5 (Text-to-Text)
- **HF Model**: `google/flan-t5-base` or `google/flan-t5-large`
- **Task**: Structured inference via text-to-text
- **Approach**: Few-shot prompting or fine-tune on (partial data → full data) pairs
- **Strengths**: Flexible, good at structured output, runs locally
- **Weakness**: Smaller context window, less scientific knowledge than Claude

#### SciBERT
- **HF Model**: `allenai/scibert_scivocab_uncased`
- **Task**: Scientific text understanding, feature extraction
- **Approach**: Use as embedding model for exoplanet descriptions, similarity search
- **Strengths**: Pre-trained on scientific papers, understands astronomy terminology
- **Weakness**: Same encoder-only limitations as BERT

---

## Recommended Experimental Plan

### Phase 1: Baseline (Claude/Bedrock)
- Wire existing InferenceEngine
- Measure: accuracy, latency, cost per planet
- Create gold-standard test set of ~50 well-characterized exoplanets

### Phase 2: Classification Experiment (BERT + LoRA)
- Fine-tune SciBERT on planet type classification
- Training data: NASA catalog labels + calculated types
- Compare: accuracy vs Claude, latency improvement

### Phase 3: Tabular Imputation (TabTransformer or ReMasker)
- Train on complete exoplanet records (mask & predict)
- Focus on numerical fields: mass, radius, temp, density
- Compare: RMSE vs Claude predictions vs true values

### Phase 4: Generation Experiment (BART or Flan-T5)
- Fine-tune on (partial → full) parameter pairs
- Evaluate structured JSON output quality
- Compare: semantic accuracy vs Claude

### Metrics
| Metric | Method |
|--------|--------|
| Numerical accuracy | RMSE against known values (holdout set) |
| Classification accuracy | F1 score for planet type |
| Inference latency | Wall-clock ms per prediction |
| Cost | $/1000 predictions (Claude API vs local compute) |
| Completeness | % of render parameters successfully filled |
