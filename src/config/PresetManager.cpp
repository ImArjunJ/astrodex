#include "config/PresetManager.hpp"
#include "physics/PhysicsWorld.hpp"
#include <spdlog/spdlog.h>
#include <fstream>
#include <algorithm>

namespace astrocore {

PresetManager::PresetManager() {
}

void PresetManager::setPresetsDirectory(const std::string& directory) {
    m_presetsDirectory = directory;
}

void PresetManager::scanPresets() {
    m_presets.clear();

    namespace fs = std::filesystem;

    if (!fs::exists(m_presetsDirectory)) {
        spdlog::warn("Presets directory does not exist: {}", m_presetsDirectory);
        return;
    }

    for (const auto& entry : fs::directory_iterator(m_presetsDirectory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            auto info = extractPresetInfo(entry.path());
            if (info.has_value()) {
                m_presets.push_back(info.value());
            }
        }
    }

    // Sort by name
    std::sort(m_presets.begin(), m_presets.end(),
              [](const PresetInfo& a, const PresetInfo& b) {
                  return a.name < b.name;
              });

    spdlog::info("Found {} presets in {}", m_presets.size(), m_presetsDirectory);

    if (m_onPresetsScanned) {
        m_onPresetsScanned(m_presets);
    }
}

std::optional<PresetManager::PresetInfo> PresetManager::extractPresetInfo(
    const std::filesystem::path& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return std::nullopt;
        }

        nlohmann::json j;
        file >> j;

        PresetInfo info;
        info.filename = filepath.filename().string();
        info.name = j.value("name", filepath.stem().string());
        info.description = j.value("description", "");
        info.source = j.value("source", "unknown");

        if (j.contains("bodies") && j["bodies"].is_array()) {
            info.bodyCount = static_cast<int>(j["bodies"].size());
        }

        return info;
    } catch (const std::exception& e) {
        spdlog::warn("Failed to parse preset {}: {}", filepath.string(), e.what());
        return std::nullopt;
    }
}

std::optional<SystemConfig> PresetManager::loadPreset(const std::string& filename) {
    namespace fs = std::filesystem;
    fs::path filepath = fs::path(m_presetsDirectory) / filename;
    return SystemConfig::loadFromFile(filepath.string());
}

std::optional<SystemConfig> PresetManager::loadPresetByIndex(size_t index) {
    if (index >= m_presets.size()) {
        spdlog::error("Preset index out of range: {}", index);
        return std::nullopt;
    }
    return loadPreset(m_presets[index].filename);
}

const PresetManager::PresetInfo* PresetManager::getPresetInfo(size_t index) const {
    if (index >= m_presets.size()) {
        return nullptr;
    }
    return &m_presets[index];
}

bool PresetManager::applyConfigToWorld(const SystemConfig& config, PhysicsWorld& world) {
    world.clear();
    world.setG(config.gravitationalConstant);

    // Set integrator type
    IntegratorType intType = IntegratorType::Verlet;
    if (config.integratorType == "Euler") intType = IntegratorType::Euler;
    else if (config.integratorType == "Verlet") intType = IntegratorType::Verlet;
    else if (config.integratorType == "RK4") intType = IntegratorType::RK4;
    else if (config.integratorType == "Leapfrog") intType = IntegratorType::Leapfrog;
    else if (config.integratorType == "Hermite") intType = IntegratorType::Hermite;
    world.setIntegrator(intType);

    // First pass: add all bodies
    for (const auto& bodyConfig : config.bodies) {
        CelestialBody body = bodyConfig.toCelestialBody();
        world.addBody(body);
    }

    // Second pass: set up orbital relationships
    for (size_t i = 0; i < config.bodies.size(); i++) {
        const auto& bodyConfig = config.bodies[i];

        if (bodyConfig.parentName.has_value() && bodyConfig.orbitalDistance.has_value()) {
            CelestialBody* child = world.getBodyByName(bodyConfig.name);
            CelestialBody* parent = world.getBodyByName(bodyConfig.parentName.value());

            if (child && parent) {
                // Position the child at orbital distance from parent
                glm::dvec3 direction(1.0, 0.0, 0.0);  // Default: +X direction
                child->setPosition(parent->position() + direction * bodyConfig.orbitalDistance.value());

                // Set up circular orbit
                PhysicsWorld::setCircularOrbit(*child, *parent, config.gravitationalConstant);
            }
        }
    }

    spdlog::info("Applied config '{}' with {} bodies", config.name, world.bodyCount());
    return true;
}

SystemConfig PresetManager::createConfigFromWorld(const PhysicsWorld& world,
                                                   const std::string& name,
                                                   const std::string& description) {
    SystemConfig config;
    config.name = name;
    config.description = description;
    config.source = "custom";
    config.gravitationalConstant = world.G();

    // Convert integrator type to string
    switch (world.integratorType()) {
        case IntegratorType::Euler: config.integratorType = "Euler"; break;
        case IntegratorType::Verlet: config.integratorType = "Verlet"; break;
        case IntegratorType::RK4: config.integratorType = "RK4"; break;
        case IntegratorType::Leapfrog: config.integratorType = "Leapfrog"; break;
        case IntegratorType::Hermite: config.integratorType = "Hermite"; break;
    }

    for (const auto& bodyPtr : world.bodies()) {
        config.bodies.push_back(BodyConfig::fromCelestialBody(*bodyPtr));
    }

    return config;
}

bool PresetManager::savePreset(const SystemConfig& config, const std::string& filename) {
    namespace fs = std::filesystem;

    // Ensure directory exists
    if (!fs::exists(m_presetsDirectory)) {
        fs::create_directories(m_presetsDirectory);
    }

    fs::path filepath = fs::path(m_presetsDirectory) / filename;
    return config.saveToFile(filepath.string());
}

void PresetManager::generateBuiltInPresets() {
    namespace fs = std::filesystem;

    // Ensure directory exists
    if (!fs::exists(m_presetsDirectory)) {
        fs::create_directories(m_presetsDirectory);
    }

    // Generate solar system preset
    {
        auto solarSystem = SystemConfig::createSolarSystem();
        fs::path filepath = fs::path(m_presetsDirectory) / "solar_system.json";
        solarSystem.saveToFile(filepath.string());
    }

    // Generate binary star preset
    {
        auto binary = SystemConfig::createBinaryStarSystem(
            constants::SOLAR_MASS,
            0.8 * constants::SOLAR_MASS,
            0.5 * constants::AU
        );
        fs::path filepath = fs::path(m_presetsDirectory) / "binary_stars.json";
        binary.saveToFile(filepath.string());
    }

    spdlog::info("Generated built-in presets in {}", m_presetsDirectory);

    // Rescan to update the list
    scanPresets();
}

}  // namespace astrocore
