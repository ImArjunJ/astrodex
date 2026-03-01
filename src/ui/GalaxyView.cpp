#include "ui/GalaxyView.hpp"
#include "render/ExoplanetConverter.hpp"
#include "render/PlanetThumbnailRenderer.hpp"
#include "render/Galaxy3DRenderer.hpp"

#include <imgui.h>
#include <GLFW/glfw3.h>
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>

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

GalaxyView::~GalaxyView() {
    if (m_thumbnailRenderer) {
        m_thumbnailRenderer->shutdown();
    }
}

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

    // Initialize thumbnail renderer for catalog
    m_thumbnailRenderer = std::make_unique<PlanetThumbnailRenderer>();
    m_thumbnailRenderer->init(128);
    m_catalogOpen = false;
    m_catalogSlideAnim = 0.0f;

    // Initialize 3D galaxy renderer
    m_galaxyRenderer = std::make_unique<Galaxy3DRenderer>();
    m_galaxyRenderer->init();
    m_galaxyRenderer->generateGalaxy(60000, 4);  // 60k stars, 4 spiral arms

    // Position camera to look at galaxy from above
    m_cameraPos = glm::vec3(0.0f, 400.0f, 600.0f);
    m_cameraYaw = 0.0f;
    m_cameraPitch = -0.5f;  // Looking down at galaxy
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

    // Sync all planets to 3D renderer
    if (m_galaxyRenderer) {
        m_galaxyRenderer->clearObjects();
        for (int i = 0; i < static_cast<int>(m_planets.size()); ++i) {
            const auto& p = m_planets[static_cast<size_t>(i)];
            m_galaxyRenderer->addObject(p.name, p.typeStr, p.distanceLY,
                                        glm::vec3(p.r, p.g, p.b), i);
        }
    }
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

    // Update 3D camera
    handleCameraInput(dt);
    updateCameraAnimation(dt);

    // Update thumbnail renderer
    if (m_thumbnailRenderer) {
        m_thumbnailRenderer->update(dt);
        m_thumbnailRenderer->processRenderQueue(2);  // Render up to 2 thumbnails per frame

        // Periodically clear unused thumbnails
        static float clearTimer = 0.0f;
        clearTimer += dt;
        if (clearTimer > 10.0f) {
            m_thumbnailRenderer->clearUnused(30.0f);
            clearTimer = 0.0f;
        }
    }

    // Animate catalog slide
    float targetSlide = m_catalogOpen ? 1.0f : 0.0f;
    float slideSpeed = 6.0f;
    if (m_catalogSlideAnim < targetSlide) {
        m_catalogSlideAnim = std::min(m_catalogSlideAnim + dt * slideSpeed, targetSlide);
    } else if (m_catalogSlideAnim > targetSlide) {
        m_catalogSlideAnim = std::max(m_catalogSlideAnim - dt * slideSpeed, targetSlide);
    }

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

void GalaxyView::renderBackground(ImDrawList* /*dl*/, float W, float H) {
    if (!m_initialized) return;

    // Render 3D galaxy (OpenGL rendering, not ImGui)
    render3DGalaxy(W, H);
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

        // Catalog button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.25f, 0.35f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.40f, 0.55f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.30f, 0.45f, 0.60f, 1.0f));
        const char* catalogBtnText = m_catalogOpen ? "Close Catalog  <" : "Visual Catalog  >";
        if (ImGui::Button(catalogBtnText, ImVec2(-1.f, 35.f))) {
            m_catalogOpen = !m_catalogOpen;
        }
        ImGui::PopStyleColor(3);
        ImGui::TextDisabled("Browse planets visually with filters");

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

    // Render catalog panel if open or animating
    if (m_catalogSlideAnim > 0.01f) {
        renderCatalogPanel(W, H);
    }

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

            if (!sel.gaiaDr3Id.empty()) {
                ImGui::TextDisabled("Gaia DR3");
                ImGui::SameLine(90.f);
                // Truncate long IDs for display
                if (sel.gaiaDr3Id.length() > 20) {
                    ImGui::Text("%s...", sel.gaiaDr3Id.substr(0, 17).c_str());
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("%s", sel.gaiaDr3Id.c_str());
                    }
                } else {
                    ImGui::Text("%s", sel.gaiaDr3Id.c_str());
                }
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
                if (sel.presetIdx == -2) {
                    // Solar system - use old behavior
                    m_solarSystemRequested = true;
                } else {
                    // Start zoom transition to planet
                    startZoomToPlanet(m_selectedIdx);
                    expandPressed = true;
                }
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
                                       float tempK, const std::string& gaiaDr3Id) {
    for (auto& p : m_planets) {
        if (p.name == name) {
            if (!hostStar.empty()) p.hostStar = hostStar;
            if (distanceLY > 0.f) p.distanceLY = distanceLY;
            if (radiusEarth > 0.f) p.radiusEarth = radiusEarth;
            if (massEarth > 0.f) p.massEarth = massEarth;
            if (tempK > 0.f) p.tempK = tempK;
            if (!gaiaDr3Id.empty()) p.gaiaDr3Id = gaiaDr3Id;
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

bool GalaxyView::planetPassesFilter(int planetIdx) const {
    if (planetIdx < 0 || planetIdx >= static_cast<int>(m_planets.size()))
        return false;

    const auto& planet = m_planets[static_cast<size_t>(planetIdx)];

    // Type filter
    if (!m_catalogTypeFilter.empty()) {
        // Convert both to lowercase for case-insensitive comparison
        std::string typeStr = planet.typeStr;
        std::string filter = m_catalogTypeFilter;
        for (auto& c : typeStr) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        for (auto& c : filter) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (typeStr.find(filter) == std::string::npos) {
            return false;
        }
    }

    // Atmosphere filter
    if (m_filterAtmosphereEnabled && m_thumbnailRenderer) {
        const auto* info = m_thumbnailRenderer->getThumbnailInfo(planet.name);
        if (info) {
            if (m_filterHasAtmosphere && !info->hasAtmosphere) return false;
            if (!m_filterHasAtmosphere && info->hasAtmosphere) return false;
        }
    }

    // Hue filter (only if thumbnail is cached)
    if (m_thumbnailRenderer && (m_filterHueMin > 0.01f || m_filterHueMax < 0.99f)) {
        const auto* info = m_thumbnailRenderer->getThumbnailInfo(planet.name);
        if (info) {
            float hue = info->dominantHue.x;  // 0-1 range
            if (hue < m_filterHueMin || hue > m_filterHueMax) {
                return false;
            }
        }
    }

    // Brightness filter
    if (m_thumbnailRenderer && (m_filterBrightnessMin > 0.01f || m_filterBrightnessMax < 0.99f)) {
        const auto* info = m_thumbnailRenderer->getThumbnailInfo(planet.name);
        if (info) {
            if (info->avgBrightness < m_filterBrightnessMin ||
                info->avgBrightness > m_filterBrightnessMax) {
                return false;
            }
        }
    }

    return true;
}

void GalaxyView::renderCatalogPanel(float W, float H) {
    // Panel slides out from the left sidebar
    const float sidebarRight = kSidebarX + kSidebarW;
    const float minWidth = 300.f;
    const float maxWidth = W - sidebarRight - 320.f;  // Leave room for fact card
    m_catalogPanelWidth = std::clamp(m_catalogPanelWidth, minWidth, maxWidth);

    float slideOffset = sidebarRight + (m_catalogPanelWidth * (m_catalogSlideAnim - 1.0f));

    // Position and size - allow horizontal resize
    ImGui::SetNextWindowPos({slideOffset, 10.f}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({m_catalogPanelWidth, H - 20.f}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.95f);
    ImGui::SetNextWindowSizeConstraints({minWidth, H - 20.f}, {maxWidth, H - 20.f});

    ImGuiWindowFlags panelFlags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings;

    // Use slide animation for alpha too
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, m_catalogSlideAnim);

    if (ImGui::Begin("##catalog_panel", nullptr, panelFlags)) {
        // Update width if user resized
        m_catalogPanelWidth = ImGui::GetWindowWidth();

        // Header with close button
        ImGui::TextDisabled("VISUAL CATALOG");
        ImGui::SameLine(m_catalogPanelWidth - 80.f);
        if (ImGui::SmallButton("Close X")) {
            m_catalogOpen = false;
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Filter controls
        ImGui::TextDisabled("Filters");

        // Type filter dropdown
        ImGui::SetNextItemWidth(120.f);
        if (ImGui::BeginCombo("##typefilter",
                              m_catalogTypeFilter.empty() ? "All Types" : m_catalogTypeFilter.c_str())) {
            if (ImGui::Selectable("All Types", m_catalogTypeFilter.empty())) {
                m_catalogTypeFilter.clear();
            }
            const char* types[] = {"Rocky", "Terrestrial", "Super-Earth", "Gas Giant", "Ice Giant",
                                   "Neptune", "Ocean", "Lava", "Desert"};
            for (const char* type : types) {
                if (ImGui::Selectable(type, m_catalogTypeFilter == type)) {
                    m_catalogTypeFilter = type;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        ImGui::Checkbox("Has Atmosphere", &m_filterHasAtmosphere);
        if (ImGui::IsItemClicked()) {
            m_filterAtmosphereEnabled = true;
        }

        // Hue range slider
        ImGui::SetNextItemWidth(-1.f);
        ImGui::SliderFloat2("Hue Range", &m_filterHueMin, 0.0f, 1.0f, "%.2f");

        // Brightness range slider
        ImGui::SetNextItemWidth(-1.f);
        ImGui::SliderFloat2("Brightness", &m_filterBrightnessMin, 0.0f, 1.0f, "%.2f");

        // Reset filters button
        if (ImGui::SmallButton("Reset Filters")) {
            m_catalogTypeFilter.clear();
            m_filterHueMin = 0.0f;
            m_filterHueMax = 1.0f;
            m_filterBrightnessMin = 0.0f;
            m_filterBrightnessMax = 1.0f;
            m_filterAtmosphereEnabled = false;
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Calculate visible area
        float scrollRegionHeight = H - 240.f;
        ImGui::BeginChild("##catalog_grid", {-1.f, scrollRegionHeight}, true,
                          ImGuiWindowFlags_HorizontalScrollbar);

        // Grid layout
        const float tileSize = static_cast<float>(m_catalogTileSize);
        const float tilePadding = 8.f;
        const float totalTileSize = tileSize + tilePadding;
        const int columns = std::max(1, static_cast<int>((m_catalogPanelWidth - 30.f) / totalTileSize));

        // Build filtered list
        std::vector<int> visiblePlanets;
        for (int i = 0; i < static_cast<int>(m_planets.size()); ++i) {
            if (m_planets[static_cast<size_t>(i)].isCached && planetPassesFilter(i)) {
                visiblePlanets.push_back(i);
            }
        }

        // Virtualization: only render visible tiles
        float scrollY = ImGui::GetScrollY();
        float windowHeight = ImGui::GetWindowHeight();
        int firstVisibleRow = std::max(0, static_cast<int>(scrollY / totalTileSize) - 1);
        int lastVisibleRow = static_cast<int>((scrollY + windowHeight) / totalTileSize) + 1;

        int totalRows = (static_cast<int>(visiblePlanets.size()) + columns - 1) / columns;

        // Add invisible spacer for scroll area
        ImGui::Dummy({0.f, static_cast<float>(totalRows) * totalTileSize});
        ImGui::SetCursorPosY(static_cast<float>(firstVisibleRow) * totalTileSize);

        for (int row = firstVisibleRow; row <= lastVisibleRow && row < totalRows; ++row) {
            for (int col = 0; col < columns; ++col) {
                int idx = row * columns + col;
                if (idx >= static_cast<int>(visiblePlanets.size())) break;

                int planetIdx = visiblePlanets[static_cast<size_t>(idx)];
                float x = static_cast<float>(col) * totalTileSize;
                float y = static_cast<float>(row) * totalTileSize;

                ImGui::SetCursorPos({x, y});
                renderCatalogTile(planetIdx, x, y, tileSize);
            }
        }

        ImGui::EndChild();

        // Stats
        ImGui::Spacing();
        ImGui::TextDisabled("Showing %d of %d cached planets",
                           static_cast<int>(visiblePlanets.size()),
                           static_cast<int>(std::count_if(m_planets.begin(), m_planets.end(),
                               [](const GalaxyPlanet& p) { return p.isCached; })));
        if (m_thumbnailRenderer) {
            ImGui::TextDisabled("Cache: %d | Queue: %d",
                               m_thumbnailRenderer->getCacheSize(),
                               m_thumbnailRenderer->getQueueSize());
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void GalaxyView::renderCatalogTile(int planetIdx, float /*x*/, float /*y*/, float size) {
    if (planetIdx < 0 || planetIdx >= static_cast<int>(m_planets.size()))
        return;

    const auto& planet = m_planets[static_cast<size_t>(planetIdx)];
    bool isSelected = (planetIdx == m_selectedIdx);

    ImGui::PushID(planetIdx);

    // Load planet params from cache
    auto cachedParams = ExoplanetConverter::loadCachedParams(planet.name);
    if (!cachedParams) {
        // Show placeholder for non-cached planets
        ImGui::BeginGroup();
        ImGui::Dummy({size, size});
        ImVec2 min = ImGui::GetItemRectMin();
        ImVec2 max = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddRectFilled(min, max,
            col32f(planet.r * 0.3f, planet.g * 0.3f, planet.b * 0.3f, 0.5f), 6.f);
        ImGui::GetWindowDrawList()->AddRect(min, max,
            col32f(planet.r, planet.g, planet.b, 0.8f), 6.f, 0, 2.f);

        // Planet name centered
        ImVec2 textSize = ImGui::CalcTextSize(planet.name.c_str());
        float textX = min.x + (size - textSize.x) * 0.5f;
        float textY = min.y + (size - textSize.y) * 0.5f;
        ImGui::GetWindowDrawList()->AddText({textX, textY},
            IM_COL32(200, 200, 200, 200), planet.name.c_str());

        ImGui::EndGroup();

        if (ImGui::IsItemClicked()) {
            m_selectedIdx = planetIdx;
            m_selectedName = planet.name;
            m_selectedPreset = planet.presetIdx;
        }

        ImGui::PopID();
        return;
    }

    // Request thumbnail render
    GLuint texId = 0;
    if (m_thumbnailRenderer) {
        texId = m_thumbnailRenderer->getThumbnail(planet.name, *cachedParams);
    }

    ImGui::BeginGroup();

    if (texId != 0) {
        // Draw the thumbnail
        ImGui::Image(static_cast<ImTextureID>(texId),
                     {size, size}, {0, 1}, {1, 0});  // Flip V for OpenGL
    } else {
        // Loading placeholder
        ImGui::Dummy({size, size});
        ImVec2 min = ImGui::GetItemRectMin();
        ImVec2 max = ImGui::GetItemRectMax();

        // Animated loading indicator
        float pulse = 0.5f + 0.3f * std::sin(m_time * 3.0f);
        ImGui::GetWindowDrawList()->AddRectFilled(min, max,
            col32f(planet.r * 0.2f, planet.g * 0.2f, planet.b * 0.25f, pulse), 6.f);

        // Loading text
        const char* loadingText = "...";
        ImVec2 textSize = ImGui::CalcTextSize(loadingText);
        float textX = min.x + (size - textSize.x) * 0.5f;
        float textY = min.y + (size - textSize.y) * 0.5f;
        ImGui::GetWindowDrawList()->AddText({textX, textY},
            IM_COL32(150, 180, 200, 200), loadingText);
    }

    // Selection highlight
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    if (isSelected) {
        ImGui::GetWindowDrawList()->AddRect(min, max,
            col32f(0.4f, 0.8f, 1.0f, 0.9f), 6.f, 0, 3.f);
    } else if (ImGui::IsItemHovered()) {
        ImGui::GetWindowDrawList()->AddRect(min, max,
            col32f(0.6f, 0.7f, 0.8f, 0.6f), 6.f, 0, 2.f);
    }

    ImGui::EndGroup();

    // Tooltip with planet info
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(planet.r, planet.g, planet.b, 1.0f));
        ImGui::Text("%s", planet.name.c_str());
        ImGui::PopStyleColor();
        ImGui::TextDisabled("%s", planet.typeStr.c_str());
        if (!planet.hostStar.empty()) {
            ImGui::TextDisabled("Host: %s", planet.hostStar.c_str());
        }
        ImGui::EndTooltip();
    }

    // Click to select
    if (ImGui::IsItemClicked()) {
        m_selectedIdx = planetIdx;
        m_selectedName = planet.name;
        m_selectedPreset = planet.presetIdx;
    }

    // Double-click to view
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        triggerExplosion();
    }

    ImGui::PopID();
}

// ═══════════════════════════════════════════════════════════════════════════════
// 3D Galaxy View Functions
// ═══════════════════════════════════════════════════════════════════════════════

glm::mat4 GalaxyView::getViewMatrix() const {
    // Calculate camera direction from yaw/pitch
    glm::vec3 direction;
    direction.x = std::cos(m_cameraPitch) * std::sin(m_cameraYaw);
    direction.y = std::sin(m_cameraPitch);
    direction.z = std::cos(m_cameraPitch) * std::cos(m_cameraYaw);

    glm::vec3 target = m_cameraPos + direction;
    return glm::lookAt(m_cameraPos, target, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 GalaxyView::getProjectionMatrix(float W, float H) const {
    return glm::perspective(glm::radians(60.0f), W / H, 1.0f, 10000.0f);
}

void GalaxyView::handleCameraInput(float dt) {
    if (!m_initialized || m_flyingToTarget) return;

    // Get GLFW window from ImGui
    ImGuiIO& io = ImGui::GetIO();

    // Don't process input if ImGui wants it
    if (io.WantCaptureKeyboard) return;

    // Calculate movement direction based on yaw
    glm::vec3 forward;
    forward.x = std::sin(m_cameraYaw);
    forward.y = 0.0f;
    forward.z = std::cos(m_cameraYaw);

    glm::vec3 right = glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f));

    float speed = m_cameraSpeed;

    // Sprint with Shift
    if (ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift)) {
        speed *= 3.0f;
    }

    // WASD / Arrow keys for movement
    if (ImGui::IsKeyDown(ImGuiKey_W) || ImGui::IsKeyDown(ImGuiKey_UpArrow)) {
        m_cameraPos += forward * speed * dt;
    }
    if (ImGui::IsKeyDown(ImGuiKey_S) || ImGui::IsKeyDown(ImGuiKey_DownArrow)) {
        m_cameraPos -= forward * speed * dt;
    }
    if (ImGui::IsKeyDown(ImGuiKey_A) || ImGui::IsKeyDown(ImGuiKey_LeftArrow)) {
        m_cameraPos -= right * speed * dt;
    }
    if (ImGui::IsKeyDown(ImGuiKey_D) || ImGui::IsKeyDown(ImGuiKey_RightArrow)) {
        m_cameraPos += right * speed * dt;
    }

    // Q/E for up/down
    if (ImGui::IsKeyDown(ImGuiKey_Q)) {
        m_cameraPos.y -= speed * dt;
    }
    if (ImGui::IsKeyDown(ImGuiKey_E)) {
        m_cameraPos.y += speed * dt;
    }

    // Mouse look (right-click drag)
    if (ImGui::IsMouseDown(ImGuiMouseButton_Right) && !io.WantCaptureMouse) {
        if (!m_rightMouseDown) {
            m_rightMouseDown = true;
            m_lastMousePos = glm::vec2(io.MousePos.x, io.MousePos.y);
        } else {
            glm::vec2 currentPos(io.MousePos.x, io.MousePos.y);
            glm::vec2 delta = currentPos - m_lastMousePos;
            m_lastMousePos = currentPos;

            m_cameraYaw -= delta.x * m_cameraSensitivity;
            m_cameraPitch -= delta.y * m_cameraSensitivity;

            // Clamp pitch
            m_cameraPitch = glm::clamp(m_cameraPitch, -1.5f, 1.5f);
        }
    } else {
        m_rightMouseDown = false;
    }

    // Scroll to zoom (move forward/backward)
    if (!io.WantCaptureMouse && std::abs(io.MouseWheel) > 0.01f) {
        glm::vec3 direction;
        direction.x = std::cos(m_cameraPitch) * std::sin(m_cameraYaw);
        direction.y = std::sin(m_cameraPitch);
        direction.z = std::cos(m_cameraPitch) * std::cos(m_cameraYaw);
        m_cameraPos += direction * io.MouseWheel * 50.0f;
    }
}

void GalaxyView::flyToObject(int objectId) {
    if (!m_galaxyRenderer) return;

    glm::vec3 targetPos = m_galaxyRenderer->getObjectPosition(objectId);

    // Position camera to view the object from a nice angle
    m_flyStartPos = m_cameraPos;
    m_flyEndPos = targetPos + glm::vec3(0.0f, 50.0f, 100.0f);

    // Point camera at target
    glm::vec3 toTarget = targetPos - m_flyEndPos;
    m_cameraYaw = std::atan2(toTarget.x, toTarget.z);
    m_cameraPitch = std::atan2(-toTarget.y, std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z));

    m_flyingToTarget = true;
    m_flyProgress = 0.0f;
    m_flyDuration = 2.0f;
}

void GalaxyView::render3DGalaxy(float W, float H) {
    if (!m_galaxyRenderer || !m_initialized) return;

    // Update selected object in renderer
    if (m_selectedIdx >= 0 && m_selectedIdx < static_cast<int>(m_planets.size())) {
        m_galaxyRenderer->setSelectedObject(m_selectedIdx);
    }

    // Get view/projection matrices
    glm::mat4 view = getViewMatrix();
    glm::mat4 proj = getProjectionMatrix(W, H);
    glm::mat4 viewProj = proj * view;

    // Render the 3D galaxy
    m_galaxyRenderer->render(viewProj, m_cameraPos, m_time);

    // Handle mouse picking (click to select planet) - only when not transitioning
    if (m_transitionState == TransitionState::None) {
        ImGuiIO& io = ImGui::GetIO();
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.WantCaptureMouse) {
            glm::vec2 mousePos(io.MousePos.x, io.MousePos.y);
            int pickedId = m_galaxyRenderer->pickObject(mousePos, viewProj, glm::vec2(W, H));
            if (pickedId >= 0 && pickedId < static_cast<int>(m_planets.size())) {
                m_selectedIdx = pickedId;
                m_selectedName = m_planets[static_cast<size_t>(pickedId)].name;
                m_selectedPreset = m_planets[static_cast<size_t>(pickedId)].presetIdx;
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Planet Zoom Transitions
// ═══════════════════════════════════════════════════════════════════════════════

void GalaxyView::startZoomToPlanet(int planetIdx) {
    if (planetIdx < 0 || planetIdx >= static_cast<int>(m_planets.size())) return;
    if (!m_galaxyRenderer) return;

    m_transitionPlanetIdx = planetIdx;
    m_transitionState = TransitionState::ZoomingIn;
    m_transitionProgress = 0.0f;

    // Store current camera state
    m_flyStartPos = m_cameraPos;
    m_flyStartYaw = m_cameraYaw;
    m_flyStartPitch = m_cameraPitch;

    // Calculate end position - very close to planet, looking at it
    glm::vec3 planetPos = m_galaxyRenderer->getObjectPosition(planetIdx);

    // End position is close to the planet, slightly offset
    m_flyEndPos = planetPos + glm::vec3(0.0f, 5.0f, 15.0f);

    // Calculate yaw/pitch to look at planet from end position
    glm::vec3 toTarget = planetPos - m_flyEndPos;
    m_flyEndYaw = std::atan2(toTarget.x, toTarget.z);
    m_flyEndPitch = std::atan2(-toTarget.y, std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z));

    // Set selected planet
    m_selectedIdx = planetIdx;
    m_selectedName = m_planets[static_cast<size_t>(planetIdx)].name;
    m_selectedPreset = m_planets[static_cast<size_t>(planetIdx)].presetIdx;
}

void GalaxyView::startZoomToGalaxy() {
    if (m_transitionState != TransitionState::ViewingPlanet) return;

    m_transitionState = TransitionState::ZoomingOut;
    m_transitionProgress = 0.0f;

    // Current position is close to planet, we want to zoom back out
    // Swap start/end for reverse animation
    glm::vec3 tempPos = m_flyStartPos;
    m_flyStartPos = m_flyEndPos;
    m_flyEndPos = tempPos;

    float tempYaw = m_flyStartYaw;
    m_flyStartYaw = m_flyEndYaw;
    m_flyEndYaw = tempYaw;

    float tempPitch = m_flyStartPitch;
    m_flyStartPitch = m_flyEndPitch;
    m_flyEndPitch = tempPitch;

    // Start from current camera position
    m_flyStartPos = m_cameraPos;
    m_flyStartYaw = m_cameraYaw;
    m_flyStartPitch = m_cameraPitch;
}

void GalaxyView::onPlanetReady() {
    // Called when planet detail rendering is ready
    // If we're zooming in and close enough, transition to viewing state
    if (m_transitionState == TransitionState::ZoomingIn && m_transitionProgress >= 0.9f) {
        m_transitionState = TransitionState::ViewingPlanet;
        m_transitionProgress = 1.0f;
    }
}

void GalaxyView::updateCameraAnimation(float dt) {
    // Handle zoom transitions
    if (m_transitionState == TransitionState::ZoomingIn ||
        m_transitionState == TransitionState::ZoomingOut) {

        m_transitionProgress += dt / m_transitionDuration;

        if (m_transitionProgress >= 1.0f) {
            m_transitionProgress = 1.0f;

            if (m_transitionState == TransitionState::ZoomingIn) {
                // Arrived at planet - wait for planet to be ready or switch immediately
                m_transitionState = TransitionState::ViewingPlanet;
            } else {
                // Returned to galaxy
                m_transitionState = TransitionState::None;
                m_transitionPlanetIdx = -1;
            }
        }

        // Smooth ease-in-out interpolation
        float t = m_transitionProgress;
        // Cubic ease-in-out for smooth acceleration/deceleration
        float smoothT;
        if (t < 0.5f) {
            smoothT = 4.0f * t * t * t;
        } else {
            float f = (2.0f * t) - 2.0f;
            smoothT = 0.5f * f * f * f + 1.0f;
        }

        // Interpolate camera position and orientation
        m_cameraPos = glm::mix(m_flyStartPos, m_flyEndPos, smoothT);
        m_cameraYaw = m_flyStartYaw + (m_flyEndYaw - m_flyStartYaw) * smoothT;
        m_cameraPitch = m_flyStartPitch + (m_flyEndPitch - m_flyStartPitch) * smoothT;

        return;  // Don't process other flying animations during transition
    }

    // Regular fly-to animation (for clicking planets in galaxy)
    if (!m_flyingToTarget) return;

    m_flyProgress += dt / m_flyDuration;

    if (m_flyProgress >= 1.0f) {
        m_flyProgress = 1.0f;
        m_flyingToTarget = false;
        m_cameraPos = m_flyEndPos;
    } else {
        // Smooth ease-in-out interpolation
        float t = m_flyProgress;
        float smoothT = t * t * (3.0f - 2.0f * t);  // Smoothstep
        m_cameraPos = glm::mix(m_flyStartPos, m_flyEndPos, smoothT);
    }
}

}  // namespace astrocore
