#include "physics/Integrator.hpp"
#include "physics/CelestialBody.hpp"
#include "physics/Octree.hpp"
#include <cmath>
#include <algorithm>

namespace astrocore {

std::unique_ptr<Integrator> Integrator::create(IntegratorType type) {
    switch (type) {
        case IntegratorType::Euler:
            return std::make_unique<EulerIntegrator>();
        case IntegratorType::Verlet:
            return std::make_unique<VerletIntegrator>();
        case IntegratorType::Leapfrog:
            return std::make_unique<LeapfrogIntegrator>();
        case IntegratorType::RK4:
            return std::make_unique<RK4Integrator>();
        default:
            return std::make_unique<VerletIntegrator>();
    }
}

// Helper: compute accelerations for all bodies using Barnes-Hut
static void computeAccelerations(std::vector<std::unique_ptr<CelestialBody>>& bodies,
                                  Octree& octree, double G) {
    // Rebuild octree with current positions
    octree.build(bodies);

    // Compute forces using Barnes-Hut approximation
    for (auto& body : bodies) {
        body->clearAcceleration();
        glm::dvec3 force = octree.calculateForce(*body, G);
        body->applyForce(force);
    }
}

// Euler Integration: x(t+dt) = x(t) + v(t)*dt
//                    v(t+dt) = v(t) + a(t)*dt
void EulerIntegrator::step(std::vector<std::unique_ptr<CelestialBody>>& bodies,
                           Octree& octree, double G, double dt) {
    computeAccelerations(bodies, octree, G);

    for (auto& body : bodies) {
        glm::dvec3 pos = body->position();
        glm::dvec3 vel = body->velocity();
        glm::dvec3 acc = body->acceleration();

        body->setPosition(pos + vel * dt);
        body->setVelocity(vel + acc * dt);
    }
}

// Velocity Verlet: x(t+dt) = x(t) + v(t)*dt + 0.5*a(t)*dt^2
//                  v(t+dt) = v(t) + 0.5*(a(t) + a(t+dt))*dt
void VerletIntegrator::step(std::vector<std::unique_ptr<CelestialBody>>& bodies,
                            Octree& octree, double G, double dt) {
    // Compute current accelerations
    computeAccelerations(bodies, octree, G);

    // Store old accelerations and update positions
    std::vector<glm::dvec3> oldAccel(bodies.size());
    for (size_t i = 0; i < bodies.size(); i++) {
        oldAccel[i] = bodies[i]->acceleration();

        glm::dvec3 pos = bodies[i]->position();
        glm::dvec3 vel = bodies[i]->velocity();

        // Position update with acceleration term
        bodies[i]->setPosition(pos + vel * dt + 0.5 * oldAccel[i] * dt * dt);
    }

    // Compute new accelerations at new positions
    computeAccelerations(bodies, octree, G);

    // Update velocities using average of old and new acceleration
    for (size_t i = 0; i < bodies.size(); i++) {
        glm::dvec3 vel = bodies[i]->velocity();
        glm::dvec3 newAccel = bodies[i]->acceleration();

        bodies[i]->setVelocity(vel + 0.5 * (oldAccel[i] + newAccel) * dt);
    }
}

// Leapfrog (Kick-Drift-Kick variant)
void LeapfrogIntegrator::step(std::vector<std::unique_ptr<CelestialBody>>& bodies,
                              Octree& octree, double G, double dt) {
    // Half-step velocity update (kick)
    computeAccelerations(bodies, octree, G);
    for (auto& body : bodies) {
        glm::dvec3 vel = body->velocity();
        body->setVelocity(vel + 0.5 * body->acceleration() * dt);
    }

    // Full-step position update (drift)
    for (auto& body : bodies) {
        glm::dvec3 pos = body->position();
        body->setPosition(pos + body->velocity() * dt);
    }

    // Half-step velocity update (kick)
    computeAccelerations(bodies, octree, G);
    for (auto& body : bodies) {
        glm::dvec3 vel = body->velocity();
        body->setVelocity(vel + 0.5 * body->acceleration() * dt);
    }
}

// Runge-Kutta 4th Order
void RK4Integrator::step(std::vector<std::unique_ptr<CelestialBody>>& bodies,
                         Octree& octree, double G, double dt) {
    size_t n = bodies.size();

    // Resize temporary storage if needed
    if (m_k1.size() != n) {
        m_k1.resize(n);
        m_k2.resize(n);
        m_k3.resize(n);
        m_k4.resize(n);
        m_tempState.resize(n);
    }

    // Save initial state
    for (size_t i = 0; i < n; i++) {
        m_tempState[i].position = bodies[i]->position();
        m_tempState[i].velocity = bodies[i]->velocity();
    }

    // k1 = f(t, y)
    computeAccelerations(bodies, octree, G);
    for (size_t i = 0; i < n; i++) {
        m_k1[i].position = bodies[i]->velocity();
        m_k1[i].velocity = bodies[i]->acceleration();
    }

    // k2 = f(t + dt/2, y + k1*dt/2)
    for (size_t i = 0; i < n; i++) {
        bodies[i]->setPosition(m_tempState[i].position + m_k1[i].position * (dt * 0.5));
        bodies[i]->setVelocity(m_tempState[i].velocity + m_k1[i].velocity * (dt * 0.5));
    }
    computeAccelerations(bodies, octree, G);
    for (size_t i = 0; i < n; i++) {
        m_k2[i].position = bodies[i]->velocity();
        m_k2[i].velocity = bodies[i]->acceleration();
    }

    // k3 = f(t + dt/2, y + k2*dt/2)
    for (size_t i = 0; i < n; i++) {
        bodies[i]->setPosition(m_tempState[i].position + m_k2[i].position * (dt * 0.5));
        bodies[i]->setVelocity(m_tempState[i].velocity + m_k2[i].velocity * (dt * 0.5));
    }
    computeAccelerations(bodies, octree, G);
    for (size_t i = 0; i < n; i++) {
        m_k3[i].position = bodies[i]->velocity();
        m_k3[i].velocity = bodies[i]->acceleration();
    }

    // k4 = f(t + dt, y + k3*dt)
    for (size_t i = 0; i < n; i++) {
        bodies[i]->setPosition(m_tempState[i].position + m_k3[i].position * dt);
        bodies[i]->setVelocity(m_tempState[i].velocity + m_k3[i].velocity * dt);
    }
    computeAccelerations(bodies, octree, G);
    for (size_t i = 0; i < n; i++) {
        m_k4[i].position = bodies[i]->velocity();
        m_k4[i].velocity = bodies[i]->acceleration();
    }

    // Final update: y(t+dt) = y(t) + (k1 + 2*k2 + 2*k3 + k4) * dt/6
    for (size_t i = 0; i < n; i++) {
        glm::dvec3 dPos = (m_k1[i].position + 2.0 * m_k2[i].position +
                          2.0 * m_k3[i].position + m_k4[i].position) * (dt / 6.0);
        glm::dvec3 dVel = (m_k1[i].velocity + 2.0 * m_k2[i].velocity +
                          2.0 * m_k3[i].velocity + m_k4[i].velocity) * (dt / 6.0);

        bodies[i]->setPosition(m_tempState[i].position + dPos);
        bodies[i]->setVelocity(m_tempState[i].velocity + dVel);
    }
}

// Adaptive timestep
AdaptiveTimestep::AdaptiveTimestep(double minDt, double maxDt, double tolerance)
    : m_minDt(minDt)
    , m_maxDt(maxDt)
    , m_tolerance(tolerance)
{
}

double AdaptiveTimestep::calculateTimestep(const std::vector<std::unique_ptr<CelestialBody>>& bodies,
                                           double G) const {
    if (bodies.empty()) return m_maxDt;

    double dt = m_maxDt;

    // Find the smallest dynamical timescale
    // t_dyn ~ sqrt(r^3 / (G * M)) for orbital motion
    // Also consider: dt < eta * |r| / |v| for proper resolution

    for (size_t i = 0; i < bodies.size(); i++) {
        for (size_t j = i + 1; j < bodies.size(); j++) {
            glm::dvec3 dr = bodies[j]->position() - bodies[i]->position();
            double r = glm::length(dr);

            if (r > 0) {
                // Dynamical timescale
                double totalMass = bodies[i]->mass() + bodies[j]->mass();
                double t_dyn = std::sqrt(r * r * r / (G * totalMass));

                // Require several steps per orbit
                dt = std::min(dt, m_tolerance * t_dyn);

                // Velocity-based constraint
                glm::dvec3 dv = bodies[j]->velocity() - bodies[i]->velocity();
                double v_rel = glm::length(dv);
                if (v_rel > 0) {
                    dt = std::min(dt, m_tolerance * r / v_rel);
                }
            }
        }
    }

    return std::clamp(dt, m_minDt, m_maxDt);
}

} // namespace astrocore
