#pragma once

#include "data/ExoplanetData.hpp"
#include "render/IRenderer.hpp"
#include <vector>
#include <string>
#include <optional>

namespace astrocore {

// A single solar system body: validated physical data + hand-tuned visual params.
// These serve two purposes:
//   1. Direct lookup — if the user searches "Earth", render immediately without AI.
//   2. Analog matching — for unknown exoplanets, find the closest known body and
//      use its visual params as a base before AI fills remaining unknown fields.
struct SolarSystemEntry {
    std::string   name;
    ExoplanetData physicalData;  // Measured physical parameters
    PlanetParams  visualParams;  // Validated renderer parameters
    std::string   description;   // Human-readable visual summary for prompts
};

struct AnalogMatch {
    const SolarSystemEntry* entry    = nullptr;
    float                   score    = 0.0f;  // [0,1]; higher is closer
};

class SolarSystemDatabase {
public:
    static const SolarSystemDatabase& instance();

        // Returns the entry whose (mass, radius, temp) is closest to the query.
        // Returns nullopt when no entry has a score above minScore.
        // excludeName: skip any entry whose name matches this (use when validating
        //   a planet against itself so it cannot be its own analog).
        std::optional<AnalogMatch> findClosestAnalog(double mass_earth,
                                                      double radius_earth,
                                                      double temp_k,
                                                      float  minScore = 0.25f,
                                                      const std::string& excludeName = "") const;

    // Case-insensitive substring lookup.  Returns nullptr if not found.
    const SolarSystemEntry* findByName(const std::string& query) const;

    const std::vector<SolarSystemEntry>& entries() const { return m_entries; }

private:
    SolarSystemDatabase();
    std::vector<SolarSystemEntry> m_entries;

    static float similarity(double mass_e, double radius_e, double temp_k,
                             const SolarSystemEntry& entry);
};

}  // namespace astrocore
