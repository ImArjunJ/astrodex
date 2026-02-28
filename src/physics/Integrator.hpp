#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <functional>

namespace astrocore {

class CelestialBody;
class Octree;

// Numerical integration methods for N-body simulation
// Each has different accuracy/performance tradeoffs

enum class IntegratorType {
    Euler,              // Simple but inaccurate (1st order)
    Verlet,             // Velocity Verlet - good for orbital mechanics (2nd order)
    Leapfrog,           // Symplectic - conserves energy well
    RK4,                // Runge-Kutta 4th order - very accurate but slower
    Hermite             // 4th order predictor-corrector - best for close encounters
};

class Integrator {
public:
    virtual ~Integrator() = default;

    // Integrate one timestep
    // bodies: all bodies in simulation
    // octree: Barnes-Hut tree (rebuilt each step)
    // G: gravitational constant
    // dt: timestep in seconds
    virtual void step(std::vector<std::unique_ptr<CelestialBody>>& bodies,
                      Octree& octree, double G, double dt) = 0;

    // Factory method
    static std::unique_ptr<Integrator> create(IntegratorType type);
};

// Simple Euler integration (for reference/testing)
class EulerIntegrator : public Integrator {
public:
    void step(std::vector<std::unique_ptr<CelestialBody>>& bodies,
              Octree& octree, double G, double dt) override;
};

// Velocity Verlet - symplectic, 2nd order, good energy conservation
// Most common choice for orbital mechanics
class VerletIntegrator : public Integrator {
public:
    void step(std::vector<std::unique_ptr<CelestialBody>>& bodies,
              Octree& octree, double G, double dt) override;
};

// Leapfrog (Kick-Drift-Kick) - symplectic, good for long simulations
class LeapfrogIntegrator : public Integrator {
public:
    void step(std::vector<std::unique_ptr<CelestialBody>>& bodies,
              Octree& octree, double G, double dt) override;
};

// Runge-Kutta 4th order - high accuracy, not symplectic
class RK4Integrator : public Integrator {
public:
    void step(std::vector<std::unique_ptr<CelestialBody>>& bodies,
              Octree& octree, double G, double dt) override;

private:
    // Temporary storage for RK4 stages
    struct State {
        glm::dvec3 position;
        glm::dvec3 velocity;
    };

    std::vector<State> m_k1, m_k2, m_k3, m_k4;
    std::vector<State> m_tempState;
};

// Adaptive timestep controller
class AdaptiveTimestep {
public:
    AdaptiveTimestep(double minDt = 1.0, double maxDt = 86400.0, double tolerance = 1e-6);

    // Calculate appropriate timestep based on system state
    double calculateTimestep(const std::vector<std::unique_ptr<CelestialBody>>& bodies,
                            double G) const;

    void setMinDt(double dt) { m_minDt = dt; }
    void setMaxDt(double dt) { m_maxDt = dt; }
    void setTolerance(double tol) { m_tolerance = tol; }

private:
    double m_minDt;
    double m_maxDt;
    double m_tolerance;
};

} // namespace astrocore
