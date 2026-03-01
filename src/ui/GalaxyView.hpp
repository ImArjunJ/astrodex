#pragma once

#include <imgui.h>
#include <vector>
#include <string>
#include <functional>

namespace astrocore {

struct PlanetParams;

// GalaxyView - Galaxy navigation screen with cached exoplanet browser
class GalaxyView {
public:
    GalaxyView();

    void init(float W, float H);
    void reset();

    void update(float dt, float W, float H);
    void renderBackground(ImDrawList* dl, float W, float H);
    bool renderUI(float W, float H);
    void releaseBorder();

    bool isInitialized()   const { return m_initialized; }
    bool isExplosionDone() const { return m_explosionDone && m_borderAssembled; }
    const std::string& selectedName()   const { return m_selectedName; }
    int                selectedPreset() const { return m_selectedPreset; }

    // Returns true once if Solar System button was clicked, then resets
    bool wasSolarSystemRequested() {
        bool v = m_solarSystemRequested;
        m_solarSystemRequested = false;
        return v;
    }

    void setExoplanetCallback(std::function<void(const std::string&)> cb);
    void setFetchMetadataCallback(std::function<void(const std::string&)> cb);
    void setExoplanetStatus(const std::string& status);
    void setFetchingMetadata(bool fetching) { m_fetchingMetadata = fetching; }

    // Update a planet's metadata (call after fetching from NASA)
    void updatePlanetMetadata(const std::string& name, const std::string& hostStar,
                              float distanceLY, float radiusEarth, float massEarth,
                              float tempK);

    // Check if selected planet needs metadata fetch
    bool selectedNeedsMetadata() const;

    // Load all cached planets - call after init
    void loadCachedPlanets();

    // Add a planet entry
    void addExoplanet(const std::string& name, const std::string& type,
                      float distanceLY, const std::string& hostStar = "",
                      float radiusEarth = 0.f, float massEarth = 0.f,
                      float tempK = 0.f);

private:
    struct Star {
        float x, y;
        float size;
        float r, g, b;
        float twinklePhase;
        float twinkleSpeed;
    };

    struct GalaxyPlanet {
        std::string name;
        std::string typeStr;
        std::string hostStar;
        float       x, y;
        float       size;
        float       r, g, b;
        int         presetIdx;    // >=0 = preset; -1 = exoplanet
        float       distanceLY;
        float       radiusEarth;  // 0 = unknown
        float       massEarth;    // 0 = unknown
        float       tempK;        // 0 = unknown
        bool        isCached;     // true if we have cached render params
    };

    struct ExplosionPart {
        float x, y;
        float vx, vy;
        float alpha;
        float size;
        float r, g, b;
    };

    struct TransPart {
        float x, y;
        float tx, ty;
        float alpha;
        float size;
        float r, g, b;
    };

    void generateStars(float W, float H);
    void generatePresetPlanets(float W, float H);
    void assignPlanetPositions(float W, float H);
    void triggerExplosion();
    void triggerTransition(float W, float H);
    void updateTransition(float dt);
    void drawTransitionParticles(ImDrawList* dl);
    void drawGlowLine(ImDrawList* dl, ImVec2 a, ImVec2 b, float alpha) const;
    void updateSearch();
    void pickHighlightStar(const std::string& planetName);
    ImU32 getPlanetColor(const GalaxyPlanet& p, float alpha = 1.0f) const;

    float galaxyCX(float W) const;
    float galaxyCY(float H) const;

    std::vector<Star>         m_stars;
    std::vector<GalaxyPlanet> m_planets;
    std::vector<int>          m_filteredIndices;  // indices into m_planets matching filter

    int m_selectedIdx = 0;
    int m_hoveredIdx  = -1;
    int m_listScrollIdx = 0;

    char m_searchBuf[256] = {};
    std::vector<int> m_searchMatches;

    std::vector<ExplosionPart> m_expParts;
    bool  m_exploding      = false;
    float m_explosionTimer = 0.f;
    bool  m_explosionDone  = false;

    std::vector<TransPart> m_transParts;
    bool  m_transitioning    = false;
    float m_transTimer       = 0.f;
    bool  m_transDone        = false;
    bool  m_borderAssembled  = false;
    bool  m_borderFading     = false;
    float m_borderFadeStart  = 0.f;

    float m_time        = 0.f;
    bool  m_initialized = false;

    float m_lastW = 0.f, m_lastH = 0.f;

    int m_highlightedStarIdx = -1;

    std::string m_selectedName;
    int         m_selectedPreset = -1;

    std::function<void(const std::string&)> m_exoCallback;
    std::function<void(const std::string&)> m_fetchMetadataCallback;
    std::string m_exoStatus = "Select a planet to view...";
    bool m_fetchingMetadata = false;
    bool m_solarSystemRequested = false;
};

}  // namespace astrocore
