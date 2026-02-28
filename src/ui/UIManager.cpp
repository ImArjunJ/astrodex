#include "ui/UIManager.hpp"
#include "render/Renderer.hpp"
#include "core/Logger.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <nlohmann/json.hpp>
#include <sstream>

using json = nlohmann::json;

namespace astrocore {

// ── JSON Export ──────────────────────────────────────────────────────────────

static std::string paramsToJson(const PlanetParams& p) {
    json j;
    // Geometry
    j["radius"] = p.radius;
    j["rotationSpeed"] = p.rotationSpeed;
    j["rotationOffset"] = p.rotationOffset;
    j["quality"] = p.quality;

    // Noise
    j["noiseType"] = static_cast<int>(p.noiseType);
    j["noiseStrength"] = p.noiseStrength;
    j["terrainScale"] = p.terrainScale;
    j["fbmOctaves"] = p.fbmOctaves;
    j["fbmPersistence"] = p.fbmPersistence;
    j["fbmLacunarity"] = p.fbmLacunarity;
    j["fbmExponentiation"] = p.fbmExponentiation;
    j["domainWarpStrength"] = p.domainWarpStrength;

    // Terrain features
    j["ridgedStrength"] = p.ridgedStrength;
    j["craterStrength"] = p.craterStrength;
    j["continentScale"] = p.continentScale;
    j["continentBlend"] = p.continentBlend;
    j["waterLevel"] = p.waterLevel;

    // Latitude
    j["polarCapSize"] = p.polarCapSize;
    j["bandingStrength"] = p.bandingStrength;
    j["bandingFrequency"] = p.bandingFrequency;

    // Colors
    j["waterColorDeep"] = {p.waterColorDeep.x, p.waterColorDeep.y, p.waterColorDeep.z};
    j["waterColorSurface"] = {p.waterColorSurface.x, p.waterColorSurface.y, p.waterColorSurface.z};
    j["sandColor"] = {p.sandColor.x, p.sandColor.y, p.sandColor.z};
    j["treeColor"] = {p.treeColor.x, p.treeColor.y, p.treeColor.z};
    j["rockColor"] = {p.rockColor.x, p.rockColor.y, p.rockColor.z};
    j["iceColor"] = {p.iceColor.x, p.iceColor.y, p.iceColor.z};

    // Biome levels
    j["sandLevel"] = p.sandLevel;
    j["treeLevel"] = p.treeLevel;
    j["rockLevel"] = p.rockLevel;
    j["iceLevel"] = p.iceLevel;
    j["transition"] = p.transition;

    // Clouds
    j["cloudsDensity"] = p.cloudsDensity;
    j["cloudsScale"] = p.cloudsScale;
    j["cloudsSpeed"] = p.cloudsSpeed;
    j["cloudAltitude"] = p.cloudAltitude;
    j["cloudThickness"] = p.cloudThickness;
    j["cloudColor"] = {p.cloudColor.x, p.cloudColor.y, p.cloudColor.z};

    // Atmosphere
    j["atmosphereColor"] = {p.atmosphereColor.x, p.atmosphereColor.y, p.atmosphereColor.z};
    j["atmosphereDensity"] = p.atmosphereDensity;

    // Lighting
    j["sunDirection"] = {p.sunDirection.x, p.sunDirection.y, p.sunDirection.z};
    j["sunIntensity"] = p.sunIntensity;
    j["ambientLight"] = p.ambientLight;
    j["sunColor"] = {p.sunColor.x, p.sunColor.y, p.sunColor.z};
    j["deepSpaceColor"] = {p.deepSpaceColor.x, p.deepSpaceColor.y, p.deepSpaceColor.z};

    return j.dump(2);
}

// ── Presets ──────────────────────────────────────────────────────────────────

static const char* presetNames[] = {
    "Earth", "Mars", "Lava World", "Ice World",
    "Gas Giant", "Ocean World", "Desert", "Alien"
};
static constexpr int presetCount = sizeof(presetNames) / sizeof(presetNames[0]);

static PlanetParams makePreset(int index) {
    PlanetParams p{}; // Earth defaults
    switch (index) {
    case 0: // Earth — use defaults
        p.continentScale = 1.0f;
        break;
    case 1: // Mars — cratered, dry, thin atmosphere
        p.waterColorDeep    = {0.15f, 0.05f, 0.02f};
        p.waterColorSurface = {0.25f, 0.12f, 0.06f};
        p.sandColor         = {0.76f, 0.50f, 0.30f};
        p.treeColor         = {0.45f, 0.25f, 0.15f};
        p.rockColor         = {0.50f, 0.30f, 0.20f};
        p.iceColor          = {0.90f, 0.85f, 0.80f};
        p.atmosphereColor   = {0.8f, 0.4f, 0.2f};
        p.atmosphereDensity = 0.05f;
        p.cloudsDensity     = 0.05f;
        p.noiseStrength     = 0.25f;
        p.sunIntensity      = 2.5f;
        p.waterLevel        = -0.1f;
        p.fbmExponentiation = 8.0f;
        p.fbmPersistence    = 0.6f;
        p.polarCapSize      = 0.3f;
        p.craterStrength    = 0.7f;
        p.ridgedStrength    = 0.2f;
        break;
    case 2: // Lava World — cracked, organic warped terrain
        p.waterColorDeep    = {0.40f, 0.05f, 0.0f};
        p.waterColorSurface = {0.80f, 0.20f, 0.0f};
        p.sandColor         = {0.90f, 0.40f, 0.05f};
        p.treeColor         = {0.30f, 0.08f, 0.02f};
        p.rockColor         = {0.15f, 0.08f, 0.05f};
        p.iceColor          = {1.0f, 0.60f, 0.10f};
        p.cloudColor        = {0.3f, 0.15f, 0.05f};
        p.atmosphereColor   = {0.9f, 0.3f, 0.05f};
        p.atmosphereDensity = 0.4f;
        p.cloudsDensity     = 0.15f;
        p.sunIntensity      = 4.0f;
        p.sunColor          = {1.0f, 0.7f, 0.3f};
        p.deepSpaceColor    = {0.02f, 0.0f, 0.0f};
        p.noiseStrength     = 0.15f;
        p.domainWarpStrength = 0.8f;
        p.fbmPersistence    = 0.35f;
        p.fbmExponentiation = 3.0f;
        p.fbmLacunarity     = 2.5f;
        p.ridgedStrength    = 0.6f;
        p.craterStrength    = 0.3f;
        break;
    case 3: // Ice World — smooth, frozen, huge polar caps
        p.waterColorDeep    = {0.05f, 0.10f, 0.20f};
        p.waterColorSurface = {0.10f, 0.20f, 0.35f};
        p.sandColor         = {0.70f, 0.80f, 0.85f};
        p.treeColor         = {0.50f, 0.60f, 0.65f};
        p.rockColor         = {0.40f, 0.45f, 0.50f};
        p.iceColor          = {0.90f, 0.95f, 1.0f};
        p.atmosphereColor   = {0.4f, 0.6f, 0.9f};
        p.atmosphereDensity = 0.5f;
        p.cloudsDensity     = 0.6f;
        p.cloudColor        = {0.85f, 0.90f, 1.0f};
        p.sandLevel         = 0.01f;
        p.treeLevel         = 0.015f;
        p.rockLevel         = 0.04f;
        p.iceLevel          = 0.06f;
        p.fbmExponentiation = 2.0f;
        p.fbmPersistence    = 0.4f;
        p.fbmLacunarity     = 1.8f;
        p.polarCapSize      = 0.7f;
        p.ridgedStrength    = 0.3f;
        p.continentScale    = 0.5f;
        break;
    case 4: // Gas Giant — banded, massive, all clouds
        p.waterColorDeep    = {0.15f, 0.10f, 0.05f};
        p.waterColorSurface = {0.25f, 0.18f, 0.08f};
        p.sandColor         = {0.60f, 0.45f, 0.25f};
        p.treeColor         = {0.40f, 0.28f, 0.15f};
        p.rockColor         = {0.30f, 0.22f, 0.12f};
        p.iceColor          = {0.80f, 0.70f, 0.50f};
        p.atmosphereColor   = {0.3f, 0.2f, 0.6f};
        p.atmosphereDensity = 0.8f;
        p.cloudsDensity     = 1.0f;
        p.cloudsScale       = 2.0f;
        p.cloudsSpeed       = 3.0f;
        p.cloudColor        = {0.9f, 0.85f, 0.7f};
        p.radius            = 3.0f;
        p.noiseStrength     = 0.05f;
        p.terrainScale      = 0.4f;
        p.bandingStrength   = 0.9f;
        p.bandingFrequency  = 25.0f;
        p.domainWarpStrength = 0.3f;
        p.fbmExponentiation = 1.5f;
        break;
    case 5: // Ocean World — high water, mostly sea
        p.waterColorDeep    = {0.01f, 0.06f, 0.18f};
        p.waterColorSurface = {0.03f, 0.15f, 0.30f};
        p.sandColor         = {0.90f, 0.90f, 0.75f};
        p.treeColor         = {0.02f, 0.15f, 0.08f};
        p.rockColor         = {0.12f, 0.10f, 0.10f};
        p.iceColor          = {0.85f, 0.92f, 0.95f};
        p.atmosphereColor   = {0.05f, 0.35f, 0.7f};
        p.atmosphereDensity = 0.4f;
        p.cloudsDensity     = 0.6f;
        p.sandLevel         = 0.06f;
        p.treeLevel         = 0.065f;
        p.rockLevel         = 0.12f;
        p.iceLevel          = 0.18f;
        p.waterLevel        = 0.15f;
        p.fbmExponentiation = 7.0f;
        p.fbmPersistence    = 0.45f;
        p.continentScale    = 1.5f;
        p.cloudAltitude     = 0.12f;
        p.cloudThickness    = 0.15f;
        break;
    case 6: // Desert — dry, broad dunes, no water
        p.waterColorDeep    = {0.35f, 0.25f, 0.10f};
        p.waterColorSurface = {0.50f, 0.38f, 0.18f};
        p.sandColor         = {0.90f, 0.78f, 0.50f};
        p.treeColor         = {0.55f, 0.42f, 0.22f};
        p.rockColor         = {0.40f, 0.30f, 0.18f};
        p.iceColor          = {0.95f, 0.90f, 0.75f};
        p.atmosphereColor   = {0.7f, 0.45f, 0.15f};
        p.atmosphereDensity = 0.1f;
        p.cloudsDensity     = 0.0f;
        p.sunIntensity      = 3.5f;
        p.sunColor          = {1.0f, 0.9f, 0.7f};
        p.noiseStrength     = 0.18f;
        p.waterLevel        = -0.15f;
        p.fbmLacunarity     = 1.6f;
        p.fbmExponentiation = 3.5f;
        p.fbmPersistence    = 0.55f;
        break;
    case 7: // Alien — heavily warped, bizarre terrain
        p.waterColorDeep    = {0.10f, 0.0f, 0.15f};
        p.waterColorSurface = {0.20f, 0.05f, 0.25f};
        p.sandColor         = {0.30f, 0.80f, 0.25f};
        p.treeColor         = {0.15f, 0.50f, 0.35f};
        p.rockColor         = {0.25f, 0.12f, 0.30f};
        p.iceColor          = {0.70f, 0.50f, 0.90f};
        p.cloudColor        = {0.6f, 0.4f, 0.7f};
        p.atmosphereColor   = {0.7f, 0.1f, 0.6f};
        p.atmosphereDensity = 0.35f;
        p.cloudsDensity     = 0.35f;
        p.sunColor          = {0.8f, 0.6f, 1.0f};
        p.deepSpaceColor    = {0.005f, 0.0f, 0.008f};
        p.domainWarpStrength = 1.2f;
        p.fbmPersistence    = 0.7f;
        p.fbmLacunarity     = 2.8f;
        p.fbmExponentiation = 2.5f;
        p.fbmOctaves        = 5;
        p.ridgedStrength    = 0.5f;
        p.craterStrength    = 0.4f;
        break;
    }
    return p;
}

// ── UIManager ────────────────────────────────────────────────────────────────

UIManager::UIManager() = default;

UIManager::~UIManager() {
    if (m_initialized) {
        shutdown();
    }
}

void UIManager::init(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 450");

    setupStyle();

    m_initialized = true;
    LOG_INFO("ImGui initialized");
}

void UIManager::shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    m_initialized = false;
}

void UIManager::setupStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark();

    style.WindowRounding = 4.0f;
    style.FrameRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowPadding = ImVec2(10, 10);
    style.FramePadding = ImVec2(6, 4);
    style.ItemSpacing = ImVec2(8, 6);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.10f, 0.95f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.14f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.20f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.25f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.30f, 0.35f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.35f, 0.35f, 0.40f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.20f, 0.40f, 0.60f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.50f, 0.70f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.25f, 0.45f, 0.65f, 1.00f);
}

void UIManager::beginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Create fullscreen dockspace
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags dockspaceFlags =
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("DockSpace", nullptr, dockspaceFlags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::End();
}

void UIManager::endFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void UIManager::render(PlanetParams& p) {
    renderPlanetEditor("Planet", p);
}

void UIManager::renderPlanetEditor(const std::string& bodyName, PlanetParams& p,
                                    RingParams* ringParams,
                                    std::function<void()> onRingChanged) {
    ImGui::SetNextWindowSize(ImVec2(340, 720), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Properties")) {
        ImGui::End();
        return;
    }

    // Show which body is being edited
    ImGui::Text("Editing: %s", bodyName.c_str());
    ImGui::Separator();

    // ── Presets ──────────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Presets", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SetNextItemWidth(-70);
        ImGui::Combo("##preset", &m_presetIndex, presetNames, presetCount);
        ImGui::SameLine();
        if (ImGui::Button("Apply")) {
            p = makePreset(m_presetIndex);
        }
    }

    // ── Planet ───────────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Planet", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Radius", &p.radius, 0.5f, 5.0f);
        ImGui::SliderFloat("Rotation Offset", &p.rotationOffset, 0.0f, 6.28f);
        ImGui::SliderFloat("Quality", &p.quality, 0.0f, 2.0f);
        ImGui::SliderFloat("Rotation Speed", &p.rotationSpeed, 0.0f, 1.0f);
    }

    // ── Terrain ──────────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Terrain", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Noise type selector
        const char* noiseTypes[] = {"Standard", "Ridged", "Billowy", "Warped", "Voronoi", "Swiss", "Hybrid"};
        int noiseType = static_cast<int>(p.noiseType);
        if (ImGui::Combo("Noise Type", &noiseType, noiseTypes, 7)) {
            p.noiseType = static_cast<NoiseType>(noiseType);
        }

        ImGui::SliderFloat("Noise Strength", &p.noiseStrength, 0.0f, 0.5f);
        ImGui::SliderFloat("Terrain Scale", &p.terrainScale, 0.1f, 3.0f);
        ImGui::SliderFloat("Water Level", &p.waterLevel, -0.2f, 0.3f);
        ImGui::Separator();
        ImGui::TextDisabled("Noise Shape");
        ImGui::SliderInt("Octaves", &p.fbmOctaves, 1, 8);
        ImGui::SliderFloat("Persistence", &p.fbmPersistence, 0.1f, 0.9f, "%.2f");
        ImGui::SliderFloat("Lacunarity", &p.fbmLacunarity, 1.0f, 4.0f, "%.2f");
        ImGui::SliderFloat("Exponentiation", &p.fbmExponentiation, 0.5f, 10.0f, "%.1f");
        ImGui::SliderFloat("Domain Warp", &p.domainWarpStrength, 0.0f, 2.0f, "%.2f");
        ImGui::Separator();
        ImGui::TextDisabled("Terrain Features");
        ImGui::SliderFloat("Ridged Strength", &p.ridgedStrength, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Crater Strength", &p.craterStrength, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Continent Scale", &p.continentScale, 0.0f, 3.0f, "%.2f");
        ImGui::SliderFloat("Continent Blend", &p.continentBlend, 0.01f, 0.5f, "%.2f");
    }

    // ── Latitude Effects ────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Latitude Effects")) {
        ImGui::SliderFloat("Polar Cap Size", &p.polarCapSize, 0.0f, 1.0f);
        ImGui::SliderFloat("Banding Strength", &p.bandingStrength, 0.0f, 1.0f);
        ImGui::SliderFloat("Banding Frequency", &p.bandingFrequency, 5.0f, 50.0f, "%.0f");
    }

    // ── Surface Colors ──────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Surface Colors")) {
        ImGui::ColorEdit3("Water Deep", &p.waterColorDeep.x);
        ImGui::ColorEdit3("Water Surface", &p.waterColorSurface.x);
        ImGui::ColorEdit3("Sand", &p.sandColor.x);
        ImGui::ColorEdit3("Trees", &p.treeColor.x);
        ImGui::ColorEdit3("Rock", &p.rockColor.x);
        ImGui::ColorEdit3("Ice", &p.iceColor.x);
    }

    // ── Biome Levels ────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Biome Levels")) {
        ImGui::SliderFloat("Sand Level", &p.sandLevel, 0.0f, 0.2f);
        ImGui::SliderFloat("Tree Level", &p.treeLevel, 0.0f, 0.2f);
        ImGui::SliderFloat("Rock Level", &p.rockLevel, 0.0f, 0.3f);
        ImGui::SliderFloat("Ice Level", &p.iceLevel, 0.0f, 0.4f);
        ImGui::SliderFloat("Transition", &p.transition, 0.001f, 0.1f);
    }

    // ── Clouds ──────────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Clouds")) {
        ImGui::SliderFloat("Density##clouds", &p.cloudsDensity, 0.0f, 1.0f);
        ImGui::SliderFloat("Scale##clouds", &p.cloudsScale, 0.1f, 4.0f);
        ImGui::SliderFloat("Speed##clouds", &p.cloudsSpeed, 0.0f, 5.0f);
        ImGui::SliderFloat("Altitude##clouds", &p.cloudAltitude, 0.02f, 0.5f, "%.3f");
        ImGui::SliderFloat("Thickness##clouds", &p.cloudThickness, 0.02f, 0.3f, "%.3f");
        ImGui::ColorEdit3("Cloud Color", &p.cloudColor.x);
    }

    // ── Atmosphere ──────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Atmosphere")) {
        ImGui::ColorEdit3("Atmo Color", &p.atmosphereColor.x);
        ImGui::SliderFloat("Density##atmo", &p.atmosphereDensity, 0.0f, 1.0f);
    }

    // ── Lighting ────────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Sun X", &p.sunDirection.x, -2.0f, 2.0f);
        ImGui::SliderFloat("Sun Y", &p.sunDirection.y, -2.0f, 2.0f);
        ImGui::SliderFloat("Sun Z", &p.sunDirection.z, -2.0f, 2.0f);
        ImGui::SliderFloat("Intensity", &p.sunIntensity, 0.0f, 6.0f);
        ImGui::SliderFloat("Ambient", &p.ambientLight, 0.0f, 0.2f);
        ImGui::ColorEdit3("Sun Color", &p.sunColor.x);
        ImGui::ColorEdit3("Deep Space", &p.deepSpaceColor.x);
    }

    // ── Rings ───────────────────────────────────────────────────────────
    if (ringParams && ImGui::CollapsingHeader("Rings")) {
        bool changed = false;

        if (ImGui::Checkbox("Enable Rings", &ringParams->enabled)) {
            changed = true;
        }

        if (ringParams->enabled) {
            // Global ring settings
            if (ImGui::SliderInt("Total Particles", &ringParams->totalParticles, 1000, 100000)) changed = true;
            if (ImGui::SliderFloat("Thickness", &ringParams->thickness, 0.001f, 0.1f, "%.3f")) changed = true;

            ImGui::Separator();
            ImGui::TextDisabled("Ring Bands");

            // Add new band button
            if (ImGui::Button("+ Add Band")) {
                // Default new band just outside existing rings
                float newInner = 1.5f;
                if (!ringParams->bands.empty()) {
                    newInner = ringParams->bands.back().outerRadius + 0.05f;
                }
                ringParams->addBand(newInner, newInner + 0.3f,
                                    {0.8f, 0.75f, 0.65f}, {0.6f, 0.55f, 0.5f}, 0.8f);
                changed = true;
            }

            // Edit each band
            int bandToRemove = -1;
            for (size_t i = 0; i < ringParams->bands.size(); i++) {
                auto& band = ringParams->bands[i];
                ImGui::PushID(static_cast<int>(i));

                // Band header with remove button
                char bandLabel[32];
                snprintf(bandLabel, sizeof(bandLabel), "Band %zu", i + 1);
                bool bandOpen = ImGui::TreeNodeEx(bandLabel, ImGuiTreeNodeFlags_DefaultOpen);

                ImGui::SameLine(ImGui::GetWindowWidth() - 60);
                if (ImGui::SmallButton("Remove")) {
                    bandToRemove = static_cast<int>(i);
                }

                if (bandOpen) {
                    if (ImGui::SliderFloat("Inner R", &band.innerRadius, 1.0f, 5.0f, "%.2f")) changed = true;
                    if (ImGui::SliderFloat("Outer R", &band.outerRadius, 1.0f, 5.0f, "%.2f")) changed = true;

                    // Ensure outer > inner
                    if (band.outerRadius <= band.innerRadius) {
                        band.outerRadius = band.innerRadius + 0.01f;
                    }

                    if (ImGui::SliderFloat("Opacity##band", &band.opacity, 0.0f, 1.0f)) changed = true;
                    if (ImGui::SliderFloat("Density##band", &band.density, 0.1f, 3.0f, "%.1f")) changed = true;
                    if (ImGui::ColorEdit3("Inner Color##band", &band.innerColor.x)) changed = true;
                    if (ImGui::ColorEdit3("Outer Color##band", &band.outerColor.x)) changed = true;

                    ImGui::TreePop();
                }

                ImGui::PopID();
            }

            // Remove band if requested
            if (bandToRemove >= 0 && bandToRemove < static_cast<int>(ringParams->bands.size())) {
                ringParams->bands.erase(ringParams->bands.begin() + bandToRemove);
                changed = true;
            }

            if (changed && onRingChanged) {
                onRingChanged();
            }
        }
    }

    // ── Actions ─────────────────────────────────────────────────────────
    ImGui::Separator();
    if (ImGui::Button("Reset to Defaults")) {
        p = PlanetParams{};
        m_presetIndex = 0;
    }
    ImGui::SameLine();
    if (ImGui::Button("Copy JSON")) {
        std::string jsonStr = paramsToJson(p);
        ImGui::SetClipboardText(jsonStr.c_str());
        LOG_INFO("Planet parameters copied to clipboard");
    }

    ImGui::End();
}

}  // namespace astrocore
