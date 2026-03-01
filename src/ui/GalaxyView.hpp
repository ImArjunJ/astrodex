#pragma once

#include <imgui.h>
#include <vector>
#include <string>
#include <functional>

namespace astrocore {

struct PlanetParams;

// ── GalaxyView ────────────────────────────────────────────────────────────────
// Renders the galaxy navigation screen (Screen 2).
// Background: random star field + named planet dots drawn on an ImGui draw list.
// Overlay:    left search/preset sidebar and a floating fact-file card connected
//             to the selected planet by glowing constellation lines.
// On Expand:  triggers an outward explosion of all dots (reuses intro mechanic),
//             then signals the application to transition to the planet detail screen.
class GalaxyView {
public:
    GalaxyView();

    void init(float W, float H);
    void reset();   // Call when re-entering from planet detail view

    void update(float dt, float W, float H);

    // Draw star field + planet dots (call before ImGui::NewFrame windows)
    void renderBackground(ImDrawList* dl, float W, float H);

    // Draw ImGui sidebar and fact card; returns true when Expand is pressed
    bool renderUI(float W, float H);

    void releaseBorder();  // Start border fade-out (call once planet UI is visible)

    bool isInitialized()   const { return m_initialized; }
    // True once the star explosion is done AND the border has fully assembled.
    // (Border holds until releaseBorder() is called — does NOT require m_transDone.)
    bool isExplosionDone() const { return m_explosionDone && m_borderAssembled; }
    const std::string& selectedName()   const { return m_selectedName; }
    int                selectedPreset() const { return m_selectedPreset; }  // -1 = not a preset

    void setExoplanetCallback(std::function<void(const std::string&)> cb);
    void setExoplanetStatus(const std::string& status);

private:
    // ── Data types ────────────────────────────────────────────────────────────
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
        float       x, y;
        float       size;
        float       r, g, b;
        int         presetIdx;    // >=0 = apply preset; -1 = query via callback
        float       distanceLY;
    };

    struct ExplosionPart {
        float x, y;
        float vx, vy;
        float alpha;
        float size;
        float r, g, b;
    };

    // Particles that spring from the constellation line to the panel border
    struct TransPart {
        float x, y;       // current pos
        float tx, ty;     // border target
        float alpha;
        float size;
        float r, g, b;
    };

    // ── Private methods ───────────────────────────────────────────────────────
    void generateStars(float W, float H);
    void generatePlanets(float W, float H);
    void triggerExplosion();
    void triggerTransition(float W, float H);   // border-assemble animation
    void updateTransition(float dt);
    void drawTransitionParticles(ImDrawList* dl);
    void drawGlowLine(ImDrawList* dl, ImVec2 a, ImVec2 b, float alpha) const;
    void updateSearch();
    void pickHighlightStar(const std::string& planetName);

    float galaxyCX(float W) const;
    float galaxyCY(float H) const;

    // ── State ─────────────────────────────────────────────────────────────────
    std::vector<Star>         m_stars;
    std::vector<GalaxyPlanet> m_planets;

    int m_selectedIdx = 0;
    int m_hoveredIdx  = -1;

    // Search / filter
    char             m_searchBuf[256] = {};
    std::vector<int> m_searchMatches;

    // Star-scatter explosion
    std::vector<ExplosionPart> m_expParts;
    bool  m_exploding      = false;
    float m_explosionTimer = 0.f;
    bool  m_explosionDone  = false;

    // Border-assemble transition (plays simultaneously with explosion)
    std::vector<TransPart> m_transParts;
    bool  m_transitioning    = false;
    float m_transTimer       = 0.f;
    bool  m_transDone        = false;
    bool  m_borderAssembled  = false;  // true once particles have locked onto border
    bool  m_borderFading     = false;  // true after releaseBorder() is called
    float m_borderFadeStart  = 0.f;   // m_transTimer value when fade began

    float m_time        = 0.f;
    bool  m_initialized = false;

    // Logical screen dimensions (set at init, used for star highlight picking)
    float m_lastW = 0.f, m_lastH = 0.f;

    // Index into m_stars of the star highlighted for an exoplanet search result
    // (-1 = none)
    int m_highlightedStarIdx = -1;

    // Results exposed to Application
    std::string m_selectedName;
    int         m_selectedPreset = -1;

    // Exoplanet search
    std::function<void(const std::string&)> m_exoCallback;
    std::string m_exoStatus = "Search for an exoplanet...";
};

}  // namespace astrocore
