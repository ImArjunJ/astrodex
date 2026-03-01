#pragma once

#include "physics/PhysicsWorld.hpp"
#include "physics/CelestialBody.hpp"
#include "render/Renderer.hpp"
#include <glm/glm.hpp>
#include <unordered_map>
#include <functional>

namespace astrocore {

class Camera;
struct SystemConfig;

// Bridges physics simulation with rendering
// Handles coordinate scaling, body appearance, and camera focus
class Simulation {
public:
    Simulation();
    ~Simulation();

    // Initialize with a physics world
    void init();

    // Update simulation (call each frame)
    // realDeltaTime: actual time since last frame (seconds)
    void update(float realDeltaTime);

    // Render orbit trails only (detailed body rendering done separately)
    void renderOrbits(Renderer& renderer, const Camera& camera);

    // Physics world access
    PhysicsWorld& world() { return m_world; }
    const PhysicsWorld& world() const { return m_world; }

    // Time controls
    void pause() { m_paused = true; }
    void resume() { m_paused = false; }
    void togglePause() { m_paused = !m_paused; }
    bool isPaused() const { return m_paused; }

    void setTimeScale(double scale) { m_timeScale = scale; }
    double timeScale() const { return m_timeScale; }

    // Simulation time (in-simulation seconds)
    double simulationTime() const { return m_world.time(); }

    // Scale factor: render units per meter
    // Default: 1 render unit = 1e9 meters (1 million km)
    void setScaleFactor(double scale) { m_scaleFactor = scale; }
    double scaleFactor() const { return m_scaleFactor; }

    // Convert between physics and render coordinates
    glm::vec3 physicsToRender(const glm::dvec3& physPos) const;
    glm::dvec3 renderToPhysics(const glm::vec3& renderPos) const;

    // Camera focus
    void setFocusBody(uint64_t bodyId);
    void setFocusBody(const std::string& name);
    void clearFocus();
    CelestialBody* focusBody() const { return m_focusBody; }

    // Selection (for editing params)
    void setSelectedBody(uint64_t bodyId);
    void setSelectedBody(const std::string& name);
    void clearSelection();
    CelestialBody* selectedBody() const { return m_selectedBody; }

    // Pick a body at screen position (returns body ID or 0 if none)
    uint64_t pickBody(const glm::vec3& rayOrigin, const glm::vec3& rayDir) const;

    // Get render position of focus body (for camera targeting)
    glm::vec3 getFocusPosition() const;

    // Body appearance (maps body ID to PlanetParams)
    void setBodyAppearance(uint64_t bodyId, const PlanetParams& params);
    PlanetParams& getBodyAppearance(uint64_t bodyId);
    bool hasAppearance(uint64_t bodyId) const;

    // Ring parameters (maps body ID to RingParams)
    void setRingParams(uint64_t bodyId, const RingParams& params);
    RingParams* getRingParams(uint64_t bodyId);
    bool hasRing(uint64_t bodyId) const;
    const std::unordered_map<uint64_t, RingParams>& allRingParams() const { return m_ringParams; }

    // Preset simulations
    void loadSolarSystem();
    void loadBinaryStars();
    void loadRandomSystem(int numBodies);

    // Load from a SystemConfig
    void loadFromConfig(const SystemConfig& config);

    // Clear all simulation state (bodies, appearances, etc.)
    void clear();

    // Statistics
    struct Stats {
        int bodyCount = 0;
        double simulationTime = 0.0;
        double timeScale = 1.0;
        bool paused = false;
        std::string focusBodyName;
    };
    Stats getStats() const;

private:
    void setupDefaultAppearances();
    PlanetParams createStarAppearance();
    PlanetParams createEarthAppearance();
    PlanetParams createMarsAppearance();
    PlanetParams createGasGiantAppearance();
    PlanetParams createMoonAppearance();
    PlanetParams createGenericAppearance(BodyType type);

    // Load planet params from a JSON file (returns true if successful)
    bool loadPlanetPreset(const std::string& filepath, PlanetParams& params);

    PhysicsWorld m_world;

    // Time control
    bool m_paused = false;
    double m_timeScale = 1.0;  // 1.0 = real time, 86400 = 1 day per second

    // Scale: render_pos = physics_pos * m_scaleFactor
    // Default: show solar system in reasonable screen space
    double m_scaleFactor = 1.0 / 1.0e10;  // 1 render unit = 10 billion meters

    // Camera focus
    CelestialBody* m_focusBody = nullptr;
    glm::dvec3 m_focusOffset{0.0};  // Fixed offset from focus body

    // Selected body for editing
    CelestialBody* m_selectedBody = nullptr;

    // Body appearances
    std::unordered_map<uint64_t, PlanetParams> m_appearances;
    PlanetParams m_defaultAppearance;

    // Ring parameters
    std::unordered_map<uint64_t, RingParams> m_ringParams;

    // Flag to clear orbit trails on next render
    bool m_orbitTrailsNeedClear = false;
};

} // namespace astrocore
