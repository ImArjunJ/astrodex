#include "data/SolarSystemDatabase.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>

namespace astrocore {

// ─── Helpers ─────────────────────────────────────────────────────────────────

static ExoplanetData makePlanetData(const std::string& name,
                                    double mass_earth, double radius_earth,
                                    double temp_k,     double density_gcc,
                                    double gravity_g,  double pressure_atm,
                                    double albedo_val,
                                    double ocean_frac,  double cloud_frac,
                                    double ice_frac,
                                    const std::string& atmo_comp,
                                    const std::string& planet_type) {
    ExoplanetData d;
    d.name             = name;
    d.discovery_method = "Direct observation";

    d.mass_earth         = MeasuredValue<double>(mass_earth,     DataSource::NASA_TAP);
    d.radius_earth       = MeasuredValue<double>(radius_earth,   DataSource::NASA_TAP);
    d.equilibrium_temp_k = MeasuredValue<double>(temp_k,         DataSource::NASA_TAP);
    d.density_gcc        = MeasuredValue<double>(density_gcc,    DataSource::NASA_TAP);
    d.surface_gravity_g  = MeasuredValue<double>(gravity_g,      DataSource::NASA_TAP);
    d.surface_pressure_atm = MeasuredValue<double>(pressure_atm, DataSource::NASA_TAP);
    d.albedo             = MeasuredValue<double>(albedo_val,      DataSource::NASA_TAP);

    d.ocean_coverage_fraction = MeasuredValue<double>(ocean_frac,  DataSource::NASA_TAP);
    d.cloud_coverage_fraction = MeasuredValue<double>(cloud_frac,  DataSource::NASA_TAP);
    d.ice_coverage_fraction   = MeasuredValue<double>(ice_frac,    DataSource::NASA_TAP);

    d.atmosphere_composition  = MeasuredValue<std::string>(atmo_comp, DataSource::NASA_TAP);
    d.planet_type             = MeasuredValue<std::string>(planet_type, DataSource::NASA_TAP);

    // Sun as host star
    d.host_star.name          = "Sol";
    d.host_star.spectral_type = "G2V";
    d.host_star.effective_temp_k = MeasuredValue<double>(5778.0, DataSource::NASA_TAP);
    d.host_star.radius_solar     = MeasuredValue<double>(1.0,    DataSource::NASA_TAP);
    d.host_star.mass_solar       = MeasuredValue<double>(1.0,    DataSource::NASA_TAP);

    return d;
}

// ─── Solar system visual params ───────────────────────────────────────────────

static PlanetParams makeMercury() {
    PlanetParams p;
    p.radius           = 0.75f;
    p.rockColor        = {0.45f, 0.42f, 0.38f};
    p.sandColor        = {0.57f, 0.53f, 0.48f};
    p.treeColor        = {0.40f, 0.37f, 0.33f};
    p.iceColor         = {0.52f, 0.50f, 0.47f};
    p.waterLevel       = -0.5f;
    p.cloudsDensity    = 0.0f;
    p.atmosphereDensity = 0.01f;
    p.atmosphereColor  = {0.30f, 0.28f, 0.25f};
    p.craterStrength   = 0.85f;
    p.noiseStrength    = 0.30f;
    p.ridgedStrength   = 0.10f;
    p.polarCapSize     = 0.0f;
    p.sunIntensity     = 5.5f;
    p.sunColor         = {1.0f, 0.95f, 0.85f};
    p.fbmExponentiation = 6.0f;
    p.fbmPersistence   = 0.55f;
    return p;
}

static PlanetParams makeVenus() {
    PlanetParams p;
    p.radius            = 1.9f;
    p.cloudsDensity     = 1.0f;
    p.cloudsScale       = 2.5f;
    p.cloudsSpeed       = 0.8f;
    p.cloudAltitude     = 0.25f;
    p.cloudThickness    = 0.20f;
    p.cloudColor        = {0.92f, 0.80f, 0.52f};
    p.atmosphereColor   = {0.80f, 0.62f, 0.32f};
    p.atmosphereDensity = 0.95f;
    p.waterLevel        = -0.3f;
    p.craterStrength    = 0.08f;
    p.rockColor         = {0.50f, 0.32f, 0.15f};
    p.sandColor         = {0.68f, 0.50f, 0.25f};
    p.treeColor         = {0.44f, 0.28f, 0.12f};
    p.sunIntensity      = 3.8f;
    p.sunColor          = {1.0f, 0.90f, 0.70f};
    p.noiseStrength     = 0.15f;
    return p;
}

static PlanetParams makeEarth() {
    PlanetParams p;
    p.radius            = 2.0f;
    p.waterLevel        = 0.22f;
    p.waterColorDeep    = {0.01f, 0.05f, 0.15f};
    p.waterColorSurface = {0.02f, 0.12f, 0.27f};
    p.treeColor         = {0.02f, 0.10f, 0.04f};
    p.sandColor         = {0.85f, 0.75f, 0.50f};
    p.rockColor         = {0.25f, 0.22f, 0.18f};
    p.iceColor          = {0.88f, 0.93f, 0.98f};
    p.cloudsDensity     = 0.50f;
    p.cloudAltitude     = 0.12f;
    p.cloudThickness    = 0.08f;
    p.atmosphereColor   = {0.05f, 0.30f, 0.90f};
    p.atmosphereDensity = 0.30f;
    p.polarCapSize      = 0.15f;
    p.continentScale    = 0.60f;
    p.noiseStrength     = 0.20f;
    p.craterStrength    = 0.02f;
    p.sunIntensity      = 3.0f;
    p.sunColor          = {1.0f, 1.0f, 0.90f};
    p.fbmExponentiation = 5.0f;
    p.fbmPersistence    = 0.50f;
    return p;
}

static PlanetParams makeMars() {
    PlanetParams p;
    p.radius            = 1.0f;
    p.waterColorDeep    = {0.15f, 0.05f, 0.02f};
    p.waterColorSurface = {0.25f, 0.12f, 0.06f};
    p.sandColor         = {0.76f, 0.50f, 0.30f};
    p.treeColor         = {0.45f, 0.25f, 0.15f};
    p.rockColor         = {0.50f, 0.30f, 0.20f};
    p.iceColor          = {0.90f, 0.85f, 0.80f};
    p.atmosphereColor   = {0.80f, 0.40f, 0.20f};
    p.atmosphereDensity = 0.05f;
    p.cloudsDensity     = 0.05f;
    p.cloudColor        = {0.9f, 0.75f, 0.55f};
    p.noiseStrength     = 0.25f;
    p.sunIntensity      = 2.5f;
    p.sunColor          = {1.0f, 0.85f, 0.70f};
    p.waterLevel        = -0.1f;
    p.fbmExponentiation = 8.0f;
    p.fbmPersistence    = 0.60f;
    p.polarCapSize      = 0.30f;
    p.craterStrength    = 0.70f;
    p.ridgedStrength    = 0.20f;
    return p;
}

static PlanetParams makeJupiter() {
    PlanetParams p;
    p.radius             = 4.5f;
    p.bandingStrength    = 0.90f;
    p.bandingFrequency   = 26.0f;
    p.cloudsDensity      = 1.0f;
    p.cloudsScale        = 2.0f;
    p.cloudsSpeed        = 3.0f;
    p.cloudColor         = {0.90f, 0.85f, 0.70f};
    p.atmosphereColor    = {0.65f, 0.45f, 0.25f};
    p.atmosphereDensity  = 0.85f;
    p.waterColorDeep     = {0.55f, 0.35f, 0.15f};
    p.waterColorSurface  = {0.80f, 0.62f, 0.38f};
    p.sandColor          = {0.92f, 0.80f, 0.58f};
    p.treeColor          = {0.70f, 0.48f, 0.22f};
    p.rockColor          = {0.45f, 0.30f, 0.12f};
    p.noiseStrength      = 0.08f;
    p.terrainScale       = 0.5f;
    p.waterLevel         = 0.0f;
    p.domainWarpStrength = 0.25f;
    p.fbmExponentiation  = 2.0f;
    p.sunIntensity       = 2.0f;
    p.sunColor           = {1.0f, 0.95f, 0.85f};
    return p;
}

static PlanetParams makeSaturn() {
    PlanetParams p;
    p.radius             = 4.0f;
    p.bandingStrength    = 0.75f;
    p.bandingFrequency   = 20.0f;
    p.cloudsDensity      = 0.85f;
    p.cloudsScale        = 2.5f;
    p.cloudColor         = {0.95f, 0.90f, 0.78f};
    p.atmosphereColor    = {0.82f, 0.72f, 0.48f};
    p.atmosphereDensity  = 0.75f;
    p.waterColorDeep     = {0.65f, 0.55f, 0.32f};
    p.waterColorSurface  = {0.88f, 0.80f, 0.58f};
    p.sandColor          = {0.93f, 0.87f, 0.68f};
    p.treeColor          = {0.75f, 0.65f, 0.42f};
    p.rockColor          = {0.60f, 0.50f, 0.30f};
    p.noiseStrength      = 0.06f;
    p.terrainScale       = 0.5f;
    p.waterLevel         = 0.0f;
    p.fbmExponentiation  = 1.8f;
    p.sunIntensity       = 1.8f;
    p.sunColor           = {1.0f, 0.95f, 0.85f};
    return p;
}

static PlanetParams makeUranus() {
    PlanetParams p;
    p.radius             = 3.0f;
    p.atmosphereColor    = {0.40f, 0.82f, 0.85f};
    p.atmosphereDensity  = 0.80f;
    p.cloudsDensity      = 0.45f;
    p.cloudColor         = {0.60f, 0.88f, 0.90f};
    p.bandingStrength    = 0.15f;
    p.bandingFrequency   = 8.0f;
    p.waterColorDeep     = {0.30f, 0.70f, 0.75f};
    p.waterColorSurface  = {0.45f, 0.80f, 0.82f};
    p.noiseStrength      = 0.06f;
    p.terrainScale       = 0.4f;
    p.waterLevel         = 0.0f;
    p.polarCapSize       = 0.55f;
    p.sunIntensity       = 1.5f;
    p.fbmExponentiation  = 1.5f;
    return p;
}

static PlanetParams makeNeptune() {
    PlanetParams p;
    p.radius             = 2.8f;
    p.atmosphereColor    = {0.05f, 0.12f, 0.82f};
    p.atmosphereDensity  = 0.85f;
    p.cloudsDensity      = 0.55f;
    p.cloudColor         = {0.70f, 0.78f, 0.95f};
    p.bandingStrength    = 0.25f;
    p.bandingFrequency   = 12.0f;
    p.domainWarpStrength = 0.15f;
    p.waterColorDeep     = {0.02f, 0.05f, 0.60f};
    p.waterColorSurface  = {0.05f, 0.12f, 0.75f};
    p.noiseStrength      = 0.10f;
    p.terrainScale       = 0.4f;
    p.waterLevel         = 0.0f;
    p.polarCapSize       = 0.45f;
    p.sunIntensity       = 1.5f;
    p.fbmExponentiation  = 1.8f;
    return p;
}

// ─── SolarSystemDatabase ──────────────────────────────────────────────────────

SolarSystemDatabase::SolarSystemDatabase() {
    // name, mass_earth, radius_earth, temp_k, density_gcc, gravity_g,
    // pressure_atm, albedo, ocean_frac, cloud_frac, ice_frac, atmo_comp, type

    m_entries.push_back({
        "Mercury",
        makePlanetData("Mercury", 0.0553, 0.383, 440.0, 5.43, 0.38,
                       0.0, 0.068, 0.0, 0.0, 0.0,
                       R"({"traces":1.0})", "Rocky"),
        makeMercury(),
        "Gray heavily-cratered rocky world, no atmosphere, extreme temperature swings"
    });
    m_entries.push_back({
        "Venus",
        makePlanetData("Venus", 0.815, 0.949, 737.0, 5.24, 0.91,
                       92.0, 0.77, 0.0, 1.0, 0.0,
                       R"({"CO2":0.965,"N2":0.035})", "Rocky"),
        makeVenus(),
        "Completely cloud-covered in thick sulfuric acid haze, orange-yellow atmosphere, runaway greenhouse"
    });
    m_entries.push_back({
        "Earth",
        makePlanetData("Earth", 1.0, 1.0, 288.0, 5.51, 1.0,
                       1.0, 0.30, 0.71, 0.50, 0.03,
                       R"({"N2":0.78,"O2":0.21,"Ar":0.01})", "Rocky"),
        makeEarth(),
        "Blue ocean world with green continents, white clouds, thin blue atmosphere, polar ice caps"
    });
    m_entries.push_back({
        "Mars",
        makePlanetData("Mars", 0.107, 0.532, 210.0, 3.93, 0.38,
                       0.006, 0.25, 0.0, 0.05, 0.12,
                       R"({"CO2":0.953,"N2":0.027,"Ar":0.016})", "Rocky"),
        makeMars(),
        "Red-orange desert world, thin CO2 atmosphere, polar CO2 ice caps, heavy cratering"
    });
    m_entries.push_back({
        "Jupiter",
        makePlanetData("Jupiter", 317.8, 11.2, 165.0, 1.33, 2.53,
                       0.0, 0.52, 0.0, 0.90, 0.0,
                       R"({"H2":0.90,"He":0.10})", "Gas Giant"),
        makeJupiter(),
        "Massive gas giant with orange-tan cloud bands, Great Red Spot, high cloud density"
    });
    m_entries.push_back({
        "Saturn",
        makePlanetData("Saturn", 95.2, 9.45, 134.0, 0.69, 1.07,
                       0.0, 0.47, 0.0, 0.80, 0.0,
                       R"({"H2":0.96,"He":0.03})", "Gas Giant"),
        makeSaturn(),
        "Golden-beige banded gas giant, less prominent bands than Jupiter, cream-coloured cloud tops"
    });
    m_entries.push_back({
        "Uranus",
        makePlanetData("Uranus", 14.5, 4.01, 76.0, 1.27, 0.90,
                       0.0, 0.51, 0.0, 0.40, 0.0,
                       R"({"H2":0.83,"He":0.15,"CH4":0.02})", "Ice Giant"),
        makeUranus(),
        "Featureless cyan-teal ice giant, methane atmosphere, very subtle banding"
    });
    m_entries.push_back({
        "Neptune",
        makePlanetData("Neptune", 17.1, 3.88, 72.0, 1.64, 1.14,
                       0.0, 0.41, 0.0, 0.50, 0.0,
                       R"({"H2":0.80,"He":0.19,"CH4":0.015})", "Ice Giant"),
        makeNeptune(),
        "Deep-blue ice giant with dynamic cloud features, methane atmosphere, Great Dark Spot"
    });
}

const SolarSystemDatabase& SolarSystemDatabase::instance() {
    static SolarSystemDatabase db;
    return db;
}

// ─── Similarity metric ────────────────────────────────────────────────────────
// Log-normalised Euclidean distance in (mass, radius, temperature) space.

float SolarSystemDatabase::similarity(double mass_e, double radius_e, double temp_k,
                                       const SolarSystemEntry& entry) {
    auto safeLog = [](double v) { return v > 0.0 ? std::log10(v) : -4.0; };

    double em = entry.physicalData.mass_earth.value;
    double er = entry.physicalData.radius_earth.value;
    double et = entry.physicalData.equilibrium_temp_k.value;

    // Normalise: mass spans ~4 decades, radius ~1.5, temperature ~600K range
    double dm = (safeLog(mass_e) - safeLog(em)) / 4.0;
    double dr = (safeLog(radius_e) - safeLog(er)) / 1.5;
    double dt = (temp_k - et) / 600.0;

    double dist = std::sqrt(dm*dm + dr*dr + dt*dt);
    return static_cast<float>(std::exp(-dist * 2.0));
}

std::optional<AnalogMatch> SolarSystemDatabase::findClosestAnalog(
    double mass_earth, double radius_earth, double temp_k, float minScore) const {

    const SolarSystemEntry* best  = nullptr;
    float                   bestS = minScore;

    for (const auto& entry : m_entries) {
        float s = similarity(mass_earth, radius_earth, temp_k, entry);
        if (s > bestS) {
            bestS = s;
            best  = &entry;
        }
    }

    if (!best) return std::nullopt;
    return AnalogMatch{best, bestS};
}

const SolarSystemEntry* SolarSystemDatabase::findByName(const std::string& query) const {
    // Normalise query to lowercase
    std::string q = query;
    std::transform(q.begin(), q.end(), q.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    for (const auto& entry : m_entries) {
        std::string n = entry.name;
        std::transform(n.begin(), n.end(), n.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (n.find(q) != std::string::npos || q.find(n) != std::string::npos) {
            return &entry;
        }
    }
    return nullptr;
}

}  // namespace astrocore
