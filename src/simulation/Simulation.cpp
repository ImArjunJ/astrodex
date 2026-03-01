#include "simulation/Simulation.hpp"
#include "config/SystemConfig.hpp"
#include "config/PresetManager.hpp"
#include "render/Camera.hpp"
#include "core/Logger.hpp"
#include <limits>
#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>

namespace astrocore {

Simulation::Simulation() {
    m_defaultAppearance = PlanetParams{};
}

Simulation::~Simulation() = default;

void Simulation::init() {
    // Set up default integrator
    m_world.setIntegrator(IntegratorType::Verlet);

    // Default time scale: 1 day per second for interesting orbital motion
    m_timeScale = constants::DAY;
}

void Simulation::update(float realDeltaTime) {
    if (m_paused || realDeltaTime <= 0) return;

    // Convert real time to simulation time
    double simDt = realDeltaTime * m_timeScale;

    // Cap maximum step for numerical stability
    // Moon's orbit requires smaller steps (~1 hour max for good accuracy)
    double maxStep = 3600.0;  // 1 hour max per step
    while (simDt > 0) {
        double step = std::min(simDt, maxStep);
        m_world.step(step);
        simDt -= step;
    }
}

void Simulation::renderOrbits(Renderer& renderer, const Camera& camera) {
    // Clear trails if requested (e.g., after loading new simulation)
    if (m_orbitTrailsNeedClear) {
        renderer.orbitRenderer().clear();
        m_orbitTrailsNeedClear = false;
    }

    // Get focus position for rendering
    glm::dvec3 focusPos = m_focusBody ? m_focusBody->position() : glm::dvec3(0.0);

    // Record orbit trail positions in PHYSICS coordinates
    for (const auto& body : m_world.bodies()) {
        if (body->isEmissive()) continue;  // Skip sun/stars

        // Get body color for trail
        glm::vec3 color(0.5f, 0.5f, 0.5f);
        if (hasAppearance(body->id())) {
            color = m_appearances.at(body->id()).sandColor;
        }

        // Record physics position - converted to render coords at draw time
        renderer.orbitRenderer().recordPosition(body->id(), body->position(), color * 0.7f);
    }

    // Render orbit trails - pass focus and scale for coordinate conversion
    renderer.orbitRenderer().render(camera.getViewProjectionMatrix(), focusPos, m_scaleFactor);
}

glm::vec3 Simulation::physicsToRender(const glm::dvec3& physPos) const {
    return glm::vec3(physPos * m_scaleFactor);
}

glm::dvec3 Simulation::renderToPhysics(const glm::vec3& renderPos) const {
    return glm::dvec3(renderPos) / m_scaleFactor;
}

void Simulation::setFocusBody(uint64_t bodyId) {
    m_focusBody = m_world.getBody(bodyId);
}

void Simulation::setFocusBody(const std::string& name) {
    m_focusBody = m_world.getBodyByName(name);
}

void Simulation::clearFocus() {
    m_focusBody = nullptr;
}

void Simulation::setSelectedBody(uint64_t bodyId) {
    m_selectedBody = m_world.getBody(bodyId);
}

void Simulation::setSelectedBody(const std::string& name) {
    m_selectedBody = m_world.getBodyByName(name);
}

void Simulation::clearSelection() {
    m_selectedBody = nullptr;
}

uint64_t Simulation::pickBody(const glm::vec3& rayOrigin, const glm::vec3& rayDir) const {
    glm::dvec3 focusPos = m_focusBody ? m_focusBody->position() : glm::dvec3(0.0);

    float closestDist = std::numeric_limits<float>::max();
    uint64_t closestId = 0;

    for (const auto& body : m_world.bodies()) {
        glm::dvec3 relativePos = body->position() - focusPos;
        glm::vec3 bodyPos = physicsToRender(relativePos);
        float radius = body->renderRadius();

        // Ray-sphere intersection
        glm::vec3 oc = rayOrigin - bodyPos;
        float a = glm::dot(rayDir, rayDir);
        float b = 2.0f * glm::dot(oc, rayDir);
        float c = glm::dot(oc, oc) - radius * radius;
        float discriminant = b * b - 4 * a * c;

        if (discriminant >= 0) {
            float t = (-b - std::sqrt(discriminant)) / (2.0f * a);
            if (t > 0 && t < closestDist) {
                closestDist = t;
                closestId = body->id();
            }
        }
    }

    return closestId;
}

glm::vec3 Simulation::getFocusPosition() const {
    if (!m_focusBody) return glm::vec3(0.0f);
    return physicsToRender(m_focusBody->position());
}

void Simulation::setBodyAppearance(uint64_t bodyId, const PlanetParams& params) {
    m_appearances[bodyId] = params;
}

PlanetParams& Simulation::getBodyAppearance(uint64_t bodyId) {
    if (m_appearances.find(bodyId) == m_appearances.end()) {
        m_appearances[bodyId] = m_defaultAppearance;
    }
    return m_appearances[bodyId];
}

bool Simulation::hasAppearance(uint64_t bodyId) const {
    return m_appearances.find(bodyId) != m_appearances.end();
}

void Simulation::setRingParams(uint64_t bodyId, const RingParams& params) {
    m_ringParams[bodyId] = params;
}

RingParams* Simulation::getRingParams(uint64_t bodyId) {
    auto it = m_ringParams.find(bodyId);
    return (it != m_ringParams.end()) ? &it->second : nullptr;
}

bool Simulation::hasRing(uint64_t bodyId) const {
    auto it = m_ringParams.find(bodyId);
    return it != m_ringParams.end() && it->second.enabled;
}

void Simulation::loadSolarSystem() {
    m_world = PhysicsWorld::createSolarSystem();

    // Scale: 1 AU = 50000 render units (1000x larger for visibility)
    // Set this BEFORE setupDefaultAppearances so render radii use same scale
    m_scaleFactor = 50000.0 / constants::AU;

    setupDefaultAppearances();

    // Focus on Sun by default for overview
    setFocusBody("Sun");

    // Time scale: 1 day per second is a good default to see orbital motion
    m_timeScale = constants::DAY;

    // Clear any old orbit trails
    m_orbitTrailsNeedClear = true;
}

void Simulation::loadBinaryStars() {
    m_world = PhysicsWorld::createBinarySystem(
        constants::SOLAR_MASS,
        0.8 * constants::SOLAR_MASS,
        0.5 * constants::AU
    );
    setupDefaultAppearances();
    m_scaleFactor = 1.0 / constants::AU * 20.0;
}

void Simulation::loadRandomSystem(int numBodies) {
    double regionSize = 10.0 * constants::AU;
    double totalMass = 100.0 * constants::EARTH_MASS;
    m_world = PhysicsWorld::createRandomSystem(numBodies, regionSize, totalMass);
    setupDefaultAppearances();
    m_scaleFactor = 1.0 / constants::AU * 5.0;
}

void Simulation::loadFromConfig(const SystemConfig& config) {
    // Clear current state
    m_appearances.clear();
    m_ringParams.clear();
    m_focusBody = nullptr;
    m_selectedBody = nullptr;

    // Apply config to physics world
    PresetManager pm;
    pm.applyConfigToWorld(config, m_world);

    // Set scale factor from config
    m_scaleFactor = config.visualScale;

    // Setup appearances for all bodies
    setupDefaultAppearances();

    // Focus on first emissive body (star) or first body
    for (const auto& body : m_world.bodies()) {
        if (body->isEmissive()) {
            setFocusBody(body->id());
            break;
        }
    }
    if (!m_focusBody && !m_world.bodies().empty()) {
        setFocusBody(m_world.bodies()[0]->id());
    }

    // Time scale: 1 day per second for orbital motion
    m_timeScale = config.defaultTimestep;

    // Clear orbit trails
    m_orbitTrailsNeedClear = true;

    LOG_INFO("Loaded system '{}' with {} bodies", config.name, m_world.bodyCount());
}

void Simulation::clear() {
    // Clear physics world
    m_world = PhysicsWorld();
    m_world.setIntegrator(IntegratorType::Verlet);

    // Clear visual data
    m_appearances.clear();
    m_ringParams.clear();

    // Clear focus/selection
    m_focusBody = nullptr;
    m_selectedBody = nullptr;

    // Flag to clear orbit trails on next render
    m_orbitTrailsNeedClear = true;

    LOG_INFO("Simulation cleared");
}

Simulation::Stats Simulation::getStats() const {
    Stats stats;
    stats.bodyCount = static_cast<int>(m_world.bodyCount());
    stats.simulationTime = m_world.time();
    stats.timeScale = m_timeScale;
    stats.paused = m_paused;
    stats.focusBodyName = m_focusBody ? m_focusBody->name() : "None";
    return stats;
}

void Simulation::setupDefaultAppearances() {
    // Use REALISTIC proportions - same scale factor for positions and sizes
    // Everything is scaled uniformly so proportions are accurate

    for (const auto& body : m_world.bodies()) {
        // Calculate render radius using same scale as positions
        float realisticRadius = static_cast<float>(body->radius() * m_scaleFactor);
        PlanetParams params = createGenericAppearance(body->type());

        // Special handling for known bodies - all use realistic radius
        if (body->name() == "Sun") {
            params = createStarAppearance();
            body->setEmissive(true);
            body->setEmissiveColor({1.0f, 0.95f, 0.8f});
        } else if (body->name() == "Mercury") {
            // Load custom preset or use defaults
            if (!loadPlanetPreset("/home/junie/mercurylike", params)) {
                // Gray, heavily cratered, no atmosphere
                params.sandColor = {0.45f, 0.42f, 0.40f};
                params.treeColor = {0.35f, 0.33f, 0.30f};
                params.rockColor = {0.25f, 0.24f, 0.22f};
                params.iceColor = {0.55f, 0.53f, 0.50f};
                params.noiseStrength = 0.025f;
                params.craterStrength = 0.8f;
                params.atmosphereDensity = 0.0f;
                params.cloudsDensity = 0.0f;
                params.waterLevel = -0.5f;
                params.fbmExponentiation = 3.0f;
            }
        } else if (body->name() == "Venus") {
            // Load custom preset or use defaults
            if (!loadPlanetPreset("/home/junie/venuslike", params)) {
                // Thick sulfuric clouds hiding surface
                params.sandColor = {0.9f, 0.7f, 0.4f};
                params.treeColor = {0.85f, 0.6f, 0.3f};
                params.rockColor = {0.7f, 0.5f, 0.25f};
                params.iceColor = {0.95f, 0.85f, 0.6f};
                params.atmosphereColor = {0.95f, 0.75f, 0.3f};
                params.atmosphereDensity = 0.7f;
                params.cloudsDensity = 0.95f;
                params.cloudsScale = 1.5f;
                params.cloudColor = {0.95f, 0.85f, 0.5f};
                params.noiseStrength = 0.015f;
                params.waterLevel = -0.5f;
            }
        } else if (body->name() == "Earth") {
            // Load custom preset or use defaults
            if (!loadPlanetPreset("/home/junie/earthlike", params)) {
                // Blue marble - oceans, continents, clouds
                params.waterColorDeep = {0.01f, 0.03f, 0.12f};
                params.waterColorSurface = {0.02f, 0.08f, 0.25f};
                params.sandColor = {0.76f, 0.70f, 0.50f};  // Beaches/deserts
                params.treeColor = {0.13f, 0.35f, 0.13f};  // Forests
                params.rockColor = {0.4f, 0.35f, 0.3f};    // Mountains
                params.iceColor = {0.9f, 0.93f, 0.97f};    // Snow
                params.atmosphereColor = {0.4f, 0.65f, 1.0f};
                params.atmosphereDensity = 0.35f;
                params.cloudsDensity = 0.5f;
                params.cloudColor = {1.0f, 1.0f, 1.0f};
                params.noiseStrength = 0.025f;
                params.continentScale = 1.2f;
                params.waterLevel = 0.02f;
                params.polarCapSize = 0.12f;
                params.fbmExponentiation = 4.0f;
            }
        } else if (body->name() == "Mars") {
            // Load custom preset or use defaults
            if (!loadPlanetPreset("/home/junie/marslike", params)) {
                // Rusty red, thin atmosphere, polar caps
                params.sandColor = {0.72f, 0.35f, 0.18f};
                params.treeColor = {0.55f, 0.25f, 0.12f};
                params.rockColor = {0.4f, 0.2f, 0.1f};
                params.iceColor = {0.85f, 0.8f, 0.75f};  // Dusty ice
                params.atmosphereColor = {0.8f, 0.5f, 0.3f};
                params.atmosphereDensity = 0.08f;
                params.cloudsDensity = 0.03f;
                params.noiseStrength = 0.03f;
                params.craterStrength = 0.25f;
                params.polarCapSize = 0.18f;
                params.waterLevel = -0.3f;
                params.fbmExponentiation = 5.0f;
                params.ridgedStrength = 0.2f;  // Olympus Mons style
            }
        } else if (body->name() == "Jupiter") {
            // Load custom preset or use defaults
            if (!loadPlanetPreset("/home/junie/jupiterlike", params)) {
                // Banded gas giant with swirling storms
                params.sandColor = {0.85f, 0.75f, 0.6f};
                params.treeColor = {0.7f, 0.5f, 0.35f};
                params.rockColor = {0.55f, 0.4f, 0.3f};
                params.iceColor = {0.95f, 0.9f, 0.8f};
                params.atmosphereColor = {0.6f, 0.4f, 0.25f};
                params.atmosphereDensity = 0.5f;
                params.cloudsDensity = 0.9f;
                params.cloudsScale = 2.5f;
                params.cloudColor = {0.9f, 0.85f, 0.7f};
                params.noiseStrength = 0.008f;
                params.bandingStrength = 0.85f;
                params.bandingFrequency = 30.0f;
                params.domainWarpStrength = 0.4f;
                params.waterLevel = -1.0f;
            }
        } else if (body->name() == "Saturn") {
            // Load custom preset or use defaults
            if (!loadPlanetPreset("/home/junie/saturnlike", params)) {
                // Pale yellow banded gas giant with iconic rings
                params.sandColor = {0.9f, 0.85f, 0.65f};
                params.treeColor = {0.8f, 0.7f, 0.5f};
                params.rockColor = {0.7f, 0.6f, 0.45f};
                params.iceColor = {0.95f, 0.92f, 0.8f};
                params.atmosphereColor = {0.8f, 0.7f, 0.4f};
                params.atmosphereDensity = 0.45f;
                params.cloudsDensity = 0.8f;
                params.cloudsScale = 2.0f;
                params.cloudColor = {0.95f, 0.9f, 0.75f};
                params.noiseStrength = 0.006f;
                params.bandingStrength = 0.7f;
                params.bandingFrequency = 25.0f;
                params.domainWarpStrength = 0.3f;
                params.waterLevel = -1.0f;
            }

            // Saturn's rings - multiple bands like real Saturn
            RingParams rings;
            rings.enabled = true;
            rings.totalParticles = 60000;
            rings.thickness = 0.01f;

            // D Ring (innermost, faint)
            rings.addBand(1.11f, 1.24f,
                          {0.5f, 0.48f, 0.42f}, {0.45f, 0.43f, 0.38f}, 0.3f);

            // C Ring (inner, moderately bright)
            rings.addBand(1.24f, 1.53f,
                          {0.7f, 0.65f, 0.55f}, {0.65f, 0.6f, 0.5f}, 0.6f);

            // B Ring (brightest and densest)
            rings.addBand(1.53f, 1.95f,
                          {0.95f, 0.9f, 0.75f}, {0.85f, 0.8f, 0.65f}, 0.9f);

            // Cassini Division (gap - no band here)

            // A Ring (outer, bright)
            rings.addBand(2.03f, 2.27f,
                          {0.85f, 0.8f, 0.65f}, {0.7f, 0.65f, 0.55f}, 0.8f);

            // F Ring (narrow outer ring)
            rings.addBand(2.32f, 2.35f,
                          {0.6f, 0.55f, 0.45f}, {0.55f, 0.5f, 0.4f}, 0.5f);

            setRingParams(body->id(), rings);
        } else if (body->name() == "Uranus") {
            // Load custom preset or use defaults
            if (!loadPlanetPreset("/home/junie/uranuslike", params)) {
                // Pale cyan, relatively featureless ice giant
                params.sandColor = {0.6f, 0.8f, 0.85f};
                params.treeColor = {0.55f, 0.75f, 0.8f};
                params.rockColor = {0.5f, 0.7f, 0.75f};
                params.iceColor = {0.7f, 0.88f, 0.92f};
                params.atmosphereColor = {0.5f, 0.8f, 0.9f};
                params.atmosphereDensity = 0.4f;
                params.cloudsDensity = 0.3f;
                params.cloudColor = {0.7f, 0.85f, 0.9f};
                params.noiseStrength = 0.003f;
                params.bandingStrength = 0.15f;
                params.bandingFrequency = 15.0f;
                params.waterLevel = -1.0f;
            }
        } else if (body->name() == "Neptune") {
            // Load custom preset or use defaults
            if (!loadPlanetPreset("/home/junie/neptunelike", params)) {
                // Deep blue ice giant with visible bands
                params.sandColor = {0.25f, 0.4f, 0.8f};
                params.treeColor = {0.2f, 0.35f, 0.7f};
                params.rockColor = {0.15f, 0.3f, 0.6f};
                params.iceColor = {0.4f, 0.55f, 0.9f};
                params.atmosphereColor = {0.2f, 0.4f, 0.85f};
                params.atmosphereDensity = 0.5f;
                params.cloudsDensity = 0.5f;
                params.cloudsScale = 1.8f;
                params.cloudColor = {0.5f, 0.65f, 0.9f};
                params.noiseStrength = 0.005f;
                params.bandingStrength = 0.4f;
                params.bandingFrequency = 18.0f;
                params.domainWarpStrength = 0.25f;
                params.waterLevel = -1.0f;
            }
        } else if (body->name() == "Moon") {
            // Load custom preset or use defaults
            if (!loadPlanetPreset("/home/junie/moonlike", params)) {
                // Gray, heavily cratered, airless
                params.sandColor = {0.65f, 0.63f, 0.6f};
                params.treeColor = {0.5f, 0.48f, 0.45f};
                params.rockColor = {0.35f, 0.33f, 0.3f};
                params.iceColor = {0.75f, 0.73f, 0.7f};
                params.noiseStrength = 0.02f;
                params.craterStrength = 0.7f;
                params.atmosphereDensity = 0.0f;
                params.cloudsDensity = 0.0f;
                params.waterLevel = -0.5f;
                params.fbmExponentiation = 2.5f;
            }
        } else if (body->type() == BodyType::Star) {
            params = createStarAppearance();
            body->setEmissive(true);
        }
        // else: use generic appearance from createGenericAppearance

        // ALL bodies use realistic render radius from physical radius
        body->setRenderRadius(realisticRadius);

        // Set the radius in params to match render radius
        params.radius = body->renderRadius();
        setBodyAppearance(body->id(), params);
    }
}

PlanetParams Simulation::createStarAppearance() {
    PlanetParams p{};
    p.radius = 3.0f;
    p.noiseStrength = 0.01f;
    p.fbmExponentiation = 1.0f;
    p.sandColor = {1.0f, 0.95f, 0.7f};
    p.treeColor = {1.0f, 0.9f, 0.6f};
    p.rockColor = {1.0f, 0.85f, 0.5f};
    p.iceColor = {1.0f, 1.0f, 0.9f};
    p.atmosphereColor = {1.0f, 0.8f, 0.4f};
    p.atmosphereDensity = 0.8f;
    p.cloudsDensity = 0.0f;
    p.sunIntensity = 0.0f;
    p.ambientLight = 1.0f;
    return p;
}

PlanetParams Simulation::createEarthAppearance() {
    PlanetParams p{};
    p.radius = 2.0f;
    p.noiseStrength = 0.02f;
    p.continentScale = 1.0f;
    p.waterLevel = 0.0f;
    p.polarCapSize = 0.15f;
    p.atmosphereColor = {0.3f, 0.5f, 0.9f};
    p.atmosphereDensity = 0.3f;
    p.cloudsDensity = 0.5f;
    return p;
}

PlanetParams Simulation::createMarsAppearance() {
    PlanetParams p{};
    p.radius = 1.5f;
    p.noiseStrength = 0.03f;
    p.sandColor = {0.76f, 0.50f, 0.30f};
    p.treeColor = {0.60f, 0.35f, 0.20f};
    p.rockColor = {0.50f, 0.30f, 0.20f};
    p.iceColor = {0.90f, 0.85f, 0.80f};
    p.atmosphereColor = {0.8f, 0.4f, 0.2f};
    p.atmosphereDensity = 0.05f;
    p.cloudsDensity = 0.02f;
    p.polarCapSize = 0.25f;
    p.craterStrength = 0.3f;
    return p;
}

PlanetParams Simulation::createGasGiantAppearance() {
    PlanetParams p{};
    p.radius = 4.0f;
    p.noiseStrength = 0.01f;
    p.bandingStrength = 0.8f;
    p.bandingFrequency = 25.0f;
    p.sandColor = {0.90f, 0.85f, 0.70f};
    p.treeColor = {0.70f, 0.55f, 0.40f};
    p.rockColor = {0.50f, 0.35f, 0.25f};
    p.iceColor = {0.85f, 0.60f, 0.45f};
    p.atmosphereColor = {0.6f, 0.5f, 0.3f};
    p.atmosphereDensity = 0.6f;
    p.cloudsDensity = 0.8f;
    p.domainWarpStrength = 0.3f;
    return p;
}

PlanetParams Simulation::createMoonAppearance() {
    PlanetParams p{};
    p.radius = 0.8f;
    p.noiseStrength = 0.015f;
    p.sandColor = {0.6f, 0.6f, 0.6f};
    p.treeColor = {0.5f, 0.5f, 0.5f};
    p.rockColor = {0.4f, 0.4f, 0.4f};
    p.iceColor = {0.7f, 0.7f, 0.7f};
    p.atmosphereDensity = 0.0f;
    p.cloudsDensity = 0.0f;
    p.craterStrength = 0.6f;
    return p;
}

PlanetParams Simulation::createGenericAppearance(BodyType type) {
    switch (type) {
        case BodyType::Star:
        case BodyType::NeutronStar:
            return createStarAppearance();
        case BodyType::Planet:
            return createEarthAppearance();
        case BodyType::Moon:
        case BodyType::Asteroid:
            return createMoonAppearance();
        case BodyType::DwarfPlanet:
        case BodyType::Comet:
            return createMoonAppearance();
        default:
            return m_defaultAppearance;
    }
}

bool Simulation::loadPlanetPreset(const std::string& filepath, PlanetParams& params) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    try {
        nlohmann::json j;
        file >> j;

        // Geometry
        if (j.contains("radius")) params.radius = j["radius"];
        if (j.contains("rotationSpeed")) params.rotationSpeed = j["rotationSpeed"];
        if (j.contains("rotationOffset")) params.rotationOffset = j["rotationOffset"];
        if (j.contains("quality")) params.quality = j["quality"];

        // Noise
        if (j.contains("noiseType")) params.noiseType = static_cast<NoiseType>(j["noiseType"].get<int>());
        if (j.contains("noiseStrength")) params.noiseStrength = j["noiseStrength"];
        if (j.contains("terrainScale")) params.terrainScale = j["terrainScale"];
        if (j.contains("fbmOctaves")) params.fbmOctaves = j["fbmOctaves"];
        if (j.contains("fbmPersistence")) params.fbmPersistence = j["fbmPersistence"];
        if (j.contains("fbmLacunarity")) params.fbmLacunarity = j["fbmLacunarity"];
        if (j.contains("fbmExponentiation")) params.fbmExponentiation = j["fbmExponentiation"];
        if (j.contains("domainWarpStrength")) params.domainWarpStrength = j["domainWarpStrength"];

        // Terrain features
        if (j.contains("ridgedStrength")) params.ridgedStrength = j["ridgedStrength"];
        if (j.contains("craterStrength")) params.craterStrength = j["craterStrength"];
        if (j.contains("continentScale")) params.continentScale = j["continentScale"];
        if (j.contains("continentBlend")) params.continentBlend = j["continentBlend"];
        if (j.contains("waterLevel")) params.waterLevel = j["waterLevel"];

        // Latitude
        if (j.contains("polarCapSize")) params.polarCapSize = j["polarCapSize"];
        if (j.contains("bandingStrength")) params.bandingStrength = j["bandingStrength"];
        if (j.contains("bandingFrequency")) params.bandingFrequency = j["bandingFrequency"];

        // Colors (arrays of 3)
        auto loadVec3 = [&](const char* key, glm::vec3& v) {
            if (j.contains(key) && j[key].is_array() && j[key].size() >= 3) {
                v.x = j[key][0];
                v.y = j[key][1];
                v.z = j[key][2];
            }
        };
        loadVec3("waterColorDeep", params.waterColorDeep);
        loadVec3("waterColorSurface", params.waterColorSurface);
        loadVec3("sandColor", params.sandColor);
        loadVec3("treeColor", params.treeColor);
        loadVec3("rockColor", params.rockColor);
        loadVec3("iceColor", params.iceColor);
        loadVec3("cloudColor", params.cloudColor);
        loadVec3("atmosphereColor", params.atmosphereColor);
        loadVec3("sunDirection", params.sunDirection);
        loadVec3("sunColor", params.sunColor);
        loadVec3("deepSpaceColor", params.deepSpaceColor);

        // Biome levels
        if (j.contains("sandLevel")) params.sandLevel = j["sandLevel"];
        if (j.contains("treeLevel")) params.treeLevel = j["treeLevel"];
        if (j.contains("rockLevel")) params.rockLevel = j["rockLevel"];
        if (j.contains("iceLevel")) params.iceLevel = j["iceLevel"];
        if (j.contains("transition")) params.transition = j["transition"];

        // Clouds
        if (j.contains("cloudsDensity")) params.cloudsDensity = j["cloudsDensity"];
        if (j.contains("cloudsScale")) params.cloudsScale = j["cloudsScale"];
        if (j.contains("cloudsSpeed")) params.cloudsSpeed = j["cloudsSpeed"];
        if (j.contains("cloudAltitude")) params.cloudAltitude = j["cloudAltitude"];
        if (j.contains("cloudThickness")) params.cloudThickness = j["cloudThickness"];

        // Atmosphere
        if (j.contains("atmosphereDensity")) params.atmosphereDensity = j["atmosphereDensity"];

        // Lighting
        if (j.contains("sunIntensity")) params.sunIntensity = j["sunIntensity"];
        if (j.contains("ambientLight")) params.ambientLight = j["ambientLight"];

        LOG_INFO("Loaded planet preset from {}", filepath);
        return true;

    } catch (const std::exception& e) {
        LOG_WARN("Failed to parse planet preset {}: {}", filepath, e.what());
        return false;
    }
}

} // namespace astrocore
