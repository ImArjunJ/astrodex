#include "config/SystemConfig.hpp"
#include <fstream>
#include <spdlog/spdlog.h>

namespace astrocore {

// BodyType string conversion
std::string bodyTypeToString(BodyType type) {
    switch (type) {
        case BodyType::Star: return "Star";
        case BodyType::Planet: return "Planet";
        case BodyType::Moon: return "Moon";
        case BodyType::DwarfPlanet: return "DwarfPlanet";
        case BodyType::Asteroid: return "Asteroid";
        case BodyType::Comet: return "Comet";
        case BodyType::BlackHole: return "BlackHole";
        case BodyType::NeutronStar: return "NeutronStar";
        default: return "Custom";
    }
}

BodyType stringToBodyType(const std::string& str) {
    if (str == "Star") return BodyType::Star;
    if (str == "Planet") return BodyType::Planet;
    if (str == "Moon") return BodyType::Moon;
    if (str == "DwarfPlanet") return BodyType::DwarfPlanet;
    if (str == "Asteroid") return BodyType::Asteroid;
    if (str == "Comet") return BodyType::Comet;
    if (str == "BlackHole") return BodyType::BlackHole;
    if (str == "NeutronStar") return BodyType::NeutronStar;
    return BodyType::Custom;
}

// BodyConfig implementation
CelestialBody BodyConfig::toCelestialBody() const {
    CelestialBody body(name, stringToBodyType(type));
    body.setMass(mass);
    body.setRadius(radius);
    body.setPosition(position);
    body.setVelocity(velocity);
    body.setRotation(rotationAxis, rotationPeriod);
    body.setEmissive(isEmissive);
    body.setEmissiveColor(emissiveColor);
    return body;
}

BodyConfig BodyConfig::fromCelestialBody(const CelestialBody& body) {
    BodyConfig config;
    config.name = body.name();
    config.type = bodyTypeToString(body.type());
    config.mass = body.mass();
    config.radius = body.radius();
    config.position = body.position();
    config.velocity = body.velocity();
    config.rotationAxis = body.rotationAxis();
    config.rotationPeriod = body.rotationPeriod();
    config.isEmissive = body.isEmissive();
    config.emissiveColor = body.emissiveColor();
    return config;
}

nlohmann::json BodyConfig::toJson() const {
    nlohmann::json j;
    j["name"] = name;
    j["type"] = type;
    j["mass"] = mass;
    j["radius"] = radius;
    j["position"] = {position.x, position.y, position.z};
    j["velocity"] = {velocity.x, velocity.y, velocity.z};
    j["rotationAxis"] = {rotationAxis.x, rotationAxis.y, rotationAxis.z};
    j["rotationPeriod"] = rotationPeriod;

    if (parentName.has_value()) {
        j["parentName"] = parentName.value();
    }
    if (orbitalDistance.has_value()) {
        j["orbitalDistance"] = orbitalDistance.value();
    }

    j["isEmissive"] = isEmissive;
    j["emissiveColor"] = {emissiveColor.r, emissiveColor.g, emissiveColor.b};

    return j;
}

BodyConfig BodyConfig::fromJson(const nlohmann::json& j) {
    BodyConfig config;
    config.name = j.value("name", "Body");
    config.type = j.value("type", "Planet");
    config.mass = j.value("mass", 0.0);
    config.radius = j.value("radius", 0.0);

    if (j.contains("position") && j["position"].is_array() && j["position"].size() == 3) {
        config.position = glm::dvec3(j["position"][0], j["position"][1], j["position"][2]);
    }
    if (j.contains("velocity") && j["velocity"].is_array() && j["velocity"].size() == 3) {
        config.velocity = glm::dvec3(j["velocity"][0], j["velocity"][1], j["velocity"][2]);
    }
    if (j.contains("rotationAxis") && j["rotationAxis"].is_array() && j["rotationAxis"].size() == 3) {
        config.rotationAxis = glm::dvec3(j["rotationAxis"][0], j["rotationAxis"][1], j["rotationAxis"][2]);
    }

    config.rotationPeriod = j.value("rotationPeriod", 86400.0);

    if (j.contains("parentName")) {
        config.parentName = j["parentName"].get<std::string>();
    }
    if (j.contains("orbitalDistance")) {
        config.orbitalDistance = j["orbitalDistance"].get<double>();
    }

    config.isEmissive = j.value("isEmissive", false);
    if (j.contains("emissiveColor") && j["emissiveColor"].is_array() && j["emissiveColor"].size() == 3) {
        config.emissiveColor = glm::vec3(j["emissiveColor"][0], j["emissiveColor"][1], j["emissiveColor"][2]);
    }

    return config;
}

// SystemConfig implementation
bool SystemConfig::saveToFile(const std::string& filepath) const {
    try {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            spdlog::error("Failed to open file for writing: {}", filepath);
            return false;
        }
        file << toJson().dump(2);
        spdlog::info("Saved system config to: {}", filepath);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to save system config: {}", e.what());
        return false;
    }
}

std::optional<SystemConfig> SystemConfig::loadFromFile(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            spdlog::error("Failed to open file for reading: {}", filepath);
            return std::nullopt;
        }
        nlohmann::json j;
        file >> j;
        spdlog::info("Loaded system config from: {}", filepath);
        return fromJson(j);
    } catch (const std::exception& e) {
        spdlog::error("Failed to load system config: {}", e.what());
        return std::nullopt;
    }
}

nlohmann::json SystemConfig::toJson() const {
    nlohmann::json j;
    j["name"] = name;
    j["description"] = description;
    j["source"] = source;
    j["version"] = version;

    j["gravitationalConstant"] = gravitationalConstant;
    j["integratorType"] = integratorType;
    j["defaultTimestep"] = defaultTimestep;
    j["visualScale"] = visualScale;

    j["bodies"] = nlohmann::json::array();
    for (const auto& body : bodies) {
        j["bodies"].push_back(body.toJson());
    }

    return j;
}

SystemConfig SystemConfig::fromJson(const nlohmann::json& j) {
    SystemConfig config;
    config.name = j.value("name", "Unknown System");
    config.description = j.value("description", "");
    config.source = j.value("source", "unknown");
    config.version = j.value("version", "1.0");

    config.gravitationalConstant = j.value("gravitationalConstant", constants::G);
    config.integratorType = j.value("integratorType", "Verlet");
    config.defaultTimestep = j.value("defaultTimestep", constants::DAY);
    config.visualScale = j.value("visualScale", 50000.0 / constants::AU);

    if (j.contains("bodies") && j["bodies"].is_array()) {
        for (const auto& bodyJson : j["bodies"]) {
            config.bodies.push_back(BodyConfig::fromJson(bodyJson));
        }
    }

    return config;
}

SystemConfig SystemConfig::createSolarSystem() {
    SystemConfig config;
    config.name = "Solar System";
    config.description = "Our solar system with the Sun, 8 planets, and Earth's Moon";
    config.source = "preset";

    // Sun
    {
        BodyConfig sun;
        sun.name = "Sun";
        sun.type = "Star";
        sun.mass = constants::SOLAR_MASS;
        sun.radius = 6.96e8;
        sun.position = {0.0, 0.0, 0.0};
        sun.velocity = {0.0, 0.0, 0.0};
        sun.rotationPeriod = 25.05 * constants::DAY;
        sun.isEmissive = true;
        sun.emissiveColor = {1.0f, 0.9f, 0.7f};
        config.bodies.push_back(sun);
    }

    // Helper to add planet with circular orbit
    auto addPlanet = [&](const std::string& name, double mass, double radius,
                         double distanceAU, double rotationDays) {
        BodyConfig planet;
        planet.name = name;
        planet.type = "Planet";
        planet.mass = mass;
        planet.radius = radius;
        planet.parentName = "Sun";
        planet.orbitalDistance = distanceAU * constants::AU;
        planet.rotationPeriod = rotationDays * constants::DAY;
        config.bodies.push_back(planet);
    };

    // Mercury
    addPlanet("Mercury", 3.3011e23, 2.4397e6, 0.387, 58.646);
    // Venus
    addPlanet("Venus", 4.8675e24, 6.0518e6, 0.723, -243.025);  // Retrograde rotation
    // Earth
    addPlanet("Earth", constants::EARTH_MASS, constants::EARTH_RADIUS, 1.0, 0.99726968);
    // Mars
    addPlanet("Mars", 6.4171e23, 3.3895e6, 1.524, 1.025957);
    // Jupiter
    addPlanet("Jupiter", 1.8982e27, 6.9911e7, 5.203, 0.41354);
    // Saturn
    addPlanet("Saturn", 5.6834e26, 5.8232e7, 9.537, 0.44401);
    // Uranus
    addPlanet("Uranus", 8.6810e25, 2.5362e7, 19.19, -0.71833);  // Retrograde
    // Neptune
    addPlanet("Neptune", 1.02413e26, 2.4622e7, 30.07, 0.6713);

    // Moon
    {
        BodyConfig moon;
        moon.name = "Moon";
        moon.type = "Moon";
        moon.mass = constants::LUNAR_MASS;
        moon.radius = constants::LUNAR_RADIUS;
        moon.parentName = "Earth";
        moon.orbitalDistance = 384400e3;  // 384,400 km
        moon.rotationPeriod = 27.321661 * constants::DAY;  // Tidally locked
        config.bodies.push_back(moon);
    }

    return config;
}

SystemConfig SystemConfig::createBinaryStarSystem(double mass1, double mass2, double separation) {
    SystemConfig config;
    config.name = "Binary Star System";
    config.description = "Two stars orbiting their common center of mass";
    config.source = "preset";

    double totalMass = mass1 + mass2;
    double r1 = separation * mass2 / totalMass;
    double r2 = separation * mass1 / totalMass;

    double v1 = std::sqrt(constants::G * mass2 * mass2 / (totalMass * separation));
    double v2 = std::sqrt(constants::G * mass1 * mass1 / (totalMass * separation));

    // Star A
    {
        BodyConfig star;
        star.name = "Star A";
        star.type = "Star";
        star.mass = mass1;
        star.radius = std::cbrt(mass1 / constants::SOLAR_MASS) * 6.96e8;
        star.position = {-r1, 0.0, 0.0};
        star.velocity = {0.0, -v1, 0.0};
        star.isEmissive = true;
        star.emissiveColor = {1.0f, 0.85f, 0.6f};
        config.bodies.push_back(star);
    }

    // Star B
    {
        BodyConfig star;
        star.name = "Star B";
        star.type = "Star";
        star.mass = mass2;
        star.radius = std::cbrt(mass2 / constants::SOLAR_MASS) * 6.96e8;
        star.position = {r2, 0.0, 0.0};
        star.velocity = {0.0, v2, 0.0};
        star.isEmissive = true;
        star.emissiveColor = {0.8f, 0.9f, 1.0f};
        config.bodies.push_back(star);
    }

    return config;
}

}  // namespace astrocore
