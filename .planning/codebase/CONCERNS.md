# Codebase Concerns

**Analysis Date:** 2026-02-28

## Security Issues

**Process Execution via popen():**
- Issue: Multiple code paths execute shell commands using `popen()` without proper escaping or validation
- Files: `src/ai/BedrockClient.cpp` (lines 18, 166), `src/ai/ImageGenerator.cpp` (lines 24, 59, 133, 218)
- Risk: Command injection if user input reaches these functions; AWS CLI invocation is moderately safe (model IDs are fixed) but region strings are used in command construction
- Current mitigation: Model IDs and regions are developer-configured, not user-supplied; AWS CLI handles authentication safely
- Recommendations: Replace `popen()` with safer subprocess APIs; validate/sanitize all command parameters; consider using AWS SDK directly instead of CLI shelling

**SQL Injection in ADQL Queries:**
- Issue: User-provided search terms are concatenated directly into ADQL WHERE clauses
- Files: `src/data/NasaApiClient.cpp` (lines 290, 299, 305)
- Risk: Malicious search strings could break ADQL syntax or alter query semantics
- Current mitigation: Input comes from UI search fields; queries use LIKE with wildcards (% characters)
- Recommendations: Implement parameterized query support or validate search input more strictly; ADQL lacks parameterization, so consider input length limits and character whitelisting

**Missing File Cleanup on Error:**
- Issue: Temporary files created for API requests may not be cleaned up if exceptions occur
- Files: `src/ai/BedrockClient.cpp` (lines 148-204), `src/ai/ImageGenerator.cpp` (lines 118-131, 205-225)
- Risk: `/tmp/bedrock_*.json` files accumulate if errors occur between creation and cleanup
- Current mitigation: `std::remove()` is called in normal path but not in exception handlers
- Recommendations: Use RAII pattern (e.g., unique_ptr with custom deleter) for temporary file management

## Error Handling Gaps

**Silent Failures in async Operations:**
- Issue: `std::async` operations launched with `std::launch::async` can throw exceptions that are lost if futures are not awaited
- Files: `src/data/NasaApiClient.cpp` (lines 285-321), `src/ai/BedrockClient.cpp` (line 216), `src/ai/InferenceEngine.cpp` (line 140)
- Impact: Network failures, JSON parse errors, or API failures in background threads are never logged if caller doesn't call `.get()` on the future
- Safe modification: Always call `.get()` on returned futures or log exceptions; consider wrapper functions that handle exceptions centrally
- Test coverage: Async paths have no error handling verification

**Exception Handling Mismatch:**
- Issue: Only 4 files have try-catch blocks; many files with external APIs have none
- Files: `src/ui/UIManager.cpp`, `src/render/TextureLoader.cpp`, `src/render/SphereMesh.cpp`, `src/render/ShaderProgram.cpp`, `src/render/Renderer.cpp` have 0 exception handlers
- Risk: JSON parsing (`nlohmann::json::parse()`) can throw; CURL operations can fail; API responses may be malformed
- Current: `src/ai/BedrockClient.cpp` (lines 131-134), `src/ai/ImageGenerator.cpp` (lines 151-154), `src/data/NasaApiClient.cpp` (lines 138-183) have catch blocks
- Recommendations: Add try-catch around all JSON parse operations and external API calls; define exception hierarchy for application-specific errors

**Resource Cleanup on Init Failure:**
- Issue: If Application initialization fails mid-way, resources allocated by early steps are not released
- Files: `src/core/Application.cpp` (lines 17-54)
- Risk: GLFW window, logger, renderer, or UI may be partially initialized; destructor cleanup assumes all steps succeeded
- Safe modification: Use RAII or explicit rollback on failure; check each `make_unique()` result before proceeding

## Performance Bottlenecks

**Large 3D Noise Texture Generation:**
- Issue: 512x512x512 3D texture (128 MB) generated every frame initialization
- Files: `src/render/Renderer.cpp` (lines 61-91, specifically line 30)
- Cause: Linear Congruential Generator fills 134 million texels one by one in main memory before GPU upload
- Improvement path: Pre-generate texture once and cache; use GPU shader-based generation; reduce to 256x256x256 or on-demand mip generation
- Current: Happens once at startup, but CPU time is not optimized

**Blocking Bedrock/NASA API Calls:**
- Issue: `inferSync()` and `queryByNameSync()` are synchronous and block the render loop
- Files: `src/ai/InferenceEngine.cpp` (lines 87, 110), `src/data/NasaApiClient.cpp` (lines 295-301)
- Risk: 30-second NASA API timeout or Bedrock inference latency (100-500ms) freezes UI
- Current: Async versions exist but are not used in UI code; UI calls sync versions
- Improvement path: Use only async versions in UI; implement loading indicators; add request timeouts

**Unbounded Memory in Response Buffers:**
- Issue: CURL response buffer appends all response data with no size limit
- Files: `src/data/NasaApiClient.cpp` (lines 17-20 callback, line 139)
- Risk: Large responses (millions of exoplanet records) could exhaust memory
- Current: NASA API queries return at most 100-50 planets by default; CURL has 30-second timeout
- Improvement path: Add buffer size limit; stream JSON parsing instead of loading entire response

**Uncontrolled Thread Spawn:**
- Issue: Each async call creates a new thread; no thread pool or queue
- Files: `src/data/NasaApiClient.cpp` (lines 318-322 - `queryAll(100)` creates a thread), `src/ai/InferenceEngine.cpp` (line 141)
- Risk: Rapid successive async queries (e.g., user clicks multiple times) spawn unlimited threads
- Current: No thread limiting mechanism; OS may reject thread creation after ~1000 threads
- Improvement path: Use thread pool (e.g., Boost.ThreadPool or custom) with queue depth limit

## Fragile Areas

**ADQL Query Building:**
- Files: `src/data/NasaApiClient.cpp` (lines 64-98)
- Why fragile: Hard-coded column names in 22-line query; single typo breaks all queries; no compile-time checking
- Safe modification: Extract column list to enum; validate against schema on startup; use parameterized queries if NASA TAP adds support
- Test coverage: No unit tests for query construction; only integration tests (which require network)

**JSON Response Parsing with Fallback Logic:**
- Files: `src/data/NasaApiClient.cpp` (lines 151-164)
- Why fragile: NASA TAP response structure detection tries array, then "data", then "results", then raw object; different branches may parse different structures incorrectly
- Safe modification: Document which query types return which structures; validate structure before parsing; add strict schema validation
- Test coverage: No tests for malformed responses; live API tests only

**Shader Loading Without Validation:**
- Files: `src/render/Renderer.cpp` (lines 25-27)
- Why fragile: Shader compilation failure logged but execution continues with uninitialized shader program
- Safe modification: Return error code from `loadFromFiles()`; halt initialization if shaders fail; provide fallback shader
- Test coverage: Shader compilation errors are not tested

**Hard-Coded Model IDs and Endpoints:**
- Files: `src/ai/BedrockClient.hpp` (line 14), `src/data/NasaApiClient.hpp` (line 12), `src/ai/ImageGenerator.cpp` (line 126)
- Why fragile: Model versions, regions, and API endpoints embedded in code; updating requires recompilation
- Safe modification: Load from config file or environment variables; add version checking
- Risk: Model ID `us.anthropic.claude-sonnet-4-5-20250929-v1:0` may be deprecated or renamed

## Concurrency & Thread Safety Issues

**CURL Handle Reuse Across Threads:**
- Issue: Single `CURL*` handle in `NasaApiClient::Impl` is used by multiple async threads
- Files: `src/data/NasaApiClient.cpp` (line 122)
- Risk: CURL handles are not thread-safe; simultaneous `curl_easy_perform()` calls from different threads cause data races
- Current: Each thread gets own `std::async`, so concurrent queries from UI + background queries could race
- Fix approach: Create thread-local or request-local CURL handles; use thread pool with per-thread handles

**No Synchronization in Renderer State:**
- Issue: `m_quadVAO`, `m_quadVBO`, `m_noiseTexture` globals modified in `init()` and `resize()` without locking
- Files: `src/render/Renderer.cpp` (lines 13-15, 45-46, 78)
- Risk: If UI resizes window while render thread uses VAO, glDelete calls may race with glDraw calls
- Current: Likely single-threaded (render loop in main thread), but not enforced
- Safe modification: Document threading model; use explicit synchronization if multi-threaded rendering is added

**Raw Pointer Lifetime Issues:**
- Issue: `GLFWwindow*` and `CURL*` pointers could outlive their objects if exceptions occur
- Files: `src/core/Window.hpp`, `src/data/NasaApiClient.cpp`
- Current: Destructors clean them up, but no exception safety guarantee
- Recommendations: Use `std::unique_ptr<T, CustomDeleter>` instead of manual cleanup

## Missing Critical Features

**No Timeout Handling for Long-Running Inference:**
- Issue: `BedrockClient::inferSync()` calls blocking `popen()` with no timeout
- Files: `src/ai/BedrockClient.cpp` (lines 156-188)
- Impact: If AWS CLI hangs, render loop freezes indefinitely; user must kill process
- Fix approach: Use `select()` or platform-specific timeout on process pipes; implement request cancellation

**No Cache Validation:**
- Issue: NasaApiClient creates `.cache/nasa` directory but cache logic is not implemented
- Files: `src/data/NasaApiClient.hpp` (line 14), `src/data/NasaApiClient.cpp` (lines 35-36)
- Impact: Every query hits NASA API; configuration says cache is used but it is not
- Fix approach: Implement cache key generation (query hash); store/retrieve from cache before network call; add TTL

**No Credential Validation:**
- Issue: AWS credentials checked only by running `aws --version`, not by attempting actual Bedrock call
- Files: `src/ai/BedrockClient.cpp` (lines 16-27)
- Impact: User may configure invalid AWS account/region; error only occurs when inference is attempted
- Fix approach: Call `aws sts get-caller-identity` or `aws bedrock list-foundation-models` to validate credentials early

## Test Coverage Gaps

**Untested Error Paths:**
- What's not tested: Malformed JSON responses, network timeouts, API errors, credential failures, file I/O errors
- Files: `src/ai/BedrockClient.cpp`, `src/data/NasaApiClient.cpp`, `src/ai/ImageGenerator.cpp`
- Risk: Exception paths in production may crash or produce cryptic errors
- Priority: High — these are external service integrations most likely to fail

**No Tests for Query Construction:**
- What's not tested: ADQL query building, SQL injection prevention, URL encoding
- Files: `src/data/NasaApiClient.cpp` (lines 46-62, 64-98)
- Risk: Query mutations or special characters could break NASA API calls silently
- Priority: High — query logic is fragile and not validated

**No Shader Compilation Tests:**
- What's not tested: Shader loading failures, missing shader files, invalid GLSL syntax
- Files: `src/render/ShaderProgram.cpp`, `src/render/Renderer.cpp`
- Risk: Users see black screen if shaders fail to load with no error indication
- Priority: Medium — affects user experience but not safety

## Dependencies at Risk

**AWS CLI Dependency:**
- Risk: Core AI features depend on external `aws` CLI tool; not bundled
- Impact: Feature disabled silently if AWS CLI not installed (user may not notice)
- Migration plan: Use AWS SDK for C++ (awssdk-cpp) instead of shelling to CLI; better error handling, no subprocess risk

**Hardcoded Bedrock Model Version:**
- Risk: Model ID `us.anthropic.claude-sonnet-4-5-20250929-v1:0` is version-specific; may be deprecated
- Impact: Inference fails with cryptic API error if model removed
- Migration plan: Use model ID from config/environment; implement model discovery via AWS API

## Scaling Limits

**Query Result Set Size:**
- Current capacity: NASA API queries default to limit 100 planets (queryAll) or 50 (queryByName)
- Limit: Response parsing into single `std::vector<ExoplanetData>` would fail with >10k records (memory)
- Scaling path: Paginate results; stream JSON parsing; implement lazy loading

**Simultaneous API Requests:**
- Current capacity: Unbounded async threads (one per request)
- Limit: OS thread creation failure after ~1000 threads; memory exhaustion before that
- Scaling path: Thread pool with configurable depth (e.g., 8-16 threads); request queue with max size

---

*Concerns audit: 2026-02-28*
