#pragma once

#include "explorer/StarData.hpp"

#include <imgui.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <memory>
#include <functional>

namespace astrocore {

struct PlanetParams;
struct StarVertex;
struct StarInfo;
class PlanetThumbnailRenderer;
class StarRenderer;
class FreeFlyCamera;
class StarData;
class StarLOD;

// GalaxyView - Galaxy navigation screen with cached exoplanet browser
class GalaxyView {
public:
    GalaxyView();
    ~GalaxyView();

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
                              float tempK, const std::string& gaiaDr3Id = "");

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
        std::string gaiaDr3Id;    // Gaia DR3 source ID for cross-matching
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

    // Catalog panel state
    std::unique_ptr<PlanetThumbnailRenderer> m_thumbnailRenderer;
    bool m_catalogOpen = false;
    float m_catalogSlideAnim = 0.0f;  // 0 = closed, 1 = fully open
    float m_catalogScrollY = 0.0f;
    int m_catalogTileSize = 100;
    float m_catalogPanelWidth = 450.f;  // User-resizable width

    // Catalog filter settings
    float m_filterHueMin = 0.0f;
    float m_filterHueMax = 1.0f;
    float m_filterBrightnessMin = 0.0f;
    float m_filterBrightnessMax = 1.0f;
    bool m_filterHasAtmosphere = false;
    bool m_filterAtmosphereEnabled = false;
    std::string m_catalogTypeFilter;  // Empty = all types

    void renderCatalogPanel(float W, float H);
    void renderCatalogTile(int planetIdx, float x, float y, float size);
    bool planetPassesFilter(int planetIdx) const;

    // Real star field (Gaia DR3 + HYG)
    void renderStarField(float W, float H);
    void handleCameraInput(float dt);
    void updateCameraAnimation(float dt);
    void updateStarLOD();

    std::unique_ptr<StarRenderer>  m_starRenderer;
    std::unique_ptr<FreeFlyCamera> m_freeCamera;
    std::unique_ptr<StarData>      m_starData;
    std::unique_ptr<StarLOD>       m_starLOD;

    // Star render settings
    float m_pointScale      = 2.0f;
    float m_brightnessBoost = 1.5f;
    bool  m_debugStars      = false;
    float m_shellMultiplier = 1.0f;

    // LOD state
    bool  m_useLOD = false;
    std::vector<StarVertex> m_visibleStars;
    int   m_visibleCount = 0;
    int   m_totalStarCount = 0;

    // Nearest star cache
    float    m_nearestTimer = 0.0f;
    StarInfo m_cachedNearest{};

    // Transition state (zooming into/out of planet)
    enum class TransitionState { None, ZoomingIn, ViewingPlanet, ZoomingOut };
    TransitionState m_transitionState = TransitionState::None;
    float m_transitionProgress = 0.0f;
    float m_transitionDuration = 2.0f;
    int m_transitionPlanetIdx = -1;

    // Warp drive effect
    float m_warpFactor = 0.0f;       // 0 = normal, 1 = full warp streaks
    glm::vec3 m_warpDirection{0.0f}; // random direction for warp travel
    float m_warpTargetYaw = 0.0f;    // yaw/pitch facing the warp direction
    float m_warpTargetPitch = 0.0f;

    // Camera state saved for zoom transitions
    glm::vec3 m_savedCameraPos{0.0f};
    float m_savedCameraYaw = -90.0f;
    float m_savedCameraPitch = 0.0f;
    float m_savedCameraSpeed = 1.0f;

    // Input state
    bool m_rightMouseDown = false;
    glm::vec2 m_lastMousePos = glm::vec2(0.0f);

public:
    // Check if we're in a transition or viewing planet (for Application to know)
    bool isZoomingToPlanet() const { return m_transitionState == TransitionState::ZoomingIn; }
    bool isViewingPlanet() const { return m_transitionState == TransitionState::ViewingPlanet; }
    bool isZoomingOut() const { return m_transitionState == TransitionState::ZoomingOut; }
    float getTransitionProgress() const { return m_transitionProgress; }

    // Start zoom transition to planet
    void startZoomToPlanet(int planetIdx);

    // Start zoom back to galaxy
    void startZoomToGalaxy();

    // Called when planet is fully loaded and we can complete transition
    void onPlanetReady();
};

}  // namespace astrocore
