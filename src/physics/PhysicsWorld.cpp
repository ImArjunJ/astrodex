#include "physics/PhysicsWorld.hpp"
#include <cmath>
#include <random>
#include <algorithm>

namespace astrocore {

PhysicsWorld::PhysicsWorld()
    : m_integrator(Integrator::create(IntegratorType::Verlet))
    , m_adaptiveTimestep(1.0, constants::DAY, 0.01)
{
}

PhysicsWorld::~PhysicsWorld() = default;

void PhysicsWorld::step(double dt) {
    if (m_bodies.empty()) return;

    m_integrator->step(m_bodies, m_octree, m_G, dt);
    m_time += dt;
    m_lastDt = dt;

    // Update rotation angles
    for (auto& body : m_bodies) {
        double period = body->rotationPeriod();
        if (period > 0) {
            double dAngle = (2.0 * M_PI * dt) / period;
            body->setRotationAngle(body->rotationAngle() + dAngle);
        }
    }
}

void PhysicsWorld::stepAdaptive() {
    double dt = m_adaptiveTimestep.calculateTimestep(m_bodies, m_G);
    step(dt);
}

void PhysicsWorld::run(double duration, double dt) {
    double endTime = m_time + duration;
    while (m_time < endTime) {
        step(std::min(dt, endTime - m_time));
    }
}

CelestialBody& PhysicsWorld::addBody(std::unique_ptr<CelestialBody> body) {
    m_bodies.push_back(std::move(body));
    return *m_bodies.back();
}

CelestialBody& PhysicsWorld::addBody(const CelestialBody& body) {
    m_bodies.push_back(std::make_unique<CelestialBody>(body));
    return *m_bodies.back();
}

void PhysicsWorld::removeBody(uint64_t id) {
    m_bodies.erase(
        std::remove_if(m_bodies.begin(), m_bodies.end(),
            [id](const std::unique_ptr<CelestialBody>& b) { return b->id() == id; }),
        m_bodies.end()
    );
}

void PhysicsWorld::clear() {
    m_bodies.clear();
    m_octree.clear();
    m_time = 0.0;
}

CelestialBody* PhysicsWorld::getBody(uint64_t id) {
    for (auto& body : m_bodies) {
        if (body->id() == id) return body.get();
    }
    return nullptr;
}

CelestialBody* PhysicsWorld::getBodyByName(const std::string& name) {
    for (auto& body : m_bodies) {
        if (body->name() == name) return body.get();
    }
    return nullptr;
}

void PhysicsWorld::setIntegrator(IntegratorType type) {
    m_integratorType = type;
    m_integrator = Integrator::create(type);
}

void PhysicsWorld::setAdaptiveTimestep(double minDt, double maxDt, double tolerance) {
    m_adaptiveTimestep = AdaptiveTimestep(minDt, maxDt, tolerance);
}

PhysicsWorld::Stats PhysicsWorld::computeStats() const {
    Stats stats;
    stats.octreeNodes = m_octree.getNodeCount();
    stats.maxOctreeDepth = m_octree.getMaxDepth();

    glm::dvec3 weightedPos(0.0);

    for (const auto& body : m_bodies) {
        double m = body->mass();
        glm::dvec3 v = body->velocity();

        stats.totalMass += m;
        stats.totalKineticEnergy += 0.5 * m * glm::dot(v, v);
        weightedPos += body->position() * m;
    }

    if (stats.totalMass > 0) {
        stats.centerOfMass = weightedPos / stats.totalMass;
    }

    // Compute potential energy (O(N^2) for accuracy)
    for (size_t i = 0; i < m_bodies.size(); i++) {
        for (size_t j = i + 1; j < m_bodies.size(); j++) {
            double r = glm::length(m_bodies[j]->position() - m_bodies[i]->position());
            if (r > 0) {
                stats.totalPotentialEnergy -= m_G * m_bodies[i]->mass() * m_bodies[j]->mass() / r;
            }
        }
    }

    stats.totalEnergy = stats.totalKineticEnergy + stats.totalPotentialEnergy;
    return stats;
}

// Static factory: Solar System (all major planets)
PhysicsWorld PhysicsWorld::createSolarSystem() {
    PhysicsWorld world;

    // Sun at origin
    auto sun = CelestialBody::Sun();
    sun.setPosition({0.0, 0.0, 0.0});
    sun.setVelocity({0.0, 0.0, 0.0});
    world.addBody(sun);

    // Mercury - 0.387 AU
    auto mercury = CelestialBody::Mercury();
    mercury.setPosition({0.387 * constants::AU, 0.0, 0.0});
    setCircularOrbit(mercury, sun, constants::G);
    world.addBody(mercury);

    // Venus - 0.723 AU
    auto venus = CelestialBody::Venus();
    venus.setPosition({0.723 * constants::AU, 0.0, 0.0});
    setCircularOrbit(venus, sun, constants::G);
    world.addBody(venus);

    // Earth - 1.0 AU
    auto earth = CelestialBody::Earth();
    earth.setPosition({constants::AU, 0.0, 0.0});
    setCircularOrbit(earth, sun, constants::G);
    world.addBody(earth);

    // Moon orbiting Earth
    auto moon = CelestialBody::Moon();
    moon.setPosition({constants::AU + 384400e3, 0.0, 0.0});  // ~384,400 km from Earth
    setCircularOrbit(moon, earth, constants::G);  // This already includes Earth's velocity
    world.addBody(moon);

    // Mars - 1.524 AU
    auto mars = CelestialBody::Mars();
    mars.setPosition({1.524 * constants::AU, 0.0, 0.0});
    setCircularOrbit(mars, sun, constants::G);
    world.addBody(mars);

    // Jupiter - 5.203 AU
    auto jupiter = CelestialBody::Jupiter();
    jupiter.setPosition({5.203 * constants::AU, 0.0, 0.0});
    setCircularOrbit(jupiter, sun, constants::G);
    world.addBody(jupiter);

    // Saturn - 9.537 AU
    auto saturn = CelestialBody::Saturn();
    saturn.setPosition({9.537 * constants::AU, 0.0, 0.0});
    setCircularOrbit(saturn, sun, constants::G);
    world.addBody(saturn);

    // Uranus - 19.19 AU
    auto uranus = CelestialBody::Uranus();
    uranus.setPosition({19.19 * constants::AU, 0.0, 0.0});
    setCircularOrbit(uranus, sun, constants::G);
    world.addBody(uranus);

    // Neptune - 30.07 AU
    auto neptune = CelestialBody::Neptune();
    neptune.setPosition({30.07 * constants::AU, 0.0, 0.0});
    setCircularOrbit(neptune, sun, constants::G);
    world.addBody(neptune);

    return world;
}

// Static factory: Binary system
PhysicsWorld PhysicsWorld::createBinarySystem(double mass1, double mass2, double separation) {
    PhysicsWorld world;

    double totalMass = mass1 + mass2;
    double r1 = separation * mass2 / totalMass;  // Distance of body 1 from COM
    double r2 = separation * mass1 / totalMass;  // Distance of body 2 from COM

    // Orbital velocity for circular orbit
    double v1 = std::sqrt(constants::G * mass2 * mass2 / (totalMass * separation));
    double v2 = std::sqrt(constants::G * mass1 * mass1 / (totalMass * separation));

    CelestialBody body1("Star A", BodyType::Star);
    body1.setMass(mass1);
    body1.setRadius(std::cbrt(mass1 / constants::SOLAR_MASS) * 6.96e8);
    body1.setPosition({-r1, 0.0, 0.0});
    body1.setVelocity({0.0, -v1, 0.0});
    world.addBody(body1);

    CelestialBody body2("Star B", BodyType::Star);
    body2.setMass(mass2);
    body2.setRadius(std::cbrt(mass2 / constants::SOLAR_MASS) * 6.96e8);
    body2.setPosition({r2, 0.0, 0.0});
    body2.setVelocity({0.0, v2, 0.0});
    world.addBody(body2);

    return world;
}

// Static factory: Random system for testing
PhysicsWorld PhysicsWorld::createRandomSystem(int numBodies, double regionSize, double totalMass) {
    PhysicsWorld world;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> posDist(-regionSize * 0.5, regionSize * 0.5);
    std::uniform_real_distribution<double> massDist(0.5, 1.5);

    double massSum = 0.0;
    std::vector<double> masses(numBodies);
    for (int i = 0; i < numBodies; i++) {
        masses[i] = massDist(gen);
        massSum += masses[i];
    }

    // Normalize masses
    for (int i = 0; i < numBodies; i++) {
        masses[i] = masses[i] * totalMass / massSum;
    }

    for (int i = 0; i < numBodies; i++) {
        CelestialBody body("Body " + std::to_string(i), BodyType::Planet);
        body.setMass(masses[i]);
        body.setRadius(std::cbrt(masses[i] / constants::EARTH_MASS) * constants::EARTH_RADIUS);
        body.setPosition({posDist(gen), posDist(gen), posDist(gen)});
        // Start with small random velocities
        double vScale = std::sqrt(constants::G * totalMass / regionSize) * 0.1;
        std::uniform_real_distribution<double> velDist(-vScale, vScale);
        body.setVelocity({velDist(gen), velDist(gen), velDist(gen)});
        world.addBody(body);
    }

    return world;
}

// Set circular orbit velocity
void PhysicsWorld::setCircularOrbit(CelestialBody& body, const CelestialBody& parent, double G) {
    glm::dvec3 r = body.position() - parent.position();
    double dist = glm::length(r);

    if (dist <= 0) return;

    // v = sqrt(G * M / r)
    double speed = std::sqrt(G * parent.mass() / dist);

    // Velocity perpendicular to radius vector (in XY plane for simplicity)
    glm::dvec3 up(0.0, 1.0, 0.0);
    glm::dvec3 tangent = glm::normalize(glm::cross(up, r));

    // If r is parallel to up, use different perpendicular
    if (glm::length(tangent) < 0.001) {
        tangent = glm::normalize(glm::cross(glm::dvec3(1.0, 0.0, 0.0), r));
    }

    body.setVelocity(parent.velocity() + tangent * speed);
}

// Set elliptical orbit
void PhysicsWorld::setEllipticalOrbit(CelestialBody& body, const CelestialBody& parent,
                                       double semiMajorAxis, double eccentricity, double G) {
    // Start at periapsis
    double rPeri = semiMajorAxis * (1.0 - eccentricity);

    glm::dvec3 direction = glm::normalize(body.position() - parent.position());
    body.setPosition(parent.position() + direction * rPeri);

    // Velocity at periapsis: v = sqrt(G*M * (2/r - 1/a))
    double speed = std::sqrt(G * parent.mass() * (2.0 / rPeri - 1.0 / semiMajorAxis));

    glm::dvec3 up(0.0, 1.0, 0.0);
    glm::dvec3 tangent = glm::normalize(glm::cross(up, direction));

    if (glm::length(tangent) < 0.001) {
        tangent = glm::normalize(glm::cross(glm::dvec3(1.0, 0.0, 0.0), direction));
    }

    body.setVelocity(parent.velocity() + tangent * speed);
}

} // namespace astrocore
