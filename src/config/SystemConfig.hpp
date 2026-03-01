#pragma once

#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>
#include <glm/glm.hpp>
#include "physics/CelestialBody.hpp"

namespace astrocore {

// Serializable configuration for a single celestial body
struct BodyConfig {
    std::string name;
    std::string type;  // "Star", "Planet", "Moon", etc.

    // Physical properties (SI units)
    double mass = 0.0;           // kg
    double radius = 0.0;         // m

    // Position and velocity (SI units)
    glm::dvec3 position{0.0};    // m
    glm::dvec3 velocity{0.0};    // m/s

    // Rotation
    glm::dvec3 rotationAxis{0.0, 1.0, 0.0};
    double rotationPeriod = 86400.0;  // seconds

    // Optional: orbital setup relative to parent
    std::optional<std::string> parentName;
    std::optional<double> orbitalDistance;  // m, for automatic circular orbit calculation

    // Rendering
    bool isEmissive = false;
    glm::vec3 emissiveColor{1.0f, 0.9f, 0.7f};

    // Convert to CelestialBody
    CelestialBody toCelestialBody() const;

    // Create from CelestialBody
    static BodyConfig fromCelestialBody(const CelestialBody& body);

    // JSON serialization
    nlohmann::json toJson() const;
    static BodyConfig fromJson(const nlohmann::json& j);
};

// Configuration for a complete celestial system
struct SystemConfig {
    std::string name;
    std::string description;
    std::string source;  // "preset", "nasa_tap", "custom", etc.
    std::string version = "1.0";

    // Bodies in the system
    std::vector<BodyConfig> bodies;

    // Simulation settings
    double gravitationalConstant = constants::G;
    std::string integratorType = "Verlet";  // "Euler", "Verlet", "RK4", "Leapfrog"
    double defaultTimestep = constants::DAY;  // seconds

    // Scaling for visualization
    double visualScale = 50000.0 / constants::AU;

    // Save to JSON file
    bool saveToFile(const std::string& filepath) const;

    // Load from JSON file
    static std::optional<SystemConfig> loadFromFile(const std::string& filepath);

    // JSON serialization
    nlohmann::json toJson() const;
    static SystemConfig fromJson(const nlohmann::json& j);

    // Factory methods for common systems
    static SystemConfig createSolarSystem();
    static SystemConfig createBinaryStarSystem(double mass1, double mass2, double separation);
};

// Helper functions for BodyType conversion
std::string bodyTypeToString(BodyType type);
BodyType stringToBodyType(const std::string& str);

}  // namespace astrocore
