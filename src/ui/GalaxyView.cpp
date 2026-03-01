#include "ui/GalaxyView.hpp"
#include "render/PlanetParams.hpp"

#include <imgui.h>
#include <GLFW/glfw3.h>

#include <random>
#include <cmath>
#include <algorithm>
#include <cstring>

namespace astrocore {

// ── Constants ─────────────────────────────────────────────────────────────────
static constexpr float kExplosionDur  = 0.75f;
static constexpr int   kStarCount     = 380;
static constexpr float kGlowLineOuter = 7.f;
static constexpr float kGlowLineCore  = 1.3f;

// Sidebar occupies the left kSidebarW px; galaxy spans the whole screen behind it.
static constexpr float kSidebarW = 340.f;
static constexpr float kSidebarX =  10.f;

// ── Helpers ───────────────────────────────────────────────────────────────────
static ImU32 col32f(float r, float g, float b, float a) {
    return IM_COL32(
        static_cast<int>(std::clamp(r, 0.f, 1.f) * 255),
        static_cast<int>(std::clamp(g, 0.f, 1.f) * 255),
        static_cast<int>(std::clamp(b, 0.f, 1.f) * 255),
        static_cast<int>(std::clamp(a, 0.f, 1.f) * 255));
}

// ── GalaxyView ────────────────────────────────────────────────────────────────

GalaxyView::GalaxyView() = default;

// Galaxy is centered on the full screen so stars bleed beautifully behind the
// transparent sidebar glass.
float GalaxyView::galaxyCX(float W) const { return W * 0.5f; }
float GalaxyView::galaxyCY(float H) const { return H * 0.5f; }

void GalaxyView::init(float W, float H) {
    m_lastW = W; m_lastH = H;
    generateStars(W, H);
    generatePlanets(W, H);
    m_initialized        = true;
    m_exploding          = false;
    m_explosionDone      = false;
    m_transitioning      = false;
    m_transTimer         = 0.f;
    m_transDone          = false;
    m_borderAssembled    = false;
    m_borderFading       = false;
    m_borderFadeStart    = 0.f;
    m_transParts.clear();
    m_selectedIdx        = 0;
    m_highlightedStarIdx = -1;
    m_time               = 0.f;
    m_searchBuf[0]       = '\0';
    m_searchMatches.clear();
}

void GalaxyView::reset() {
    m_exploding        = false;
    m_explosionDone    = false;
    m_explosionTimer   = 0.f;
    m_expParts.clear();
    m_transitioning    = false;
    m_transTimer       = 0.f;
    m_transDone        = false;
    m_borderAssembled  = false;
    m_borderFading     = false;
    m_borderFadeStart  = 0.f;
    m_transParts.clear();
    m_time = 0.f;
}

// ── Star + planet generation ──────────────────────────────────────────────────

void GalaxyView::generateStars(float W, float H) {
    m_stars.clear();
    m_stars.reserve(static_cast<size_t>(kStarCount));

    std::mt19937 rng(42);
    auto frand = [&](float lo, float hi) -> float {
        return lo + std::uniform_real_distribution<float>(0.f, 1.f)(rng) * (hi - lo);
    };

    const float cx = galaxyCX(W);
    const float cy = galaxyCY(H);
    // Semi-axes of the galaxy disk (fills most of the screen)
    const float rx = W * 0.44f;
    const float ry = H * 0.38f;

    for (int i = 0; i < kStarCount; ++i) {
        float angle = frand(0.f, 6.2832f);
        float dist;
        if (i < kStarCount * 0.6f)
            dist = frand(0.f, 1.f) * frand(0.f, 1.f);  // concentrated core
        else
            dist = std::sqrt(frand(0.f, 1.f));           // uniform area halo

        Star s;
        s.x = cx + std::cos(angle) * dist * rx;
        s.y = cy + std::sin(angle) * dist * ry;
        s.size         = frand(0.4f, 2.2f);
        s.twinklePhase = frand(0.f, 6.28f);
        s.twinkleSpeed = frand(0.4f, 2.5f);

        float roll = frand(0.f, 1.f);
        if (roll < 0.70f) {
            s.r = frand(0.75f, 0.95f); s.g = frand(0.80f, 0.95f); s.b = frand(0.88f, 1.00f);
        } else if (roll < 0.88f) {
            s.r = frand(0.90f, 1.00f); s.g = frand(0.82f, 0.95f); s.b = frand(0.55f, 0.72f);
        } else {
            s.r = frand(0.45f, 0.65f); s.g = frand(0.60f, 0.80f); s.b = frand(0.88f, 1.00f);
        }
        m_stars.push_back(s);
    }
}

void GalaxyView::generatePlanets(float W, float H) {
    m_planets.clear();

    const float cx = galaxyCX(W);
    const float cy = galaxyCY(H);

    // All planets must be visible past the sidebar (x > kSidebarX + kSidebarW + margin).
    // Offsets are from the true screen center (cx, cy).
    // For W=1280, cx=640: minimum dx ≈ kSidebarX+kSidebarW+20 - 640 = -260.
    struct PlanetDef {
        const char* name;
        const char* typeStr;
        float dx, dy;
        float size;
        float r, g, b;
        int   presetIdx;
        float distLY;
    };

    static const PlanetDef defs[] = {
        // ── Preset planets ────────────────────────────────────────────────────
        { "Earth",         "Ocean-Rock",    10.f,   15.f, 7.5f, 0.28f, 0.60f, 1.00f,  0,    0.f },
        { "Mars",          "Desert-Rock",  155.f,  -35.f, 6.5f, 0.90f, 0.42f, 0.18f,  1,    0.f },
        { "Lava World",    "Volcanic",     -55.f,   70.f, 6.0f, 1.00f, 0.22f, 0.04f,  2,  120.f },
        { "Ice World",     "Frozen",       270.f,  -75.f, 6.5f, 0.72f, 0.88f, 1.00f,  3,   88.f },
        { "Gas Giant",     "Gas Giant",   -130.f,  -50.f, 9.0f, 0.82f, 0.62f, 0.30f,  4,   14.f },
        { "Ocean World",   "Ocean",        170.f,   90.f, 7.0f, 0.08f, 0.28f, 0.90f,  5,  310.f },
        { "Desert",        "Arid",          30.f, -100.f, 6.0f, 0.95f, 0.80f, 0.38f,  6,  240.f },
        { "Alien",         "Exotic",       320.f,   55.f, 7.0f, 0.60f, 0.18f, 0.92f,  7, 1220.f },
        // ── Exoplanet stubs ───────────────────────────────────────────────────
        { "Kepler-452 b",  "Super-Earth", -175.f,   25.f, 4.5f, 0.70f, 0.75f, 0.85f, -1, 1402.f },
        { "Proxima Cen b", "Rocky",        385.f,  -60.f, 4.0f, 0.60f, 0.70f, 0.80f, -1,    4.2f},
        { "TRAPPIST-1e",   "Rocky",        -75.f,  -95.f, 4.0f, 0.65f, 0.78f, 0.88f, -1,   39.f },
        { "HD 209458 b",   "Hot Jupiter",  250.f,  130.f, 5.5f, 0.85f, 0.65f, 0.40f, -1,  159.f },
    };

    for (auto& d : defs) {
        GalaxyPlanet p;
        p.name       = d.name;
        p.typeStr    = d.typeStr;
        p.x          = cx + d.dx;
        p.y          = cy + d.dy;
        p.size       = d.size;
        p.r = d.r; p.g = d.g; p.b = d.b;
        p.presetIdx  = d.presetIdx;
        p.distanceLY = d.distLY;
        m_planets.push_back(p);
    }

    if (!m_planets.empty()) {
        m_selectedName   = m_planets[0].name;
        m_selectedPreset = m_planets[0].presetIdx;
    }
}

// ── Star highlight (exoplanet search placeholder) ─────────────────────────────
// Pick a deterministic star near the galaxy centre to highlight when an
// exoplanet is selected via search (placeholder until real positions are known).

void GalaxyView::pickHighlightStar(const std::string& planetName) {
    if (m_stars.empty()) { m_highlightedStarIdx = -1; return; }

    const float cx = galaxyCX(m_lastW);
    const float cy = galaxyCY(m_lastH);
    const float maxDist = m_lastW * 0.12f;  // inner ~12 % of screen width

    std::vector<int> central;
    central.reserve(64);
    for (int si = 0; si < static_cast<int>(m_stars.size()); ++si) {
        float dx = m_stars[static_cast<size_t>(si)].x - cx;
        float dy = m_stars[static_cast<size_t>(si)].y - cy;
        if (dx * dx + dy * dy < maxDist * maxDist)
            central.push_back(si);
    }

    if (central.empty()) { m_highlightedStarIdx = -1; return; }
    size_t hash = std::hash<std::string>{}(planetName);
    m_highlightedStarIdx = central[hash % central.size()];
}

// ── Search ────────────────────────────────────────────────────────────────────

void GalaxyView::updateSearch() {
    m_searchMatches.clear();
    if (m_searchBuf[0] == '\0') return;

    std::string q(m_searchBuf);
    std::transform(q.begin(), q.end(), q.begin(), ::tolower);

    for (int i = 0; i < static_cast<int>(m_planets.size()); ++i) {
        std::string name = m_planets[static_cast<size_t>(i)].name;
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        if (name.find(q) != std::string::npos)
            m_searchMatches.push_back(i);
    }
}

// ── Update ────────────────────────────────────────────────────────────────────
// MUST be called after ImGui::NewFrame() so input queries are valid.

void GalaxyView::update(float dt, float W, float H) {
    m_time += dt;

    if (m_exploding) {
        m_explosionTimer += dt;
        for (auto& p : m_expParts) {
            p.x    += p.vx * dt;
            p.y    += p.vy * dt;
            p.alpha = std::max(0.f, 1.f - m_explosionTimer / kExplosionDur);
        }
        if (m_explosionTimer >= kExplosionDur)
            m_explosionDone = true;
        // Don't return — also tick the transition animation below
    }
    if (m_transitioning)
        updateTransition(dt);

    if (m_exploding || m_transitioning) return;

    // Arrow keys cycle planets (only when ImGui isn't capturing keyboard)
    if (!ImGui::GetIO().WantCaptureKeyboard && !m_planets.empty()) {
        int delta = 0;
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true) ||
            ImGui::IsKeyPressed(ImGuiKey_DownArrow,  true)) delta =  1;
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow,  true) ||
            ImGui::IsKeyPressed(ImGuiKey_UpArrow,    true)) delta = -1;

        if (delta != 0) {
            m_selectedIdx = (m_selectedIdx + delta + static_cast<int>(m_planets.size()))
                            % static_cast<int>(m_planets.size());
            m_selectedName       = m_planets[static_cast<size_t>(m_selectedIdx)].name;
            m_selectedPreset     = m_planets[static_cast<size_t>(m_selectedIdx)].presetIdx;
            m_highlightedStarIdx = -1;
        }
    }

    // Mouse hover + click on planet dots
    m_hoveredIdx = -1;
    ImVec2 mp = ImGui::GetMousePos();
    for (int i = 0; i < static_cast<int>(m_planets.size()); ++i) {
        auto& p  = m_planets[static_cast<size_t>(i)];
        float dx = mp.x - p.x, dy = mp.y - p.y;
        if (dx * dx + dy * dy < (p.size + 8.f) * (p.size + 8.f)) {
            m_hoveredIdx = i;
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
                !ImGui::GetIO().WantCaptureMouse) {
                m_selectedIdx        = i;
                m_selectedName       = p.name;
                m_selectedPreset     = p.presetIdx;
                m_highlightedStarIdx = -1;
            }
            break;
        }
    }
}

// ── Explosion ────────────────────────────────────────────────────────────────

void GalaxyView::triggerExplosion() {
    m_expParts.clear();

    const float ox = m_planets.empty() ? 640.f : m_planets[static_cast<size_t>(m_selectedIdx)].x;
    const float oy = m_planets.empty() ? 360.f : m_planets[static_cast<size_t>(m_selectedIdx)].y;

    std::mt19937 rng(1337);
    auto frand = [&](float lo, float hi) -> float {
        return lo + std::uniform_real_distribution<float>(0.f, 1.f)(rng) * (hi - lo);
    };

    auto addPart = [&](float x, float y, float r, float g, float b, float sz) {
        float dx = x - ox, dy = y - oy;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.01f) { dx = frand(-1.f, 1.f); dy = frand(-1.f, 1.f); len = 1.f; }
        float spd = frand(300.f, 1100.f);
        ExplosionPart p;
        p.x = x; p.y = y;
        p.vx = (dx / len) * spd; p.vy = (dy / len) * spd;
        p.alpha = 1.f; p.size = sz;
        p.r = r; p.g = g; p.b = b;
        m_expParts.push_back(p);
    };

    for (auto& s : m_stars)   addPart(s.x, s.y, s.r, s.g, s.b, s.size);
    for (auto& p : m_planets) addPart(p.x, p.y, p.r, p.g, p.b, p.size * 0.7f);

    m_exploding      = true;
    m_explosionTimer = 0.f;
    m_explosionDone  = false;

    // Simultaneously animate the constellation line particles to the panel border
    if (m_lastW > 0.f && m_lastH > 0.f)
        triggerTransition(m_lastW, m_lastH);
}

// ── Draw helpers ──────────────────────────────────────────────────────────────

void GalaxyView::drawGlowLine(ImDrawList* dl, ImVec2 a, ImVec2 b, float alpha) const {
    dl->AddLine(a, b, IM_COL32(100, 180, 255, static_cast<int>(28 * alpha)), kGlowLineOuter);
    dl->AddLine(a, b, IM_COL32(160, 210, 255, static_cast<int>(55 * alpha)), 3.0f);
    dl->AddLine(a, b, IM_COL32(220, 240, 255, static_cast<int>(130 * alpha)), kGlowLineCore);
}

// ── renderBackground ─────────────────────────────────────────────────────────

void GalaxyView::renderBackground(ImDrawList* dl, float W, float H) {
    if (!m_initialized) return;

    if (m_exploding || m_transitioning) {
        // Star-scatter particles
        if (m_exploding) {
            for (auto& p : m_expParts) {
                if (p.alpha > 0.f)
                    dl->AddCircleFilled({ p.x, p.y }, p.size, col32f(p.r, p.g, p.b, p.alpha));
            }
        }
        // Border-assemble particles (draw on top)
        if (m_transitioning)
            drawTransitionParticles(dl);
        return;
    }

    // Background stars (twinkle); highlighted star gets a pulsing exoplanet glow
    for (int si = 0; si < static_cast<int>(m_stars.size()); ++si) {
        auto& s  = m_stars[static_cast<size_t>(si)];
        float tw = 0.55f + 0.45f * std::sin(m_time * s.twinkleSpeed + s.twinklePhase);

        if (si == m_highlightedStarIdx) {
            float p = 0.65f + 0.35f * std::sin(m_time * 2.6f);
            dl->AddCircleFilled({ s.x, s.y }, s.size * 7.f,
                col32f(0.25f, 0.55f, 1.0f, 0.12f * p));
            dl->AddCircleFilled({ s.x, s.y }, s.size * 4.f,
                col32f(0.45f, 0.75f, 1.0f, 0.30f * p));
            dl->AddCircleFilled({ s.x, s.y }, s.size * 2.f,
                col32f(0.80f, 0.92f, 1.0f, 0.85f * p));
            // bright core
            dl->AddCircleFilled({ s.x, s.y }, s.size,
                col32f(1.0f, 1.0f, 1.0f, 1.0f));
        } else {
            dl->AddCircleFilled({ s.x, s.y }, s.size,
                col32f(s.r, s.g, s.b, 0.50f + 0.50f * tw));
        }
    }

    // Planet dots
    for (int i = 0; i < static_cast<int>(m_planets.size()); ++i) {
        auto&      p     = m_planets[static_cast<size_t>(i)];
        const bool isSel = (i == m_selectedIdx);
        const bool isHov = (i == m_hoveredIdx);
        const float pulse = isSel ? (0.75f + 0.25f * std::sin(m_time * 2.2f)) : 1.0f;

        if (isSel || isHov) {
            float ga = isSel ? 0.30f * pulse : 0.16f;
            dl->AddCircleFilled({ p.x, p.y }, p.size * 3.6f, col32f(p.r * 0.8f, p.g * 0.8f, p.b, ga));
            dl->AddCircleFilled({ p.x, p.y }, p.size * 2.2f, col32f(p.r, p.g, p.b, ga * 1.8f));
        }

        dl->AddCircleFilled({ p.x, p.y }, p.size * pulse, col32f(p.r, p.g, p.b, isSel ? 1.0f : 0.80f));
        dl->AddCircleFilled({ p.x - p.size * 0.28f, p.y - p.size * 0.28f },
            p.size * 0.30f, IM_COL32(255, 255, 255, isSel ? 200 : 120));
    }

    // Constellation connector: planet → diagonal → star-node dot → horizontal → card
    if (!m_planets.empty()) {
        auto& sel   = m_planets[static_cast<size_t>(m_selectedIdx)];
        const float cardX = W - 280.f;
        const float cardY = 35.f;
        float pulse = 0.6f + 0.4f * std::sin(m_time * 1.5f);

        // Node placed just left of the card, at a height between the planet and
        // the card's vertical midpoint.
        float nodeX = cardX - 42.f;
        float nodeY = sel.y * 0.38f + (cardY + 100.f) * 0.62f;

        // Segment 1: diagonal from planet to node
        drawGlowLine(dl, { sel.x, sel.y }, { nodeX, nodeY }, pulse);

        // Junction: a pulsing star dot (constellation vertex)
        float dotR = 3.0f + 1.2f * std::sin(m_time * 2.3f);
        dl->AddCircleFilled({ nodeX, nodeY }, dotR + 5.f,
            col32f(0.40f, 0.70f, 1.00f, 0.18f * pulse));
        dl->AddCircleFilled({ nodeX, nodeY }, dotR + 2.5f,
            col32f(0.60f, 0.85f, 1.00f, 0.45f * pulse));
        dl->AddCircleFilled({ nodeX, nodeY }, dotR,
            col32f(0.92f, 0.96f, 1.00f, 0.90f * pulse));

        // Segment 2: horizontal from node to card left edge
        drawGlowLine(dl, { nodeX, nodeY }, { cardX, nodeY }, pulse);
    }
}

// ── renderUI ─────────────────────────────────────────────────────────────────

bool GalaxyView::renderUI(float W, float H) {
    if (!m_initialized || m_exploding) return false;
    bool expandPressed = false;

    // ── Left sidebar ──────────────────────────────────────────────────────────
    ImGui::SetNextWindowPos({ kSidebarX, 10.f }, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ kSidebarW, H - 20.f }, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(-1.f);

    ImGuiWindowFlags sidebarFlags =
        ImGuiWindowFlags_NoDecoration        |
        ImGuiWindowFlags_NoMove              |
        ImGuiWindowFlags_NoSavedSettings     |
        ImGuiWindowFlags_NoScrollWithMouse   |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    if (ImGui::Begin("##galaxy_sidebar", nullptr, sidebarFlags)) {

        // ── Header ────────────────────────────────────────────────────────────
        ImGui::Spacing();
        ImGui::TextDisabled("ASTRODEX  //  GALAXY VIEW");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // ── Search + live dropdown ─────────────────────────────────────────
        ImGui::TextDisabled("Search Exoplanets");
        ImGui::SetNextItemWidth(-1.f);
        bool changed = ImGui::InputText("##search", m_searchBuf, sizeof(m_searchBuf));
        if (changed) updateSearch();

        // Results drop straight down from the input field
        if (m_searchBuf[0] != '\0') {
            if (!m_searchMatches.empty()) {
                float dropH = std::min<float>(
                    static_cast<float>(m_searchMatches.size()) * 22.f + 8.f, 130.f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 6.f, 4.f });
                if (ImGui::BeginChild("##drop", { -1.f, dropH }, true,
                        ImGuiWindowFlags_NoScrollbar)) {
                    for (int idx : m_searchMatches) {
                        auto& planet  = m_planets[static_cast<size_t>(idx)];
                        bool  isSel   = (idx == m_selectedIdx);
                        if (ImGui::Selectable(planet.name.c_str(), isSel)) {
                            m_selectedIdx    = idx;
                            m_selectedName   = planet.name;
                            m_selectedPreset = planet.presetIdx;
                            std::memset(m_searchBuf, 0, sizeof(m_searchBuf));
                            m_searchMatches.clear();
                            // For exoplanets highlight a random central star
                            // (placeholder until real galaxy positions are known)
                            if (planet.presetIdx < 0)
                                pickHighlightStar(planet.name);
                            else
                                m_highlightedStarIdx = -1;
                        }
                    }
                }
                ImGui::EndChild();
                ImGui::PopStyleVar();
            } else {
                ImGui::TextDisabled("  No matches");
            }
        }

        // ── Solar System planets — pinned to bottom ───────────────────────────
        // Only show preset (solar system) bodies here; exoplanet stubs are
        // discovered exclusively via the search bar above.
        int solarCount = 0;
        for (auto& p : m_planets)
            if (p.presetIdx >= 0) ++solarCount;

        const float rowH          = 22.f;
        const float sectionTopPad = 28.f;
        float       sectionH      = sectionTopPad
                                    + static_cast<float>(solarCount) * rowH
                                    + 6.f;

        float windowH  = ImGui::GetWindowSize().y;
        float targetY  = windowH - sectionH - ImGui::GetStyle().WindowPadding.y;
        float currentY = ImGui::GetCursorPosY();
        if (targetY > currentY + 4.f)
            ImGui::SetCursorPosY(targetY);

        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextDisabled("Solar System");
        ImGui::Spacing();

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 6.f, 4.f });
        for (int i = 0; i < static_cast<int>(m_planets.size()); ++i) {
            auto& planet = m_planets[static_cast<size_t>(i)];
            if (planet.presetIdx < 0) continue;   // skip exoplanet stubs

            bool  isSel  = (i == m_selectedIdx);

            if (isSel) {
                ImGui::PushStyleColor(ImGuiCol_Header,
                    ImVec4(planet.r * 0.4f, planet.g * 0.4f, planet.b * 0.4f, 0.65f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                    ImVec4(planet.r * 0.5f, planet.g * 0.5f, planet.b * 0.5f, 0.75f));
            }

            ImVec2 cp = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddCircleFilled(
                { cp.x + 7.f, cp.y + 9.f }, 5.f,
                col32f(planet.r, planet.g, planet.b, isSel ? 1.f : 0.7f));
            ImGui::SetCursorScreenPos({ cp.x + 18.f, cp.y });

            if (ImGui::Selectable(planet.name.c_str(), isSel,
                    ImGuiSelectableFlags_None, { 0.f, 18.f })) {
                m_selectedIdx        = i;
                m_selectedName       = planet.name;
                m_selectedPreset     = planet.presetIdx;
                m_highlightedStarIdx = -1;
            }

            if (isSel) ImGui::PopStyleColor(2);
        }
        ImGui::PopStyleVar();
    }
    ImGui::End();

    // ── Fact file card (upper right) ──────────────────────────────────────────
    if (!m_planets.empty()) {
        auto& sel   = m_planets[static_cast<size_t>(m_selectedIdx)];
        float cardX = W - 280.f;
        float cardY = 35.f;

        ImGui::SetNextWindowPos({ cardX, cardY }, ImGuiCond_Always);
        ImGui::SetNextWindowSize({ 265.f, 205.f }, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(-1.f);

        if (ImGui::Begin("##factcard", nullptr,
                ImGuiWindowFlags_NoDecoration        |
                ImGuiWindowFlags_NoMove              |
                ImGuiWindowFlags_NoSavedSettings     |
                ImGuiWindowFlags_NoBringToFrontOnFocus)) {

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text,
                ImVec4(sel.r * 0.85f + 0.15f, sel.g * 0.85f + 0.15f, sel.b * 0.85f + 0.15f, 1.f));
            ImGui::Text("%s", sel.name.c_str());
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::TextDisabled("Type");     ImGui::SameLine(80.f); ImGui::Text("%s", sel.typeStr.c_str());
            ImGui::TextDisabled("Distance"); ImGui::SameLine(80.f);
            if (sel.presetIdx >= 0 && sel.distanceLY <= 0.f)
                ImGui::Text("—");
            else
                ImGui::Text("%.1f ly", sel.distanceLY);
            ImGui::TextDisabled("Source");   ImGui::SameLine(80.f);
            ImGui::Text("%s", sel.presetIdx >= 0 ? "Preset" : "NASA Archive");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Button,
                ImVec4(sel.r * 0.25f, sel.g * 0.25f, sel.b * 0.35f, 0.80f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                ImVec4(sel.r * 0.40f, sel.g * 0.40f, sel.b * 0.50f, 0.90f));
            if (ImGui::Button("  EXPAND  \xe2\x96\xb6", { -1.f, 32.f })) {
                expandPressed = true;
                triggerExplosion();
            }
            ImGui::PopStyleColor(2);
        }
        ImGui::End();
    }

    return expandPressed;
}

// ── Border-assemble transition ────────────────────────────────────────────────
// Particles seeded along the constellation line spring to the planet-editor
// panel border, glow for a beat, then fade — signalling the screen switch.

void GalaxyView::triggerTransition(float W, float H) {
    m_transParts.clear();

    if (m_planets.empty()) { m_transDone = true; return; }

    auto& sel    = m_planets[static_cast<size_t>(m_selectedIdx)];
    float cardX  = W - 280.f;
    float cardY  = 35.f;
    float nodeX  = cardX - 42.f;
    float nodeY  = sel.y * 0.38f + (cardY + 100.f) * 0.62f;

    // Panel border — must match UIManager::render() SetNextWindowPos/Size exactly.
    const float pX = 10.f, pY = 10.f, pW = 340.f, pH = H - 20.f;
    const float rr = 10.f;   // WindowRounding

    // Perimeter helper
    float sTop   = (pX + pW - rr) - (pX + rr);
    float sRight = (pY + pH - rr) - (pY + rr);
    float arcLen = rr * 1.5708f;
    float perim  = 2.f * (sTop + sRight) + 4.f * arcLen;

    const int N = 160;
    std::mt19937 rng(77);
    auto frand = [&](float lo, float hi) {
        return lo + std::uniform_real_distribution<float>(0.f, 1.f)(rng) * (hi - lo);
    };

    // Lengths of the two constellation segments
    float seg1Len = std::sqrt((nodeX - sel.x) * (nodeX - sel.x) +
                              (nodeY - sel.y) * (nodeY - sel.y));
    float seg2Len = std::abs(cardX - nodeX);
    float totalLen = seg1Len + seg2Len;

    for (int i = 0; i < N; ++i) {
        float t = static_cast<float>(i) / N;

        // ── Start: along the two constellation segments ──────────────────
        float sx, sy;
        float d = t * totalLen;
        if (d < seg1Len) {
            float u = d / seg1Len;
            sx = sel.x + (nodeX - sel.x) * u;
            sy = sel.y + (nodeY - sel.y) * u;
        } else {
            float u = (d - seg1Len) / seg2Len;
            sx = nodeX + (cardX - nodeX) * u;
            sy = nodeY;
        }

        // ── Target: evenly around the rounded-rect border ────────────────
        float bt = t * perim;
        float tx, ty;
        float x0 = pX, y0 = pY, x1 = pX + pW, y1 = pY + pH;
        if (bt < sTop) {
            tx = x0 + rr + bt; ty = y0;
        } else if ((bt -= sTop) < arcLen) {
            float a = -1.5708f + (bt / arcLen) * 1.5708f;
            tx = (x1 - rr) + std::cos(a) * rr; ty = (y0 + rr) + std::sin(a) * rr;
        } else if ((bt -= arcLen) < sRight) {
            tx = x1; ty = y0 + rr + bt;
        } else if ((bt -= sRight) < arcLen) {
            float a = (bt / arcLen) * 1.5708f;
            tx = (x1 - rr) + std::cos(a) * rr; ty = (y1 - rr) + std::sin(a) * rr;
        } else if ((bt -= arcLen) < sTop) {
            tx = x1 - rr - bt; ty = y1;
        } else if ((bt -= sTop) < arcLen) {
            float a = 1.5708f + (bt / arcLen) * 1.5708f;
            tx = (x0 + rr) + std::cos(a) * rr; ty = (y1 - rr) + std::sin(a) * rr;
        } else if ((bt -= arcLen) < sRight) {
            tx = x0; ty = y1 - rr - bt;
        } else {
            bt -= sRight;
            float a = 3.14159f + (bt / arcLen) * 1.5708f;
            tx = (x0 + rr) + std::cos(a) * rr; ty = (y0 + rr) + std::sin(a) * rr;
        }

        TransPart p;
        p.x     = sx + frand(-4.f, 4.f);
        p.y     = sy + frand(-4.f, 4.f);
        p.tx    = tx;
        p.ty    = ty;
        p.alpha = 1.f;
        p.size  = frand(1.6f, 3.2f);
        float cr = frand(0.f, 1.f);
        if      (cr < 0.55f) { p.r = 0.45f; p.g = 0.76f; p.b = 1.00f; }
        else if (cr < 0.82f) { p.r = 0.68f; p.g = 0.90f; p.b = 1.00f; }
        else                 { p.r = 0.90f; p.g = 0.96f; p.b = 1.00f; }
        m_transParts.push_back(p);
    }

    m_transitioning = true;
    m_transTimer    = 0.f;
    m_transDone     = false;
}

void GalaxyView::updateTransition(float dt) {
    if (!m_transitioning) return;
    m_transTimer += dt;

    constexpr float kAssemble  = 0.60f;   // particles spring to border
    constexpr float kFadeDur   = 0.35f;   // fade-out duration after releaseBorder()

    if (m_transTimer < kAssemble) {
        // Spring toward border targets
        float spring = 5.f + m_transTimer * 7.f;
        for (auto& p : m_transParts) {
            p.x += (p.tx - p.x) * spring * dt;
            p.y += (p.ty - p.y) * spring * dt;
        }
    } else if (!m_borderFading) {
        // Particles have assembled — lock on border and hold indefinitely
        // until releaseBorder() is called by the Application.
        m_borderAssembled = true;
        for (auto& p : m_transParts) {
            p.x = p.tx; p.y = p.ty; p.alpha = 1.f;
        }
    } else {
        // releaseBorder() was called — fade out and finish
        float f = (m_transTimer - m_borderFadeStart) / kFadeDur;
        for (auto& p : m_transParts) {
            p.x = p.tx; p.y = p.ty;
            p.alpha = std::max(0.f, 1.f - f);
        }
        if (f >= 1.f) {
            m_transitioning = false;
            m_transDone     = true;
        }
    }
}

void GalaxyView::releaseBorder() {
    if (m_transitioning && !m_borderFading) {
        m_borderFading    = true;
        m_borderFadeStart = m_transTimer;
    }
}

void GalaxyView::drawTransitionParticles(ImDrawList* dl) {
    // While particles are locked on the border (assembled, not yet fading),
    // use a gentle breathing glow so the border looks alive.
    bool  assembled  = (m_borderAssembled && !m_borderFading);
    float glowScale  = assembled
        ? (1.f + 0.55f * std::sin(m_transTimer * 5.0f))
        : 1.f;

    for (auto& p : m_transParts) {
        if (p.alpha <= 0.01f) continue;
        float sz = p.size * glowScale;
        // Outer halo
        dl->AddCircleFilled({ p.x, p.y }, sz + 4.f,
            col32f(p.r * 0.45f, p.g * 0.60f, p.b, 0.14f * p.alpha));
        // Mid glow
        dl->AddCircleFilled({ p.x, p.y }, sz + 1.8f,
            col32f(p.r * 0.75f, p.g * 0.85f, p.b, 0.35f * p.alpha));
        // Core
        dl->AddCircleFilled({ p.x, p.y }, sz,
            col32f(p.r, p.g, p.b, 0.92f * p.alpha));
    }
}

// ── Callbacks ─────────────────────────────────────────────────────────────────

void GalaxyView::setExoplanetCallback(std::function<void(const std::string&)> cb) {
    m_exoCallback = std::move(cb);
}

void GalaxyView::setExoplanetStatus(const std::string& status) {
    m_exoStatus = status;
}

}  // namespace astrocore
