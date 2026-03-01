#include "ui/GalaxyView.hpp"
#include "render/ExoplanetConverter.hpp"

#include <imgui.h>
#include <GLFW/glfw3.h>

#include <random>
#include <cmath>
#include <algorithm>
#include <cstring>

namespace astrocore {

static constexpr float kExplosionDur  = 0.75f;
static constexpr int   kStarCount     = 380;
static constexpr float kGlowLineOuter = 7.f;
static constexpr float kGlowLineCore  = 1.3f;
static constexpr float kSidebarW = 360.f;
static constexpr float kSidebarX = 10.f;

static ImU32 col32f(float r, float g, float b, float a) {
    return IM_COL32(
        static_cast<int>(std::clamp(r, 0.f, 1.f) * 255),
        static_cast<int>(std::clamp(g, 0.f, 1.f) * 255),
        static_cast<int>(std::clamp(b, 0.f, 1.f) * 255),
        static_cast<int>(std::clamp(a, 0.f, 1.f) * 255));
}

GalaxyView::GalaxyView() = default;

float GalaxyView::galaxyCX(float W) const { return W * 0.5f; }
float GalaxyView::galaxyCY(float H) const { return H * 0.5f; }

void GalaxyView::init(float W, float H) {
    m_lastW = W; m_lastH = H;
    generateStars(W, H);
    generatePresetPlanets(W, H);
    loadCachedPlanets();
    assignPlanetPositions(W, H);

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
    updateSearch();
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

void GalaxyView::generateStars(float W, float H) {
    m_stars.clear();
    m_stars.reserve(static_cast<size_t>(kStarCount));

    std::mt19937 rng(42);
    auto frand = [&](float lo, float hi) -> float {
        return lo + std::uniform_real_distribution<float>(0.f, 1.f)(rng) * (hi - lo);
    };

    const float cx = galaxyCX(W);
    const float cy = galaxyCY(H);
    const float rx = W * 0.44f;
    const float ry = H * 0.38f;

    for (int i = 0; i < kStarCount; ++i) {
        float angle = frand(0.f, 6.2832f);
        float dist;
        if (static_cast<float>(i) < static_cast<float>(kStarCount) * 0.6f)
            dist = frand(0.f, 1.f) * frand(0.f, 1.f);
        else
            dist = std::sqrt(frand(0.f, 1.f));

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

void GalaxyView::generatePresetPlanets(float /*W*/, float /*H*/) {
    m_planets.clear();

    // Real solar system planet presets
    struct PresetDef {
        const char* name;
        const char* typeStr;
        int presetIdx;
        float r, g, b;
    };
    static const PresetDef presets[] = {
        { "Earth",   "Terrestrial",  0, 0.30f, 0.55f, 0.90f },
        { "Mars",    "Desert",       1, 0.85f, 0.45f, 0.25f },
        { "Venus",   "Volcanic",     2, 0.90f, 0.70f, 0.40f },
        { "Mercury", "Rocky",        3, 0.50f, 0.48f, 0.45f },
        { "Jupiter", "Gas Giant",    4, 0.90f, 0.80f, 0.60f },
        { "Saturn",  "Gas Giant",    5, 0.95f, 0.85f, 0.65f },
        { "Neptune", "Ice Giant",    6, 0.25f, 0.45f, 0.85f },
        { "Uranus",  "Ice Giant",    7, 0.55f, 0.80f, 0.85f },
    };

    for (auto& d : presets) {
        GalaxyPlanet p;
        p.name       = d.name;
        p.typeStr    = d.typeStr;
        p.hostStar   = "Sol";
        p.presetIdx  = d.presetIdx;
        p.distanceLY = 0.f;
        p.radiusEarth = 1.0f;
        p.massEarth   = 1.0f;
        p.tempK       = 288.f;
        p.isCached    = false;
        p.x = p.y = 0.f;
        p.size = 6.0f;
        p.r = d.r; p.g = d.g; p.b = d.b;
        m_planets.push_back(p);
    }
}

void GalaxyView::loadCachedPlanets() {
    auto cached = ExoplanetConverter::listCachedPlanets();

    for (const auto& info : cached) {
        // Check if already exists (avoid duplicates)
        bool exists = false;
        for (const auto& p : m_planets) {
            if (p.name == info.name) { exists = true; break; }
        }
        if (exists) continue;

        GalaxyPlanet p;
        p.name        = info.name;
        p.typeStr     = info.type;
        p.presetIdx   = -1;  // Not a preset
        p.isCached    = true;
        p.x = p.y = 0.f;
        p.size = 4.0f;

        // Metadata will be fetched from NASA API when needed
        // For now, parse host star from planet name (common format: "HostStar b")
        p.hostStar    = "";
        p.distanceLY  = 0.f;
        p.radiusEarth = 0.f;
        p.massEarth   = 0.f;
        p.tempK       = 0.f;

        size_t lastSpace = info.name.rfind(' ');
        if (lastSpace != std::string::npos && lastSpace > 0) {
            p.hostStar = info.name.substr(0, lastSpace);
        }

        // Color based on type
        if (info.type.find("Gas") != std::string::npos ||
            info.type.find("Jupiter") != std::string::npos) {
            p.r = 0.85f; p.g = 0.65f; p.b = 0.40f;
        } else if (info.type.find("Neptune") != std::string::npos ||
                   info.type.find("Ice") != std::string::npos) {
            p.r = 0.40f; p.g = 0.70f; p.b = 1.00f;
        } else if (info.type.find("Lava") != std::string::npos) {
            p.r = 1.00f; p.g = 0.30f; p.b = 0.10f;
        } else if (info.type.find("Ocean") != std::string::npos) {
            p.r = 0.20f; p.g = 0.50f; p.b = 0.90f;
        } else if (info.type.find("Super") != std::string::npos ||
                   info.type.find("Earth") != std::string::npos ||
                   info.type.find("Terrestrial") != std::string::npos) {
            p.r = 0.40f; p.g = 0.80f; p.b = 0.40f;
        } else {
            p.r = 0.65f; p.g = 0.75f; p.b = 0.85f;
        }

        m_planets.push_back(p);
    }

    // Update filtered indices
    updateSearch();
}

void GalaxyView::assignPlanetPositions(float W, float H) {
    const float cx = galaxyCX(W);
    const float cy = galaxyCY(H);

    std::mt19937 rng(12345);
    auto frand = [&](float lo, float hi) {
        return lo + std::uniform_real_distribution<float>(0.f, 1.f)(rng) * (hi - lo);
    };

    // Place preset planets in visible area (right of sidebar)
    float presetX = kSidebarX + kSidebarW + 80.f;
    float presetY = 80.f;
    int presetCount = 0;

    for (auto& p : m_planets) {
        if (p.presetIdx == -2) {
            // Solar System - prominent position top center-right
            p.x = presetX + 120.f;
            p.y = presetY - 40.f;
            p.size = 12.0f;
        } else if (p.presetIdx >= 0) {
            // Preset planets in a grid on the right
            p.x = presetX + static_cast<float>(presetCount % 4) * 80.f;
            p.y = presetY + static_cast<float>(presetCount / 4) * 80.f;  // NOLINT(bugprone-integer-division)
            p.size = 7.0f;
            presetCount++;
        } else {
            // Exoplanets scattered across galaxy
            float angle = frand(0.f, 6.2832f);
            float dist = frand(0.2f, 0.8f);
            p.x = cx + std::cos(angle) * dist * (W * 0.35f);
            p.y = cy + std::sin(angle) * dist * (H * 0.35f);

            // Keep away from sidebar
            if (p.x < kSidebarX + kSidebarW + 50.f) {
                p.x = kSidebarX + kSidebarW + 50.f + frand(0.f, 100.f);
            }
            p.size = frand(3.0f, 5.0f);
        }
    }
}

void GalaxyView::addExoplanet(const std::string& name, const std::string& type,
                              float distanceLY, const std::string& hostStar,
                              float radiusEarth, float massEarth, float tempK) {
    // Check if already exists
    for (const auto& p : m_planets) {
        if (p.name == name) return;
    }

    GalaxyPlanet p;
    p.name = name;
    p.typeStr = type;
    p.hostStar = hostStar.empty() ? name.substr(0, name.rfind(' ')) : hostStar;
    p.presetIdx = -1;
    p.distanceLY = distanceLY;
    p.radiusEarth = radiusEarth;
    p.massEarth = massEarth;
    p.tempK = tempK;
    p.isCached = false;
    p.x = p.y = 0.f;
    p.size = 4.0f;

    // Color based on type
    if (type.find("Gas") != std::string::npos || type.find("Jupiter") != std::string::npos) {
        p.r = 0.85f; p.g = 0.65f; p.b = 0.40f;
    } else if (type.find("Neptune") != std::string::npos) {
        p.r = 0.40f; p.g = 0.70f; p.b = 1.00f;
    } else {
        p.r = 0.65f; p.g = 0.75f; p.b = 0.85f;
    }

    m_planets.push_back(p);

    if (m_initialized && m_lastW > 0.f) {
        assignPlanetPositions(m_lastW, m_lastH);
    }
    updateSearch();
}

ImU32 GalaxyView::getPlanetColor(const GalaxyPlanet& p, float alpha) const {
    return col32f(p.r, p.g, p.b, alpha);
}

void GalaxyView::pickHighlightStar(const std::string& planetName) {
    if (m_stars.empty()) { m_highlightedStarIdx = -1; return; }

    const float cx = galaxyCX(m_lastW);
    const float cy = galaxyCY(m_lastH);
    const float maxDist = m_lastW * 0.12f;

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

void GalaxyView::updateSearch() {
    m_filteredIndices.clear();

    std::string query;
    if (m_searchBuf[0] != '\0') {
        query = m_searchBuf;
        std::transform(query.begin(), query.end(), query.begin(), ::tolower);
    }

    for (int i = 0; i < static_cast<int>(m_planets.size()); ++i) {
        const auto& p = m_planets[static_cast<size_t>(i)];

        if (query.empty()) {
            m_filteredIndices.push_back(i);
        } else {
            std::string nameLower = p.name;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
            std::string typeLower = p.typeStr;
            std::transform(typeLower.begin(), typeLower.end(), typeLower.begin(), ::tolower);
            std::string hostLower = p.hostStar;
            std::transform(hostLower.begin(), hostLower.end(), hostLower.begin(), ::tolower);

            if (nameLower.find(query) != std::string::npos ||
                typeLower.find(query) != std::string::npos ||
                hostLower.find(query) != std::string::npos) {
                m_filteredIndices.push_back(i);
            }
        }
    }
}

void GalaxyView::update(float dt, float W, float H) {
    m_time += dt;

    // Handle window resize
    if (m_initialized && (std::abs(W - m_lastW) > 1.f || std::abs(H - m_lastH) > 1.f)) {
        float scaleX = W / m_lastW;
        float scaleY = H / m_lastH;
        for (auto& s : m_stars) {
            s.x *= scaleX;
            s.y *= scaleY;
        }
        for (auto& p : m_planets) {
            p.x *= scaleX;
            p.y *= scaleY;
        }
        for (auto& p : m_expParts) {
            p.x *= scaleX;
            p.y *= scaleY;
        }
        for (auto& p : m_transParts) {
            p.x *= scaleX;
            p.y *= scaleY;
            p.tx *= scaleX;
            p.ty *= scaleY;
        }
        m_lastW = W;
        m_lastH = H;
    }

    if (m_exploding) {
        m_explosionTimer += dt;
        for (auto& p : m_expParts) {
            p.x    += p.vx * dt;
            p.y    += p.vy * dt;
            p.alpha = std::max(0.f, 1.f - m_explosionTimer / kExplosionDur);
        }
        if (m_explosionTimer >= kExplosionDur)
            m_explosionDone = true;
    }
    if (m_transitioning)
        updateTransition(dt);

    if (m_exploding || m_transitioning) return;

    // Arrow keys cycle planets
    if (!ImGui::GetIO().WantCaptureKeyboard && !m_filteredIndices.empty()) {
        int delta = 0;
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true)) delta = 1;
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true)) delta = -1;

        if (delta != 0) {
            // Find current selection in filtered list
            int filteredPos = 0;
            for (int i = 0; i < static_cast<int>(m_filteredIndices.size()); ++i) {
                if (m_filteredIndices[static_cast<size_t>(i)] == m_selectedIdx) {
                    filteredPos = i;
                    break;
                }
            }
            filteredPos = (filteredPos + delta + static_cast<int>(m_filteredIndices.size()))
                          % static_cast<int>(m_filteredIndices.size());
            m_selectedIdx = m_filteredIndices[static_cast<size_t>(filteredPos)];

            auto& sel = m_planets[static_cast<size_t>(m_selectedIdx)];
            m_selectedName = sel.name;
            m_selectedPreset = sel.presetIdx;
            m_highlightedStarIdx = -1;
        }
    }

    // Mouse hover + click on planet dots
    m_hoveredIdx = -1;
    ImVec2 mp = ImGui::GetMousePos();
    for (int i = 0; i < static_cast<int>(m_planets.size()); ++i) {
        auto& p = m_planets[static_cast<size_t>(i)];
        float dx = mp.x - p.x, dy = mp.y - p.y;
        if (dx * dx + dy * dy < (p.size + 8.f) * (p.size + 8.f)) {
            m_hoveredIdx = i;
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
                !ImGui::GetIO().WantCaptureMouse) {
                m_selectedIdx = i;
                m_selectedName = p.name;
                m_selectedPreset = p.presetIdx;
                m_highlightedStarIdx = -1;
            }
            break;
        }
    }
}

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

    if (m_lastW > 0.f && m_lastH > 0.f)
        triggerTransition(m_lastW, m_lastH);
}

void GalaxyView::drawGlowLine(ImDrawList* dl, ImVec2 a, ImVec2 b, float alpha) const {
    dl->AddLine(a, b, IM_COL32(100, 180, 255, static_cast<int>(28 * alpha)), kGlowLineOuter);
    dl->AddLine(a, b, IM_COL32(160, 210, 255, static_cast<int>(55 * alpha)), 3.0f);
    dl->AddLine(a, b, IM_COL32(220, 240, 255, static_cast<int>(130 * alpha)), kGlowLineCore);
}

void GalaxyView::renderBackground(ImDrawList* dl, float W, float H) {
    if (!m_initialized) return;

    if (m_exploding || m_transitioning) {
        if (m_exploding) {
            for (auto& p : m_expParts) {
                if (p.alpha > 0.f)
                    dl->AddCircleFilled({p.x, p.y}, p.size, col32f(p.r, p.g, p.b, p.alpha));
            }
        }
        if (m_transitioning)
            drawTransitionParticles(dl);
        return;
    }

    // Background stars with twinkle
    for (int si = 0; si < static_cast<int>(m_stars.size()); ++si) {
        auto& s = m_stars[static_cast<size_t>(si)];
        float tw = 0.55f + 0.45f * std::sin(m_time * s.twinkleSpeed + s.twinklePhase);

        if (si == m_highlightedStarIdx) {
            float p = 0.65f + 0.35f * std::sin(m_time * 2.6f);
            dl->AddCircleFilled({s.x, s.y}, s.size * 7.f, col32f(0.25f, 0.55f, 1.0f, 0.12f * p));
            dl->AddCircleFilled({s.x, s.y}, s.size * 4.f, col32f(0.45f, 0.75f, 1.0f, 0.30f * p));
            dl->AddCircleFilled({s.x, s.y}, s.size * 2.f, col32f(0.80f, 0.92f, 1.0f, 0.85f * p));
            dl->AddCircleFilled({s.x, s.y}, s.size, col32f(1.0f, 1.0f, 1.0f, 1.0f));
        } else {
            dl->AddCircleFilled({s.x, s.y}, s.size, col32f(s.r, s.g, s.b, 0.50f + 0.50f * tw));
        }
    }

    // Planet dots
    for (int i = 0; i < static_cast<int>(m_planets.size()); ++i) {
        auto& p = m_planets[static_cast<size_t>(i)];
        const bool isSel = (i == m_selectedIdx);
        const bool isHov = (i == m_hoveredIdx);
        const float pulse = isSel ? (0.75f + 0.25f * std::sin(m_time * 2.2f)) : 1.0f;

        if (isSel || isHov) {
            float ga = isSel ? 0.30f * pulse : 0.16f;
            dl->AddCircleFilled({p.x, p.y}, p.size * 3.6f, col32f(p.r * 0.8f, p.g * 0.8f, p.b, ga));
            dl->AddCircleFilled({p.x, p.y}, p.size * 2.2f, col32f(p.r, p.g, p.b, ga * 1.8f));
        }

        dl->AddCircleFilled({p.x, p.y}, p.size * pulse, col32f(p.r, p.g, p.b, isSel ? 1.0f : 0.80f));
        dl->AddCircleFilled({p.x - p.size * 0.28f, p.y - p.size * 0.28f},
            p.size * 0.30f, IM_COL32(255, 255, 255, isSel ? 200 : 120));
    }

    // Constellation connector to fact card
    if (!m_planets.empty() && m_selectedIdx < static_cast<int>(m_planets.size())) {
        auto& sel = m_planets[static_cast<size_t>(m_selectedIdx)];
        const float cardX = W - 300.f;
        const float cardY = 35.f;
        float pulse = 0.6f + 0.4f * std::sin(m_time * 1.5f);

        float nodeX = cardX - 42.f;
        float nodeY = sel.y * 0.38f + (cardY + 100.f) * 0.62f;

        drawGlowLine(dl, {sel.x, sel.y}, {nodeX, nodeY}, pulse);

        float dotR = 3.0f + 1.2f * std::sin(m_time * 2.3f);
        dl->AddCircleFilled({nodeX, nodeY}, dotR + 5.f, col32f(0.40f, 0.70f, 1.00f, 0.18f * pulse));
        dl->AddCircleFilled({nodeX, nodeY}, dotR + 2.5f, col32f(0.60f, 0.85f, 1.00f, 0.45f * pulse));
        dl->AddCircleFilled({nodeX, nodeY}, dotR, col32f(0.92f, 0.96f, 1.00f, 0.90f * pulse));

        drawGlowLine(dl, {nodeX, nodeY}, {cardX, nodeY}, pulse);
    }
}

bool GalaxyView::renderUI(float W, float H) {
    if (!m_initialized || m_exploding) return false;
    bool expandPressed = false;

    // Left sidebar with planet list
    ImGui::SetNextWindowPos({kSidebarX, 10.f}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({kSidebarW, H - 20.f}, ImGuiCond_Always);

    ImGuiWindowFlags sidebarFlags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    if (ImGui::Begin("##galaxy_sidebar", nullptr, sidebarFlags)) {
        ImGui::Spacing();
        ImGui::TextDisabled("ASTRODEX  //  EXOPLANET BROWSER");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Search bar
        ImGui::TextDisabled("Search (name, type, or host star)");
        ImGui::SetNextItemWidth(-1.f);
        if (ImGui::InputText("##search", m_searchBuf, sizeof(m_searchBuf))) {
            updateSearch();
        }
        ImGui::Spacing();

        // Planet count
        ImGui::TextDisabled("Showing %d of %d planets",
                           static_cast<int>(m_filteredIndices.size()),
                           static_cast<int>(m_planets.size()));
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Scrollable planet list
        float listHeight = H - 280.f;
        if (ImGui::BeginChild("##planetlist", {-1.f, listHeight}, true)) {
            for (size_t fi = 0; fi < m_filteredIndices.size(); ++fi) {
                int idx = m_filteredIndices[fi];
                auto& planet = m_planets[static_cast<size_t>(idx)];
                bool isSel = (idx == m_selectedIdx);

                ImGui::PushID(idx);

                // Color indicator
                ImVec2 cp = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddCircleFilled(
                    {cp.x + 8.f, cp.y + 10.f}, 6.f,
                    col32f(planet.r, planet.g, planet.b, isSel ? 1.f : 0.7f));


                ImGui::SetCursorScreenPos({cp.x + 20.f, cp.y});

                // Selectable row
                std::string label = planet.name;
                if (ImGui::Selectable(label.c_str(), isSel, ImGuiSelectableFlags_None, {0.f, 20.f})) {
                    m_selectedIdx = idx;
                    m_selectedName = planet.name;
                    m_selectedPreset = planet.presetIdx;
                    if (planet.presetIdx < 0)
                        pickHighlightStar(planet.name);
                    else
                        m_highlightedStarIdx = -1;
                }

                // Type on same line (right-aligned)
                ImGui::SameLine(kSidebarW - 120.f);
                ImGui::TextDisabled("%s", planet.typeStr.c_str());

                ImGui::PopID();
            }
        }
        ImGui::EndChild();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Solar System button at bottom of sidebar
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.15f, 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.3f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.4f, 0.2f, 1.0f));
        if (ImGui::Button("Solar System Simulation", ImVec2(-1.f, 35.f))) {
            m_solarSystemRequested = true;
        }
        ImGui::PopStyleColor(3);
        ImGui::TextDisabled("View the full solar system with orbits");
    }
    ImGui::End();

    // Fact card (upper right)
    if (!m_planets.empty() && m_selectedIdx < static_cast<int>(m_planets.size())) {
        auto& sel = m_planets[static_cast<size_t>(m_selectedIdx)];
        float cardX = W - 300.f;
        float cardY = 35.f;

        ImGui::SetNextWindowPos({cardX, cardY}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({285.f, 320.f}, ImGuiCond_Always);

        if (ImGui::Begin("##factcard", nullptr,
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoBringToFrontOnFocus)) {

            ImGui::Spacing();

            // Planet name with color
            ImGui::PushStyleColor(ImGuiCol_Text,
                ImVec4(sel.r * 0.85f + 0.15f, sel.g * 0.85f + 0.15f, sel.b * 0.85f + 0.15f, 1.f));
            ImGui::Text("%s", sel.name.c_str());
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Properties
            ImGui::TextDisabled("Type");
            ImGui::SameLine(90.f);
            ImGui::Text("%s", sel.typeStr.c_str());

            if (!sel.hostStar.empty()) {
                ImGui::TextDisabled("Host Star");
                ImGui::SameLine(90.f);
                ImGui::Text("%s", sel.hostStar.c_str());
            }

            ImGui::TextDisabled("Distance");
            ImGui::SameLine(90.f);
            if (sel.distanceLY > 0.f)
                ImGui::Text("%.1f ly", sel.distanceLY);
            else if (sel.presetIdx >= 0)
                ImGui::Text("Solar System");
            else
                ImGui::TextDisabled("Unknown");

            if (sel.radiusEarth > 0.f) {
                ImGui::TextDisabled("Radius");
                ImGui::SameLine(90.f);
                ImGui::Text("%.2f Earth", sel.radiusEarth);
            }

            if (sel.massEarth > 0.f) {
                ImGui::TextDisabled("Mass");
                ImGui::SameLine(90.f);
                ImGui::Text("%.2f Earth", sel.massEarth);
            }

            if (sel.tempK > 0.f) {
                ImGui::TextDisabled("Temp");
                ImGui::SameLine(90.f);
                ImGui::Text("%.0f K", sel.tempK);
            }

            ImGui::TextDisabled("Source");
            ImGui::SameLine(90.f);
            if (sel.presetIdx == -2)
                ImGui::Text("System Preset");
            else if (sel.presetIdx >= 0)
                ImGui::Text("Preset");
            else
                ImGui::Text("NASA Archive");

            // Show fetch metadata button if needed
            bool needsMetadata = (sel.presetIdx < 0 && sel.distanceLY == 0.f &&
                                  sel.radiusEarth == 0.f && sel.massEarth == 0.f);
            if (needsMetadata && !m_fetchingMetadata) {
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.6f, 0.8f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.5f, 0.7f, 0.9f));
                if (ImGui::Button("Fetch Metadata", {-1.f, 24.f})) {
                    if (m_fetchMetadataCallback) {
                        m_fetchMetadataCallback(sel.name);
                    }
                }
                ImGui::PopStyleColor(2);
            } else if (m_fetchingMetadata) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "Fetching metadata...");
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Status indicator
            if (sel.presetIdx == -2) {
                ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.5f, 1.0f), "Full system simulation");
            } else if (sel.presetIdx >= 0) {
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Ready to view");
            } else {
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Ready to view");
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Expand button
            ImGui::PushStyleColor(ImGuiCol_Button,
                ImVec4(sel.r * 0.25f, sel.g * 0.25f, sel.b * 0.35f, 0.80f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                ImVec4(sel.r * 0.40f, sel.g * 0.40f, sel.b * 0.50f, 0.90f));
            const char* btnText = (sel.presetIdx == -2) ? "  ENTER SYSTEM  >" : "  VIEW PLANET  >";
            if (ImGui::Button(btnText, {-1.f, 36.f})) {
                expandPressed = true;
                triggerExplosion();
            }
            ImGui::PopStyleColor(2);
        }
        ImGui::End();
    }

    return expandPressed;
}

void GalaxyView::triggerTransition(float W, float H) {
    m_transParts.clear();

    if (m_planets.empty()) { m_transDone = true; return; }

    auto& sel = m_planets[static_cast<size_t>(m_selectedIdx)];
    float cardX = W - 300.f;
    float cardY = 35.f;
    float nodeX = cardX - 42.f;
    float nodeY = sel.y * 0.38f + (cardY + 100.f) * 0.62f;

    const float pX = 10.f, pY = 10.f, pW = 340.f, pH = H - 20.f;
    const float rr = 10.f;

    float sTop   = (pX + pW - rr) - (pX + rr);
    float sRight = (pY + pH - rr) - (pY + rr);
    float arcLen = rr * 1.5708f;
    float perim  = 2.f * (sTop + sRight) + 4.f * arcLen;

    const int N = 160;
    std::mt19937 rng(77);
    auto frand = [&](float lo, float hi) {
        return lo + std::uniform_real_distribution<float>(0.f, 1.f)(rng) * (hi - lo);
    };

    float seg1Len = std::sqrt((nodeX - sel.x) * (nodeX - sel.x) + (nodeY - sel.y) * (nodeY - sel.y));
    float seg2Len = std::abs(cardX - nodeX);
    float totalLen = seg1Len + seg2Len;

    for (int i = 0; i < N; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(N);

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
        p.x = sx + frand(-4.f, 4.f);
        p.y = sy + frand(-4.f, 4.f);
        p.tx = tx;
        p.ty = ty;
        p.alpha = 1.f;
        p.size = frand(1.6f, 3.2f);
        float cr = frand(0.f, 1.f);
        if      (cr < 0.55f) { p.r = 0.45f; p.g = 0.76f; p.b = 1.00f; }
        else if (cr < 0.82f) { p.r = 0.68f; p.g = 0.90f; p.b = 1.00f; }
        else                 { p.r = 0.90f; p.g = 0.96f; p.b = 1.00f; }
        m_transParts.push_back(p);
    }

    m_transitioning = true;
    m_transTimer = 0.f;
    m_transDone = false;
}

void GalaxyView::updateTransition(float dt) {
    if (!m_transitioning) return;
    m_transTimer += dt;

    constexpr float kAssemble = 0.60f;
    constexpr float kFadeDur = 0.35f;

    if (m_transTimer < kAssemble) {
        float spring = 5.f + m_transTimer * 7.f;
        for (auto& p : m_transParts) {
            p.x += (p.tx - p.x) * spring * dt;
            p.y += (p.ty - p.y) * spring * dt;
        }
    } else if (!m_borderFading) {
        m_borderAssembled = true;
        for (auto& p : m_transParts) {
            p.x = p.tx; p.y = p.ty; p.alpha = 1.f;
        }
    } else {
        float f = (m_transTimer - m_borderFadeStart) / kFadeDur;
        for (auto& p : m_transParts) {
            p.x = p.tx; p.y = p.ty;
            p.alpha = std::max(0.f, 1.f - f);
        }
        if (f >= 1.f) {
            m_transitioning = false;
            m_transDone = true;
        }
    }
}

void GalaxyView::releaseBorder() {
    if (m_transitioning && !m_borderFading) {
        m_borderFading = true;
        m_borderFadeStart = m_transTimer;
    }
}

void GalaxyView::drawTransitionParticles(ImDrawList* dl) {
    bool assembled = (m_borderAssembled && !m_borderFading);
    float glowScale = assembled ? (1.f + 0.55f * std::sin(m_transTimer * 5.0f)) : 1.f;

    for (auto& p : m_transParts) {
        if (p.alpha <= 0.01f) continue;
        float sz = p.size * glowScale;
        dl->AddCircleFilled({p.x, p.y}, sz + 4.f, col32f(p.r * 0.45f, p.g * 0.60f, p.b, 0.14f * p.alpha));
        dl->AddCircleFilled({p.x, p.y}, sz + 1.8f, col32f(p.r * 0.75f, p.g * 0.85f, p.b, 0.35f * p.alpha));
        dl->AddCircleFilled({p.x, p.y}, sz, col32f(p.r, p.g, p.b, 0.92f * p.alpha));
    }
}

void GalaxyView::updatePlanetMetadata(const std::string& name, const std::string& hostStar,
                                       float distanceLY, float radiusEarth, float massEarth,
                                       float tempK) {
    for (auto& p : m_planets) {
        if (p.name == name) {
            if (!hostStar.empty()) p.hostStar = hostStar;
            if (distanceLY > 0.f) p.distanceLY = distanceLY;
            if (radiusEarth > 0.f) p.radiusEarth = radiusEarth;
            if (massEarth > 0.f) p.massEarth = massEarth;
            if (tempK > 0.f) p.tempK = tempK;
            break;
        }
    }
}

bool GalaxyView::selectedNeedsMetadata() const {
    if (m_selectedIdx < 0 || m_selectedIdx >= static_cast<int>(m_planets.size()))
        return false;
    const auto& sel = m_planets[static_cast<size_t>(m_selectedIdx)];
    // Presets don't need metadata fetch
    if (sel.presetIdx >= 0) return false;
    // Check if we're missing key metadata
    return sel.distanceLY == 0.f && sel.radiusEarth == 0.f && sel.massEarth == 0.f;
}

void GalaxyView::setExoplanetCallback(std::function<void(const std::string&)> cb) {
    m_exoCallback = std::move(cb);
}

void GalaxyView::setFetchMetadataCallback(std::function<void(const std::string&)> cb) {
    m_fetchMetadataCallback = std::move(cb);
}

void GalaxyView::setExoplanetStatus(const std::string& status) {
    m_exoStatus = status;
}

}  // namespace astrocore
