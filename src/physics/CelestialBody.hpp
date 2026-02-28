#pragma once

#include <glm/glm.hpp>
#include <string>
#include <cstdint>

namespace astrocore {

// Forward declaration
struct PlanetParams;

// Types of celestial bodies
enum class BodyType {
    Star,
    Planet,
    Moon,
    DwarfPlanet,
    Asteroid,
    Comet,
    BlackHole,
    NeutronStar,
    Custom
};

// Physical and orbital state of a celestial body
class CelestialBody {
public:
    CelestialBody(const std::string& name = "Body", BodyType type = BodyType::Planet);
    ~CelestialBody() = default;

    // Identifiers
    uint64_t id() const { return m_id; }
    const std::string& name() const { return m_name; }
    BodyType type() const { return m_type; }

    // Physical properties (SI units: kg, m)
    double mass() const { return m_mass; }
    double radius() const { return m_radius; }
    double density() const;

    void setMass(double mass) { m_mass = mass; }
    void setRadius(double radius) { m_radius = radius; }
    void setMassFromDensity(double density);

    // Position and velocity (SI units: m, m/s)
    // Using double precision for astronomical distances
    const glm::dvec3& position() const { return m_position; }
    const glm::dvec3& velocity() const { return m_velocity; }

    void setPosition(const glm::dvec3& pos) { m_position = pos; }
    void setVelocity(const glm::dvec3& vel) { m_velocity = vel; }

    // Rotation
    const glm::dvec3& rotationAxis() const { return m_rotationAxis; }
    double rotationPeriod() const { return m_rotationPeriod; }  // seconds
    double rotationAngle() const { return m_rotationAngle; }

    void setRotation(const glm::dvec3& axis, double period);
    void setRotationAngle(double angle) { m_rotationAngle = angle; }

    // For integration
    glm::dvec3& acceleration() { return m_acceleration; }
    const glm::dvec3& acceleration() const { return m_acceleration; }
    void clearAcceleration() { m_acceleration = glm::dvec3(0.0); }
    void addAcceleration(const glm::dvec3& a) { m_acceleration += a; }

    // Apply force (F = ma, so a = F/m)
    void applyForce(const glm::dvec3& force);

    // Sphere of influence (for hierarchical simulation)
    double sphereOfInfluence() const { return m_soi; }
    void setSphereOfInfluence(double soi) { m_soi = soi; }

    // Parent body (for moons orbiting planets, etc.)
    CelestialBody* parent() const { return m_parent; }
    void setParent(CelestialBody* parent) { m_parent = parent; }

    // Orbital mechanics helpers
    double orbitalSpeed(double parentMass, double G) const;
    double escapeVelocity(double G) const;
    double hillSphereRadius(double parentMass, double semiMajorAxis) const;

    // Rendering scale (visual radius in render units)
    float renderRadius() const { return m_renderRadius; }
    void setRenderRadius(float r) { m_renderRadius = r; }

    // Emissive (for stars)
    bool isEmissive() const { return m_isEmissive; }
    void setEmissive(bool e) { m_isEmissive = e; }
    const glm::vec3& emissiveColor() const { return m_emissiveColor; }
    void setEmissiveColor(const glm::vec3& c) { m_emissiveColor = c; }

    // Static factory methods for common body types
    static CelestialBody createStar(const std::string& name, double mass, double radius);
    static CelestialBody createPlanet(const std::string& name, double mass, double radius);
    static CelestialBody createMoon(const std::string& name, double mass, double radius);
    static CelestialBody createAsteroid(const std::string& name, double mass, double radius);

    // Real solar system bodies (approximate values)
    static CelestialBody Sun();
    static CelestialBody Mercury();
    static CelestialBody Venus();
    static CelestialBody Earth();
    static CelestialBody Moon();
    static CelestialBody Mars();
    static CelestialBody Jupiter();
    static CelestialBody Saturn();
    static CelestialBody Uranus();
    static CelestialBody Neptune();

private:
    static uint64_t s_nextId;

    uint64_t m_id;
    std::string m_name;
    BodyType m_type;

    // Physical properties
    double m_mass = 1.0e24;      // kg (default ~Earth mass)
    double m_radius = 6.371e6;  // m (default ~Earth radius)

    // State vectors
    glm::dvec3 m_position{0.0};
    glm::dvec3 m_velocity{0.0};
    glm::dvec3 m_acceleration{0.0};

    // Rotation
    glm::dvec3 m_rotationAxis{0.0, 1.0, 0.0};
    double m_rotationPeriod = 86400.0;  // seconds (default 1 day)
    double m_rotationAngle = 0.0;

    // Hierarchy
    CelestialBody* m_parent = nullptr;
    double m_soi = 0.0;

    // Rendering
    float m_renderRadius = 1.0f;
    bool m_isEmissive = false;
    glm::vec3 m_emissiveColor{1.0f, 0.9f, 0.7f};
};

// Physical constants
namespace constants {
    constexpr double G = 6.67430e-11;           // Gravitational constant (m^3 kg^-1 s^-2)
    constexpr double AU = 1.495978707e11;       // Astronomical unit (m)
    constexpr double c = 299792458.0;           // Speed of light (m/s)
    constexpr double SOLAR_MASS = 1.989e30;     // kg
    constexpr double EARTH_MASS = 5.972e24;     // kg
    constexpr double EARTH_RADIUS = 6.371e6;    // m
    constexpr double LUNAR_MASS = 7.342e22;     // kg
    constexpr double LUNAR_RADIUS = 1.737e6;    // m
    constexpr double YEAR = 31557600.0;         // seconds (Julian year)
    constexpr double DAY = 86400.0;             // seconds
}

} // namespace astrocore
