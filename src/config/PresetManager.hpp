#pragma once

#include "config/SystemConfig.hpp"
#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <functional>

namespace astrocore {

class PhysicsWorld;

// Manages system presets and configurations
class PresetManager {
public:
    struct PresetInfo {
        std::string name;
        std::string filename;
        std::string description;
        std::string source;
        int bodyCount = 0;
    };

    PresetManager();
    ~PresetManager() = default;

    // Set the directory to search for presets
    void setPresetsDirectory(const std::string& directory);
    const std::string& getPresetsDirectory() const { return m_presetsDirectory; }

    // Scan directory for available presets
    void scanPresets();

    // Get list of available presets
    const std::vector<PresetInfo>& getAvailablePresets() const { return m_presets; }

    // Load a preset by filename
    std::optional<SystemConfig> loadPreset(const std::string& filename);

    // Load a preset by index
    std::optional<SystemConfig> loadPresetByIndex(size_t index);

    // Get preset info by index
    const PresetInfo* getPresetInfo(size_t index) const;

    // Apply a SystemConfig to a PhysicsWorld
    // Returns true on success
    bool applyConfigToWorld(const SystemConfig& config, PhysicsWorld& world);

    // Create a SystemConfig from the current state of a PhysicsWorld
    SystemConfig createConfigFromWorld(const PhysicsWorld& world,
                                        const std::string& name = "Custom System",
                                        const std::string& description = "");

    // Save a preset to a file
    bool savePreset(const SystemConfig& config, const std::string& filename);

    // Generate built-in presets (saved to configs directory)
    void generateBuiltInPresets();

    // Callback when presets are scanned
    using OnPresetsScannedCallback = std::function<void(const std::vector<PresetInfo>&)>;
    void setOnPresetsScanned(OnPresetsScannedCallback callback) { m_onPresetsScanned = callback; }

private:
    std::string m_presetsDirectory = "configs";
    std::vector<PresetInfo> m_presets;
    OnPresetsScannedCallback m_onPresetsScanned;

    // Helper to extract preset info from a config file
    std::optional<PresetInfo> extractPresetInfo(const std::filesystem::path& filepath);
};

}  // namespace astrocore
