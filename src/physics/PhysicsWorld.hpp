#pragma once

#include "physics/CelestialBody.hpp"
#include "physics/Octree.hpp"
#include "physics/Integrator.hpp"
#include <vector>
#include <memory>
#include <string>

namespace astrocore {

// Main container for N-body gravitational simulation
// Manages celestial bodies and their interactions
class PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld();

    // Move-only (contains unique_ptrs)
    PhysicsWorld(PhysicsWorld&&) noexcept = default;
    PhysicsWorld& operator=(PhysicsWorld&&) noexcept = default;
    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    // Simulation control
    void step(double dt);                    // Single timestep
    void stepAdaptive();                     // Use adaptive timestep
    void run(double duration, double dt);   // Run for duration

    // Body management
    CelestialBody& addBody(std::unique_ptr<CelestialBody> body);
    CelestialBody& addBody(const CelestialBody& body);
    void removeBody(uint64_t id);
    void clear();

    // Access bodies
    CelestialBody* getBody(uint64_t id);
    CelestialBody* getBodyByName(const std::string& name);
    const std::vector<std::unique_ptr<CelestialBody>>& bodies() const { return m_bodies; }
    size_t bodyCount() const { return m_bodies.size(); }

    // Time
    double time() const { return m_time; }
    void setTime(double t) { m_time = t; }

    // Integrator
    void setIntegrator(IntegratorType type);
    IntegratorType integratorType() const { return m_integratorType; }

    // Barnes-Hut parameter (0 = exact, 0.5 = good balance, 1.0 = fast)
    void setTheta(double theta) { m_theta = theta; }
    double theta() const { return m_theta; }

    // Gravitational constant (can be scaled for non-SI units)
    void setG(double G) { m_G = G; }
    double G() const { return m_G; }

    // Adaptive timestep settings
    void setAdaptiveTimestep(double minDt, double maxDt, double tolerance);
    double lastTimestep() const { return m_lastDt; }

    // Simulation statistics
    struct Stats {
        int octreeNodes = 0;
        int maxOctreeDepth = 0;
        double totalKineticEnergy = 0.0;
        double totalPotentialEnergy = 0.0;
        double totalEnergy = 0.0;
        glm::dvec3 centerOfMass{0.0};
        double totalMass = 0.0;
    };
    Stats computeStats() const;

    // Presets for common systems
    static PhysicsWorld createSolarSystem();
    static PhysicsWorld createBinarySystem(double mass1, double mass2, double separation);
    static PhysicsWorld createRandomSystem(int numBodies, double regionSize, double totalMass);

    // Orbital mechanics helpers
    static void setCircularOrbit(CelestialBody& body, const CelestialBody& parent, double G);
    static void setEllipticalOrbit(CelestialBody& body, const CelestialBody& parent,
                                   double semiMajorAxis, double eccentricity, double G);

private:
    std::vector<std::unique_ptr<CelestialBody>> m_bodies;
    Octree m_octree;
    std::unique_ptr<Integrator> m_integrator;
    IntegratorType m_integratorType = IntegratorType::Verlet;
    AdaptiveTimestep m_adaptiveTimestep;

    double m_time = 0.0;
    double m_lastDt = 0.0;
    double m_G = constants::G;
    double m_theta = Octree::DEFAULT_THETA;
};

} // namespace astrocore
