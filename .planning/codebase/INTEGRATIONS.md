# External Integrations

**Analysis Date:** 2026-02-28

## APIs & External Services

**NASA Exoplanet Archive TAP (Astronomical Data Provider):**
- NASA's Exoplanet Archive API via TAP (Table Access Protocol)
- What it's used for: Querying exoplanet data (orbital parameters, host star properties, habitability data)
- SDK/Client: Custom C++ client with libcurl HTTP wrapper
- Implementation: `src/data/NasaApiClient.hpp`, `src/data/NasaApiClient.cpp`
- Endpoint: `https://exoplanetarchive.ipac.caltech.edu/TAP/sync`
- Query Language: ADQL (Astronomy Data Query Language)
- Auth: Public API - no authentication required

**AWS Bedrock (AI Inference):**
- Claude 4.5 Sonnet model via AWS Bedrock inference profile
- What it's used for: Inferring missing exoplanet properties (atmosphere composition, habitability, rendering hints)
- SDK/Client: Custom C++ client with AWS CLI invocation
- Implementation: `src/ai/BedrockClient.hpp`, `src/ai/BedrockClient.cpp`
- Model: `us.anthropic.claude-sonnet-4-5-20250929-v1:0`
- Auth: AWS credentials via environment variables or AWS CLI configuration
  - `AWS_ACCESS_KEY_ID` env var or AWS CLI config
  - `AWS_SECRET_ACCESS_KEY` env var or AWS CLI config
  - `AWS_REGION` defaults to `us-east-1`
- Availability: Runtime check for AWS CLI presence in `BedrockClient::Impl::checkAwsCli()`

**Amazon Titan Image Generator (Image Generation):**
- Image generation for procedural exoplanet visualization
- What it's used for: Generating planet textures from AI descriptions
- SDK/Client: Custom C++ client via AWS Bedrock
- Implementation: `src/ai/ImageGenerator.hpp`, `src/ai/ImageGenerator.cpp`
- Model: `amazon.titan-image-generator-v2:0`
- Configuration: 1024x1024 default output size
- Auth: Same AWS credentials as Bedrock

## Data Storage

**Databases:**
- None - application is read-only from external APIs

**File Storage:**
- Local filesystem only
  - Cache directory: `.cache/nasa/` (configurable in `NasaApiConfig`)
  - Shader cache: Built into executable at build time
  - Asset cache: Built into executable at build time
  - Generated images: Stored as in-memory buffers (RGBA pixel data in `GeneratedImage::data`)

**Caching:**
- NASA API responses: Optional file-based cache (enabled by default)
  - Location: `.cache/nasa/`
  - Config flag: `NasaApiConfig::use_cache = true`
  - Configurable directory via `NasaApiConfig::cache_directory`
- No persistent database; all runtime state is in-memory

## Authentication & Identity

**Auth Provider:**
- AWS IAM for Bedrock/Titan services
- NASA TAP: Public (no authentication)

**Implementation:**
- AWS CLI integration: BedrockClient checks for AWS CLI availability at startup
- Credentials sourced from:
  1. Environment variables (`AWS_ACCESS_KEY_ID`, `AWS_SECRET_ACCESS_KEY`)
  2. AWS CLI configuration (`~/.aws/config`, `~/.aws/credentials`)
  3. EC2 instance role (if deployed on AWS)
- Fallback behavior: If AWS CLI is unavailable, AI features are disabled with warning log
- Connection test: `BedrockClient::testConnection()` validates AWS CLI availability

## Monitoring & Observability

**Error Tracking:**
- None detected - no external error tracking service

**Logs:**
- spdlog file/console logging to stderr
- Log levels via macros: `LOG_TRACE`, `LOG_DEBUG`, `LOG_INFO`, `LOG_WARN`, `LOG_ERROR`, `LOG_CRITICAL`
- Initialization in `src/core/Logger.cpp`
- Used throughout:
  - CURL errors in `NasaApiClient.cpp`
  - AWS CLI availability in `BedrockClient.cpp`
  - GLFW window creation in `src/core/Window.cpp`

**Performance Metrics:**
- Latency tracking in `InferenceResponse::latency_ms` (populated by BedrockClient)

## CI/CD & Deployment

**Hosting:**
- Not detected - application is desktop/standalone
- Designed for local execution with OpenGL 4.5 GPU

**CI Pipeline:**
- Not detected

**Build Output:**
- Executable: `astrodex`
- Library: `libastrocore_lib.a` (static)
- Post-build steps copy shaders and assets to executable directory

## Environment Configuration

**Required env vars (for full functionality):**
- `AWS_ACCESS_KEY_ID` - AWS Bedrock access
- `AWS_SECRET_ACCESS_KEY` - AWS Bedrock access
- `AWS_REGION` - AWS region (optional, defaults to `us-east-1`)

**Optional env vars:**
- `NASA_API_KEY` - Not used (NASA TAP is public)
- None detected - application uses sensible defaults

**Secrets location:**
- AWS credentials: Standard AWS CLI locations
  - `~/.aws/credentials` (recommended)
  - `~/.aws/config` (alternative)
  - Environment variables (for CI/containerization)
- No embedded secrets in codebase

## Webhooks & Callbacks

**Incoming:**
- None - application is event-driven by user input

**Outgoing:**
- None - application makes request-response calls only
  - NASA TAP: One-way GET requests via ADQL query
  - AWS Bedrock: One-way POST requests for inference

## HTTP/Network Configuration

**Request Headers:**
- NASA TAP: Standard HTTP headers (content-type, user-agent)
- AWS Bedrock: AWS SigV4 signing via AWS CLI (not directly in C++)

**Timeouts:**
- NASA TAP: 30 seconds (configurable via `NasaApiConfig::timeout_seconds`)
- AWS Bedrock: Not explicitly set (uses AWS CLI defaults)

**SSL/TLS:**
- All endpoints use HTTPS
  - `https://exoplanetarchive.ipac.caltech.edu/TAP/sync`
  - AWS Bedrock endpoints (via AWS CLI)
- Certificate validation: Default libcurl behavior (enabled)

---

*Integration audit: 2026-02-28*
