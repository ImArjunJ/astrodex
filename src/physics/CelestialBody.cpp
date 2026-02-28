#include "physics/CelestialBody.hpp"
#include <cmath>

namespace astrocore {

uint64_t CelestialBody::s_nextId = 1;

CelestialBody::CelestialBody(const std::string& name, BodyType type)
    : m_id(s_nextId++)
    , m_name(name)
    , m_type(type)
{
}

double CelestialBody::density() const {
    double volume = (4.0 / 3.0) * M_PI * m_radius * m_radius * m_radius;
    return m_mass / volume;
}

void CelestialBody::setMassFromDensity(double density) {
    double volume = (4.0 / 3.0) * M_PI * m_radius * m_radius * m_radius;
    m_mass = density * volume;
}

void CelestialBody::setRotation(const glm::dvec3& axis, double period) {
    m_rotationAxis = glm::normalize(axis);
    m_rotationPeriod = period;
}

void CelestialBody::applyForce(const glm::dvec3& force) {
    if (m_mass > 0.0) {
        m_acceleration += force / m_mass;
    }
}

double CelestialBody::orbitalSpeed(double parentMass, double G) const {
    double r = glm::length(m_position);
    if (r <= 0.0) return 0.0;
    return std::sqrt(G * parentMass / r);
}

double CelestialBody::escapeVelocity(double G) const {
    return std::sqrt(2.0 * G * m_mass / m_radius);
}

double CelestialBody::hillSphereRadius(double parentMass, double semiMajorAxis) const {
    if (parentMass <= 0.0) return 0.0;
    return semiMajorAxis * std::cbrt(m_mass / (3.0 * parentMass));
}

// Factory methods
CelestialBody CelestialBody::createStar(const std::string& name, double mass, double radius) {
    CelestialBody body(name, BodyType::Star);
    body.setMass(mass);
    body.setRadius(radius);
    return body;
}

CelestialBody CelestialBody::createPlanet(const std::string& name, double mass, double radius) {
    CelestialBody body(name, BodyType::Planet);
    body.setMass(mass);
    body.setRadius(radius);
    return body;
}

CelestialBody CelestialBody::createMoon(const std::string& name, double mass, double radius) {
    CelestialBody body(name, BodyType::Moon);
    body.setMass(mass);
    body.setRadius(radius);
    return body;
}

CelestialBody CelestialBody::createAsteroid(const std::string& name, double mass, double radius) {
    CelestialBody body(name, BodyType::Asteroid);
    body.setMass(mass);
    body.setRadius(radius);
    return body;
}

// Real solar system bodies
CelestialBody CelestialBody::Sun() {
    CelestialBody body("Sun", BodyType::Star);
    body.setMass(constants::SOLAR_MASS);
    body.setRadius(6.9634e8);  // meters
    body.setRotation({0.0, 1.0, 0.0}, 25.05 * constants::DAY);  // Equatorial rotation
    return body;
}

CelestialBody CelestialBody::Earth() {
    CelestialBody body("Earth", BodyType::Planet);
    body.setMass(constants::EARTH_MASS);
    body.setRadius(constants::EARTH_RADIUS);
    body.setRotation({0.0, 0.9175, 0.3978}, constants::DAY);  // 23.4 degree tilt
    return body;
}

CelestialBody CelestialBody::Moon() {
    CelestialBody body("Moon", BodyType::Moon);
    body.setMass(constants::LUNAR_MASS);
    body.setRadius(constants::LUNAR_RADIUS);
    body.setRotation({0.0, 1.0, 0.0}, 27.3 * constants::DAY);  // Tidally locked
    return body;
}

CelestialBody CelestialBody::Mars() {
    CelestialBody body("Mars", BodyType::Planet);
    body.setMass(6.4171e23);
    body.setRadius(3.3895e6);
    body.setRotation({0.0, 0.9397, 0.342}, 1.026 * constants::DAY);  // 25.2 degree tilt
    return body;
}

CelestialBody CelestialBody::Jupiter() {
    CelestialBody body("Jupiter", BodyType::Planet);
    body.setMass(1.8982e27);
    body.setRadius(6.9911e7);
    body.setRotation({0.0, 0.9986, 0.0523}, 9.925 * 3600.0);  // ~10 hour rotation
    return body;
}

CelestialBody CelestialBody::Mercury() {
    CelestialBody body("Mercury", BodyType::Planet);
    body.setMass(3.3011e23);
    body.setRadius(2.4397e6);
    body.setRotation({0.0, 1.0, 0.0}, 58.646 * constants::DAY);
    return body;
}

CelestialBody CelestialBody::Venus() {
    CelestialBody body("Venus", BodyType::Planet);
    body.setMass(4.8675e24);
    body.setRadius(6.0518e6);
    body.setRotation({0.0, -1.0, 0.0}, 243.025 * constants::DAY);  // Retrograde
    return body;
}

CelestialBody CelestialBody::Saturn() {
    CelestialBody body("Saturn", BodyType::Planet);
    body.setMass(5.6834e26);
    body.setRadius(5.8232e7);
    body.setRotation({0.0, 0.9163, 0.4005}, 10.7 * 3600.0);  // 26.7 degree tilt
    return body;
}

CelestialBody CelestialBody::Uranus() {
    CelestialBody body("Uranus", BodyType::Planet);
    body.setMass(8.6810e25);
    body.setRadius(2.5362e7);
    body.setRotation({0.0, -0.0872, 0.9962}, 17.24 * 3600.0);  // 97.8 degree tilt
    return body;
}

CelestialBody CelestialBody::Neptune() {
    CelestialBody body("Neptune", BodyType::Planet);
    body.setMass(1.02413e26);
    body.setRadius(2.4622e7);
    body.setRotation({0.0, 0.8829, 0.4695}, 16.11 * 3600.0);  // 28.3 degree tilt
    return body;
}

} // namespace astrocore
