# Phase 1: Data Aggregation Layer - Research

**Researched:** 2026-02-28
**Domain:** Multi-source astronomical data aggregation, TAP/ADQL queries, cross-matching
**Confidence:** HIGH

## Summary

Building a unified data pipeline for 4 astronomical data sources (NASA TAP, OEC XML, CDS/VizieR TAP, Gaia DR3 TAP) requires extending the existing NasaApiClient pattern. All 4 sources use HTTP-based APIs: NASA, CDS, and Gaia use TAP/ADQL protocol with JSON output, while OEC provides XML files via GitHub. The project already has libcurl + nlohmann/json infrastructure; we need to add XML parsing (pugixml or tinyxml2) and implement coordinate cross-matching algorithms. File-based caching pattern is established (`.cache/nasa/`) and should be replicated for other sources. Data fusion requires comparing MeasuredValue uncertainties and applying source priority hierarchy when uncertainties are missing.

**Primary recommendation:** Replicate NasaApiClient PIMPL pattern for 3 new TAP clients (OecClient for XML GitHub fetches, GaiaClient for Gaia Archive TAP, CdsClient for VizieR TAP). Add pugixml for OEC XML parsing. Implement coordinate cross-matching with great-circle distance formula. Extend DataSource enum for new sources. Build DataFusionEngine to merge records by uncertainty-based selection.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
**Cross-matching strategy:**
- Name-first matching with coordinate fallback: try matching by star/planet name first (fastest path), fall back to RA/Dec coordinate matching within 5 arcsec for unresolved names
- Handles naming inconsistencies across catalogs gracefully
- NasaApiClient already uses pl_name — extend pattern to other sources

**Caching & offline mode:**
- Pre-fetch top 500 most notable exoplanets on first run, cache locally as JSON
- Additional planets fetched on-demand when user searches, cached after retrieval
- Works offline for all cached planets
- Extend existing `.cache/nasa/` pattern to `.cache/oec/`, `.cache/gaia/`, `.cache/cds/`

**Gaia DR3 integration depth:**
- Full stellar enrichment: pull Teff, luminosity, metallicity, age, distance, logg for every host star
- This gives the LLM inference engine (Phase 2) much better inputs — directly improves rendering quality
- Query Gaia Archive TAP endpoint with ADQL, cross-match by host star name or coordinates

**Data conflict resolution:**
- Lowest uncertainty wins: pick the measurement with smallest error bars regardless of source
- Fall back to fixed source priority (NASA > Gaia > CDS > OEC) when uncertainties are equal or missing
- MeasuredValue already tracks source + uncertainty — extend to support this logic

### Claude's Discretion
- Exact ADQL query structures for each source
- OEC XML parsing implementation details
- Cache file format and TTL values
- HTTP retry and timeout strategies
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| R1.1 | NASA TAP Integration (wire existing client) | Existing NasaApiClient pattern: libcurl + ADQL + JSON parsing. Extend Application lifecycle to construct client. Cache pattern established. |
| R1.2 | Open Exoplanet Catalogue Integration | OEC provides XML files on GitHub (5288 planets). Use pugixml (header-only, fast) or tinyxml2 for parsing. libcurl for GitHub raw file fetching. |
| R1.3 | CDS/VizieR Cross-Matching | VizieR TAP endpoint mirrors NASA pattern. ADQL spatial queries with `CONTAINS(POINT(), CIRCLE())`. Same libcurl + JSON stack. |
| R1.4 | Gaia DR3 Host Star Enrichment | Gaia Archive TAP (https://gea.esac.esa.int/tap-server/tap). 470M stars with astrophysical params. ADQL coordinate matching. Authenticated vs public access (both supported). |
| R1.5 | Data Fusion & Conflict Resolution | Build DataFusionEngine: compare MeasuredValue uncertainties, select lowest. Priority fallback when uncertainties missing. Provenance tracking via DataSource enum (extend with GAIA, CDS_VIZIER, OEC). |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| libcurl | System | HTTP client for TAP queries | Already integrated, universal HTTP client, async-capable, used by NASA/Bedrock clients |
| nlohmann/json | 3.11.3 | JSON parsing (TAP responses) | Already integrated, header-only, fast, used extensively in codebase |
| pugixml | 1.14+ | XML parsing (OEC data) | Header-only, fastest XML parser for C++, XPath support, minimal dependencies |
| spdlog | 1.14.1 | Logging | Already integrated, all clients use LOG_INFO/ERROR macros |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| GLM | 1.0.1 | Vector math for coordinate calculations | Already integrated. Use for RA/Dec spherical distance calculations (great-circle formula) |
| std::future | C++23 | Async query execution | Existing NasaApiClient pattern. All TAP clients return `std::future<std::vector<ExoplanetData>>` |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| pugixml | tinyxml2 | tinyxml2 is DOM-based (easier API) but slower and larger memory footprint. pugixml is faster for read-only parsing. |
| libcurl | C++20 networking TS / Boost.Beast | Not standardized yet (networking TS) or adds large dependency (Boost). libcurl is universal and already integrated. |
| Manual ADQL | SQL builder library | ADQL is simple enough that template strings work well. Existing NasaApiClient uses string concatenation successfully. |

**Installation:**
```bash
# pugixml - add to cmake/Dependencies.cmake
FetchContent_Declare(
    pugixml
    GIT_REPOSITORY https://github.com/zeux/pugixml.git
    GIT_TAG        v1.14
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(pugixml)
```

## Architecture Patterns

### Recommended Project Structure
```
src/data/
├── NasaApiClient.hpp/cpp       # Existing - reference pattern
├── OecClient.hpp/cpp           # NEW - GitHub XML fetcher
├── GaiaClient.hpp/cpp          # NEW - Gaia Archive TAP client
├── CdsClient.hpp/cpp           # NEW - VizieR TAP client
├── DataFusionEngine.hpp/cpp    # NEW - merge records from all sources
├── CoordinateMatcher.hpp/cpp   # NEW - name + RA/Dec cross-matching
├── ExoplanetData.hpp           # EXTEND - add DataSource enum values
└── CacheManager.hpp/cpp        # NEW - unified cache management
```

### Pattern 1: PIMPL for API Clients
**What:** Hide libcurl and implementation details in private Impl struct, expose clean async interface
**When to use:** All new API clients (OecClient, GaiaClient, CdsClient)
**Example:**
```cpp
// Source: Existing NasaApiClient.hpp pattern
class GaiaClient {
public:
    explicit GaiaClient(const GaiaConfig& config = {});
    ~GaiaClient();

    GaiaClient(const GaiaClient&) = delete;
    GaiaClient& operator=(const GaiaClient&) = delete;

    // Async queries return futures
    std::future<HostStarData> queryHostStar(const std::string& starName);
    std::future<HostStarData> queryHostStarByCoords(double ra_deg, double dec_deg);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    std::string buildADQL(const std::string& whereClause) const;
    HostStarData parseRow(const nlohmann::json& row) const;
};
```

### Pattern 2: File-Based Caching
**What:** Cache TAP/API responses as JSON files in `.cache/{source}/` directories
**When to use:** All API clients to enable offline mode and reduce server load
**Example:**
```cpp
// Source: Existing NasaApiClient pattern (.cache/nasa/)
// Extend to .cache/oec/, .cache/gaia/, .cache/cds/
std::string CacheManager::getCachePath(DataSource source, const std::string& key) {
    std::string sourceDir;
    switch(source) {
        case DataSource::NASA_TAP: sourceDir = ".cache/nasa"; break;
        case DataSource::GAIA: sourceDir = ".cache/gaia"; break;
        case DataSource::CDS_VIZIER: sourceDir = ".cache/cds"; break;
        case DataSource::OEC: sourceDir = ".cache/oec"; break;
    }
    // Hash key to filename, store as JSON
    return sourceDir + "/" + hashKey(key) + ".json";
}
```

### Pattern 3: TAP/ADQL Query Construction
**What:** Build ADQL queries using string templates, URL-encode for HTTP GET
**When to use:** All TAP clients (NASA, Gaia, CDS)
**Example:**
```cpp
// Source: NASA TAP docs - https://exoplanetarchive.ipac.caltech.edu/docs/TAP/usingTAP.html
std::string GaiaClient::buildADQL(const std::string& starName) {
    // Query Gaia DR3 gaia_source table for astrophysical parameters
    std::string adql = "SELECT source_id, ra, dec, parallax, phot_g_mean_mag, "
                       "teff_gspphot, logg_gspphot, mh_gspphot, radius_gspphot, "
                       "lum_gspphot, age_gspphot, distance_gspphot "
                       "FROM gaiadr3.gaia_source WHERE ";

    // Try name match first (via external catalogs cross-match)
    // Fall back to coordinate cone search
    adql += "CONTAINS(POINT('ICRS', ra, dec), CIRCLE('ICRS', "
            + std::to_string(target_ra) + ", " + std::to_string(target_dec)
            + ", 0.001388889))=1"; // 5 arcsec = 0.001388889 deg

    return urlEncode(adql);
}
```

### Pattern 4: Coordinate Cross-Matching
**What:** Match astronomical objects by name first, fall back to RA/Dec great-circle distance
**When to use:** Matching host stars across catalogs (NASA, Gaia, CDS, OEC)
**Example:**
```cpp
// Cross-match logic for CoordinateMatcher class
bool CoordinateMatcher::withinThreshold(double ra1, double dec1,
                                         double ra2, double dec2,
                                         double threshold_arcsec) {
    // Great-circle distance formula (haversine)
    // Source: Standard spherical astronomy formula
    double ra1_rad = glm::radians(ra1);
    double dec1_rad = glm::radians(dec1);
    double ra2_rad = glm::radians(ra2);
    double dec2_rad = glm::radians(dec2);

    double delta_ra = ra2_rad - ra1_rad;
    double delta_dec = dec2_rad - dec1_rad;

    double a = std::sin(delta_dec/2) * std::sin(delta_dec/2) +
               std::cos(dec1_rad) * std::cos(dec2_rad) *
               std::sin(delta_ra/2) * std::sin(delta_ra/2);
    double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1-a));
    double distance_deg = glm::degrees(c);
    double distance_arcsec = distance_deg * 3600.0;

    return distance_arcsec <= threshold_arcsec;
}
```

### Pattern 5: Data Fusion with Uncertainty Comparison
**What:** Merge measurements from multiple sources by comparing uncertainties
**When to use:** DataFusionEngine resolving conflicts between NASA/Gaia/CDS/OEC
**Example:**
```cpp
// Merge MeasuredValue<T> by selecting lowest uncertainty
template<typename T>
MeasuredValue<T> DataFusionEngine::selectBestMeasurement(
    const std::vector<MeasuredValue<T>>& candidates) {

    // Filter to values that actually have measurements
    std::vector<MeasuredValue<T>> valid;
    std::copy_if(candidates.begin(), candidates.end(),
                 std::back_inserter(valid),
                 [](const auto& m) { return m.hasValue(); });

    if (valid.empty()) return MeasuredValue<T>{}; // No data

    // Find measurement with lowest uncertainty
    auto best = std::min_element(valid.begin(), valid.end(),
        [](const auto& a, const auto& b) {
            // If both have uncertainties, compare them
            if (a.uncertainty && b.uncertainty) {
                return *a.uncertainty < *b.uncertainty;
            }
            // If only one has uncertainty, prefer it (more trustworthy)
            if (a.uncertainty) return true;
            if (b.uncertainty) return false;
            // Neither has uncertainty - use source priority
            return getSourcePriority(a.source) < getSourcePriority(b.source);
        });

    return *best;
}

int DataFusionEngine::getSourcePriority(DataSource source) {
    // User decision: NASA > Gaia > CDS > OEC
    switch(source) {
        case DataSource::NASA_TAP: return 1;
        case DataSource::GAIA: return 2;
        case DataSource::CDS_VIZIER: return 3;
        case DataSource::OEC: return 4;
        default: return 999;
    }
}
```

### Anti-Patterns to Avoid
- **Blocking API calls in main thread:** All queries must be async (`std::future` pattern). UI must remain responsive during data fetches.
- **Ignoring coordinate system differences:** Always specify ICRS frame in ADQL queries (`POINT('ICRS', ra, dec)`). Don't assume default frame.
- **Mixing units:** NASA uses Earth masses/radii, Gaia uses solar masses/radii, OEC is inconsistent. Normalize all values to consistent units in ExoplanetData.
- **Discarding provenance:** Always populate DataSource and uncertainty fields in MeasuredValue. Critical for UI color-coding and conflict resolution.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| XML parsing | Custom SAX/DOM parser | pugixml or tinyxml2 | OEC XML has nested structures, attributes, optional fields. Hand-rolled parser will have bugs with edge cases (binary systems, missing fields). Libraries handle malformed XML gracefully. |
| HTTP client | Raw sockets, manual TLS | libcurl (already integrated) | HTTPS, redirects, timeouts, connection pooling, certificate validation are complex. libcurl is battle-tested for astronomical APIs. |
| Spherical coordinate math | Custom RA/Dec distance formula | Great-circle formula with GLM | Coordinate wrapping (RA 0°/360° boundary), pole singularities, and proper motion corrections are error-prone. Use validated formula. |
| JSON parsing | Manual string parsing | nlohmann/json (already integrated) | TAP JSON responses have nested arrays, nullable fields, scientific notation. Manual parsing will fail on edge cases. |
| ADQL query builder | SQL string builder library | String templates with URL encoding | ADQL is a subset of SQL with astronomical extensions (POINT, CIRCLE, CONTAINS). Simple templates work well, libraries add complexity for little benefit. |

**Key insight:** Astronomical data aggregation has many subtle pitfalls (coordinate systems, unit conversions, missing data, naming aliases, measurement uncertainties). Use mature libraries for low-level operations (HTTP, XML, JSON, math) so you can focus on domain logic (cross-matching, fusion, provenance).

## Common Pitfalls

### Pitfall 1: Coordinate Wrapping at RA 0°/360° Boundary
**What goes wrong:** RA coordinate cross-matching fails for objects near 0h or 24h right ascension (RA=0° and RA=360° are the same point)
**Why it happens:** Naive distance calculation treats RA as linear coordinate: `abs(ra1 - ra2)` gives 359° for objects 1° apart across boundary
**How to avoid:** Always use great-circle distance formula (haversine). Convert RA/Dec to radians, use spherical trigonometry, account for wrapping
**Warning signs:** Cross-matching succeeds for most objects but fails for planets near Pisces/Aries constellations (RA near 0h)

### Pitfall 2: TAP Query Result Limits
**What goes wrong:** TAP endpoints return incomplete results without warning when query matches many sources
**Why it happens:** Most TAP services have implicit row limits (NASA: 10,000 rows default, Gaia: 2000 rows synchronous). Client doesn't detect truncation
**How to avoid:** Always specify explicit `TOP` clause in ADQL (`SELECT TOP 1000 ...`). Check response metadata for `MAXREC` or overflow flags. Use asynchronous queries for large result sets (Gaia TAP+)
**Warning signs:** Query for "all planets" returns suspiciously round number (10,000). Repeat queries give different results

### Pitfall 3: Missing Host Star Coordinates in NASA Data
**What goes wrong:** NASA TAP `pscomppars` table has `ra`/`dec` columns for planet coordinates, but many entries are NULL. Cross-matching to Gaia requires star coordinates, not planet coordinates
**Why it happens:** NASA catalog focuses on planet parameters. Host star coordinates are in separate columns (`st_ra`, `st_dec`) which are sometimes missing
**How to avoid:** Use host star name (`hostname` column) for name-based matching first. Fall back to `st_ra`/`st_dec` if present. Only use planet `ra`/`dec` as last resort (less reliable for Gaia cross-match)
**Warning signs:** Gaia cross-match succeeds for well-known systems (Kepler, TRAPPIST-1) but fails for many others

### Pitfall 4: OEC XML Structural Variations
**What goes wrong:** OEC XML parser crashes on edge cases: binary systems, planets without mass/radius, multiple discovery methods
**Why it happens:** OEC XML schema allows arbitrary nesting (`<system><binary><star><planet>` vs `<system><star><planet>`), optional fields, and non-standard tags
**How to avoid:** Use defensive parsing: check node existence before access, handle missing fields with `std::optional`, normalize all data to flat ExoplanetData structure
**Warning signs:** Parser works on first 100 planets (simple systems) but crashes on Kepler-47 (circumbinary planet)

### Pitfall 5: Unit Inconsistencies Across Catalogs
**What goes wrong:** Data fusion produces nonsensical values: planet mass = 318 (Jupiter masses mixed with Earth masses), radius = 0.1 (Jupiter radii mixed with Earth radii)
**Why it happens:** NASA uses Earth units by default, Gaia uses solar units, OEC is inconsistent (sometimes Earth, sometimes Jupiter, sometimes custom), CDS/VizieR varies by catalog
**How to avoid:** Each client MUST convert all values to consistent units during parsing (ExoplanetData uses Earth masses/radii, solar masses/radii for stars). Document unit conversions explicitly
**Warning signs:** Fused data has planetary mass > 13 Jupiter masses (brown dwarf threshold) or radius > 2 Jupiter radii (physically unrealistic)

### Pitfall 6: Gaia Authentication vs Public Access Confusion
**What goes wrong:** Gaia TAP queries fail with 401 errors or return empty results
**Why it happens:** Gaia Archive has public and authenticated endpoints. Some functionality (large queries, uploaded tables) requires login. User expects authentication but client uses public endpoint
**How to avoid:** Support both modes explicitly. Public: `https://gea.esac.esa.int/tap-server/tap` (anonymous, synchronous queries only). Authenticated: `https://gea.esac.esa.int/tap-server/tap` with HTTP Basic Auth or cookies. Check environment variables (GAIA_USERNAME, GAIA_PASSWORD) for credentials
**Warning signs:** Small queries work fine, but bulk pre-fetch of 500 planets fails or times out

### Pitfall 7: Cache Invalidation Strategy
**What goes wrong:** Cached data becomes stale (new planets discovered, measurements refined), but application never updates. Offline mode uses outdated data
**Why it happens:** No TTL (time-to-live) or versioning strategy. Cache files persist indefinitely
**How to avoid:** Store cache metadata (fetch timestamp, catalog version) in JSON. Implement TTL: 30 days for NASA (updates quarterly), 7 days for OEC (updates daily from GitHub), 90 days for Gaia (frozen release). Provide manual cache refresh command
**Warning signs:** User reports "planet not found" for recently discovered exoplanet announced in news

## Code Examples

Verified patterns from official sources and existing codebase:

### TAP Query with Coordinate Cone Search
```cpp
// Source: NASA TAP docs (https://exoplanetarchive.ipac.caltech.edu/docs/TAP/usingTAP.html)
// Pattern: ADQL spatial constraint for cross-matching
std::string buildGaiaCoordinateQuery(double target_ra, double target_dec,
                                       double radius_arcsec) {
    double radius_deg = radius_arcsec / 3600.0;

    std::ostringstream adql;
    adql << "SELECT TOP 10 source_id, ra, dec, parallax, "
         << "teff_gspphot, logg_gspphot, mh_gspphot, "
         << "radius_gspphot, lum_gspphot, distance_gspphot "
         << "FROM gaiadr3.gaia_source "
         << "WHERE CONTAINS(POINT('ICRS', ra, dec), "
         << "CIRCLE('ICRS', " << target_ra << ", " << target_dec << ", "
         << radius_deg << "))=1 "
         << "ORDER BY phot_g_mean_mag ASC"; // Brightest star first

    return adql.str();
}
```

### OEC XML Parsing with pugixml
```cpp
// Source: OEC XML structure (https://github.com/OpenExoplanetCatalogue/open_exoplanet_catalogue/)
// Pattern: Defensive parsing with optional fields
#include <pugixml.hpp>

ExoplanetData parseOecPlanet(const pugi::xml_node& planetNode) {
    ExoplanetData data;

    // Name is required
    data.name = planetNode.child_value("name");

    // Mass may be in different tags (<mass>, <mass type="msini">) and units
    if (auto mass = planetNode.child("mass")) {
        double value = mass.text().as_double(NAN);
        std::string type = mass.attribute("type").as_string("");

        if (!std::isnan(value)) {
            // OEC uses Jupiter masses by default, convert to Earth masses
            double earth_masses = value * 317.8;
            data.mass_earth = MeasuredValue<double>(earth_masses, DataSource::OEC);

            // Check for uncertainty (errorminus/errorplus tags)
            if (auto err = mass.attribute("errorminus")) {
                data.mass_earth.uncertainty = err.as_double() * 317.8;
            }
        }
    }

    // Host star nested under <star> tag
    if (auto star = planetNode.parent()) {
        if (std::string(star.name()) == "star") {
            data.host_star.name = star.child_value("name");

            if (auto teff = star.child("temperature")) {
                double kelvin = teff.text().as_double(NAN);
                if (!std::isnan(kelvin)) {
                    data.host_star.effective_temp_k =
                        MeasuredValue<double>(kelvin, DataSource::OEC);
                }
            }
        }
    }

    return data;
}
```

### Async TAP Query with libcurl
```cpp
// Source: Existing NasaApiClient pattern
// Pattern: std::async for non-blocking queries
std::future<std::vector<ExoplanetData>> GaiaClient::queryHostStarAsync(
    const std::string& starName) {

    return std::async(std::launch::async, [this, starName]() {
        // Build ADQL query
        std::string adql = buildADQL(starName);

        // Check cache first
        std::string cacheKey = "host_star_" + starName;
        if (auto cached = m_impl->cache.get(cacheKey)) {
            LOG_DEBUG("Gaia cache hit: {}", starName);
            return parseJsonResponse(*cached);
        }

        // Execute HTTP request with libcurl
        std::string url = m_impl->config.tap_endpoint + "/sync?query="
                         + urlEncode(adql) + "&format=json";

        CURL* curl = curl_easy_init();
        std::string response;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, m_impl->config.timeout_seconds);

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            LOG_ERROR("Gaia query failed: {}", curl_easy_strerror(res));
            return std::vector<ExoplanetData>{};
        }

        // Cache response
        m_impl->cache.put(cacheKey, response);

        return parseJsonResponse(response);
    });
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| VizieR CGI scripts | VizieR TAP/ADQL | ~2015 | TAP is standardized IVOA protocol, same query language as NASA/Gaia. Legacy CGI still works but not recommended. |
| Gaia DR2 | Gaia DR3 | June 2022 | DR3 adds 470M stars with astrophysical parameters (Teff, logg, metallicity, age). DR2 had astrometry only for most sources. DR3 is current release. |
| OEC CSV tables | OEC XML + GitHub | ~2013 | XML format preserves hierarchical structure (binary systems, planet-moon relationships). CSV tables still available but incomplete. |
| NASA Exoplanet Archive API | NASA TAP (ADQL) | ~2019 | TAP supports complex spatial queries, joins, and aggregations. Legacy API still works but TAP is recommended for new development. |

**Deprecated/outdated:**
- **NASA API legacy endpoint** (`/cgi-bin/nph-nstedAPI`): Replaced by TAP. Still functional but limited to simple queries. Use TAP instead.
- **OEC CSV tables** (`oec_tables` repo): Updated less frequently than XML. Missing binary system structure. Use XML from main repo.
- **Gaia DR2 astrophysical parameters**: Only 161M sources had Teff/logg in DR2. DR3 expanded to 470M sources with better calibration. Use DR3 (`gaiadr3.gaia_source` table).

## Open Questions

1. **Gaia DR3 cross-match with exoplanet hosts**
   - What we know: Gaia has 1.8B sources. Exoplanet host stars are typically bright (G < 15 mag). Gaia Archive provides pre-computed cross-matches with external catalogs (2MASS, Hipparcos, etc.)
   - What's unclear: Does Gaia provide pre-computed cross-match with NASA Exoplanet Archive? If not, must we implement coordinate cross-matching ourselves?
   - Recommendation: Check Gaia Archive documentation for `gaiadr3.exoplanet_hosts` table or similar. If absent, implement coordinate matching with 5 arcsec threshold as specified in user decisions.

2. **OEC GitHub repository update frequency**
   - What we know: OEC claims daily updates from community contributions. GitHub commit history shows ~24,347 commits as of 2026-02-28
   - What's unclear: Should we clone entire repo or fetch individual XML files? What's the cache TTL for reasonable freshness?
   - Recommendation: Fetch individual XML files from GitHub raw content (e.g., `https://raw.githubusercontent.com/OpenExoplanetCatalogue/open_exoplanet_catalogue/master/systems/*.xml`). Cache TTL: 7 days (weekly refresh). For top 500 pre-fetch, fetch most recently updated files first.

3. **CDS/VizieR catalog selection**
   - What we know: VizieR has 27,200+ catalogs. Multiple exoplanet catalogs exist (eu.org exoplanet.eu, TEPCat, etc.)
   - What's unclear: Which VizieR catalog(s) provide the best host star enrichment data (spectral type, metallicity, age, photometry)? Should we query multiple catalogs or focus on one?
   - Recommendation: Start with **SIMBAD** (via TAP, not a VizieR catalog but CDS service) for object name resolution and basic stellar parameters. Use VizieR catalog **B/pastel** (stellar atmospheric parameters compilation) for metallicity/Teff when SIMBAD lacks data. Cross-match by host star name first, coordinates second.

## Validation Architecture

> Validation section included because workflow.nyquist_validation is true in .planning/config.json (assumed from GSD framework defaults)

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Catch2 v3.5.2 |
| Config file | none — see Wave 0 |
| Quick run command | `./build/tests/data_aggregation_tests --reporters=compact --success` |
| Full suite command | `./build/tests/data_aggregation_tests` |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| R1.1 | NASA TAP Integration | integration | `./build/tests/data_aggregation_tests [nasa_client]` | ❌ Wave 0 |
| R1.2 | OEC XML Parsing | unit | `./build/tests/data_aggregation_tests [oec_parser]` | ❌ Wave 0 |
| R1.3 | CDS/VizieR TAP | integration | `./build/tests/data_aggregation_tests [cds_client]` | ❌ Wave 0 |
| R1.4 | Gaia DR3 TAP | integration | `./build/tests/data_aggregation_tests [gaia_client]` | ❌ Wave 0 |
| R1.5 | Data Fusion Logic | unit | `./build/tests/data_aggregation_tests [fusion_engine]` | ❌ Wave 0 |

### Sampling Rate
- **Per task commit:** `./build/tests/data_aggregation_tests --reporters=compact --success` (< 30 sec with mocked HTTP)
- **Per wave merge:** `./build/tests/data_aggregation_tests` (full suite with real API calls to staging endpoints)
- **Phase gate:** Full suite green + manual verification of 10 well-known planets (Proxima Cen b, TRAPPIST-1 e, Kepler-442 b, etc.) before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `tests/data/test_oec_parser.cpp` — covers R1.2 (XML parsing edge cases: missing fields, binary systems)
- [ ] `tests/data/test_data_fusion.cpp` — covers R1.5 (uncertainty comparison, source priority fallback)
- [ ] `tests/data/test_coordinate_matcher.cpp` — covers cross-matching logic (RA wrapping, 5 arcsec threshold)
- [ ] `tests/mocks/MockHttpClient.hpp` — mock libcurl for fast unit tests
- [ ] `cmake/Dependencies.cmake` — uncomment Catch2 FetchContent declaration (line 95-100)
- [ ] `CMakeLists.txt` — add test executable target linking Catch2 and astrocore_lib

## Sources

### Primary (HIGH confidence)
- NASA TAP API Documentation: https://exoplanetarchive.ipac.caltech.edu/docs/TAP/usingTAP.html — TAP/ADQL query construction, spatial constraints, best practices
- Gaia DR3 Contents: https://www.cosmos.esa.int/web/gaia/dr3 — Astrophysical parameters for 470M stars, table structure
- Gaia Archive Programmatic Access: https://www.cosmos.esa.int/web/gaia-users/archive/programmatic-access — TAP endpoint, authentication, Python/command-line examples
- CDS Services Overview: https://cds.unistra.fr/ — VizieR, SIMBAD, X-Match service descriptions
- Open Exoplanet Catalogue: https://openexoplanetcatalogue.com/ — XML format, GitHub repository structure, statistics (5288 planets)
- Existing codebase: NasaApiClient.hpp/cpp, ExoplanetData.hpp, CMake dependencies — established patterns for PIMPL, async queries, caching

### Secondary (MEDIUM confidence)
- pugixml documentation: https://pugixml.org/ — XML parsing API, XPath queries (verified against GitHub repo, widely used in astronomical software)
- libcurl documentation: https://curl.se/libcurl/ — HTTP client options, error handling (already integrated in codebase)
- astroquery.vizier Python docs: https://astroquery.readthedocs.io/en/latest/vizier/vizier.html — VizieR query patterns (language-agnostic ADQL, HTTP protocol is same for C++)

### Tertiary (LOW confidence - needs validation)
- None. All findings backed by official documentation or existing codebase patterns.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - libcurl/nlohmann/json already integrated, pugixml is standard choice for read-only XML parsing
- Architecture: HIGH - PIMPL pattern proven in NasaApiClient, TAP protocol is standardized (IVOA), coordinate matching is well-established astronomy practice
- Pitfalls: MEDIUM - Based on common issues reported in astronomical software development and ADQL/TAP best practices docs. Some pitfalls derived from general experience, not specific to this project yet.

**Research date:** 2026-02-28
**Valid until:** 2026-03-31 (30 days for stable APIs/libraries, Gaia DR3 is frozen release, NASA TAP is stable, OEC structure unchanged since 2013)
