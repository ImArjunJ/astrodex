#include "ui/UIManager.hpp"
#include "ui/DataVisualization.hpp"
#include "render/Renderer.hpp"
#include "config/PresetManager.hpp"
#include "core/Logger.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <GLFW/glfw3.h>
#include <imgui_impl_opengl3.h>

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>

using json = nlohmann::json;

namespace astrocore {

// JSON Export
static std::string paramsToJson(const PlanetParams& p) {
    json j;
    j["radius"] = p.radius;
    j["rotationSpeed"] = p.rotationSpeed;
    j["rotationOffset"] = p.rotationOffset;
    j["quality"] = p.quality;
    j["noiseType"] = static_cast<int>(p.noiseType);
    j["noiseStrength"] = p.noiseStrength;
    j["terrainScale"] = p.terrainScale;
    j["fbmOctaves"] = p.fbmOctaves;
    j["fbmPersistence"] = p.fbmPersistence;
    j["fbmLacunarity"] = p.fbmLacunarity;
    j["fbmExponentiation"] = p.fbmExponentiation;
    j["domainWarpStrength"] = p.domainWarpStrength;
    j["ridgedStrength"] = p.ridgedStrength;
    j["craterStrength"] = p.craterStrength;
    j["continentScale"] = p.continentScale;
    j["continentBlend"] = p.continentBlend;
    j["waterLevel"] = p.waterLevel;
    j["polarCapSize"] = p.polarCapSize;
    j["bandingStrength"] = p.bandingStrength;
    j["bandingFrequency"] = p.bandingFrequency;
    j["waterColorDeep"] = {p.waterColorDeep.x, p.waterColorDeep.y, p.waterColorDeep.z};
    j["waterColorSurface"] = {p.waterColorSurface.x, p.waterColorSurface.y, p.waterColorSurface.z};
    j["sandColor"] = {p.sandColor.x, p.sandColor.y, p.sandColor.z};
    j["treeColor"] = {p.treeColor.x, p.treeColor.y, p.treeColor.z};
    j["rockColor"] = {p.rockColor.x, p.rockColor.y, p.rockColor.z};
    j["iceColor"] = {p.iceColor.x, p.iceColor.y, p.iceColor.z};
    j["sandLevel"] = p.sandLevel;
    j["treeLevel"] = p.treeLevel;
    j["rockLevel"] = p.rockLevel;
    j["iceLevel"] = p.iceLevel;
    j["transition"] = p.transition;
    j["cloudsDensity"] = p.cloudsDensity;
    j["cloudsScale"] = p.cloudsScale;
    j["cloudsSpeed"] = p.cloudsSpeed;
    j["cloudAltitude"] = p.cloudAltitude;
    j["cloudThickness"] = p.cloudThickness;
    j["cloudColor"] = {p.cloudColor.x, p.cloudColor.y, p.cloudColor.z};
    j["atmosphereColor"] = {p.atmosphereColor.x, p.atmosphereColor.y, p.atmosphereColor.z};
    j["atmosphereDensity"] = p.atmosphereDensity;
    j["sunDirection"] = {p.sunDirection.x, p.sunDirection.y, p.sunDirection.z};
    j["sunIntensity"] = p.sunIntensity;
    j["ambientLight"] = p.ambientLight;
    j["sunColor"] = {p.sunColor.x, p.sunColor.y, p.sunColor.z};
    j["deepSpaceColor"] = {p.deepSpaceColor.x, p.deepSpaceColor.y, p.deepSpaceColor.z};
    return j.dump(2);
}

// Presets - exact values from ~/earthlike, ~/marslike, etc.
static PlanetParams makePreset(int index) {
    PlanetParams p{};
    switch (index) {
    case 0: // Earth (from ~/earthlike)
        p.ambientLight       = 0.005f;
        p.atmosphereColor    = {0.4f, 0.65f, 1.0f};
        p.atmosphereDensity  = 0.35f;
        p.bandingFrequency   = 20.0f;
        p.bandingStrength    = 0.0f;
        p.cloudAltitude      = 0.15f;
        p.cloudColor         = {1.0f, 1.0f, 1.0f};
        p.cloudThickness     = 0.1f;
        p.cloudsDensity      = 0.0f;
        p.cloudsScale        = 1.0f;
        p.cloudsSpeed        = 1.5f;
        p.continentBlend     = 0.5f;
        p.continentScale     = 0.33f;
        p.craterStrength     = 0.0f;
        p.deepSpaceColor     = {0.0f, 0.0f, 0.001f};
        p.domainWarpStrength = 0.0f;
        p.fbmExponentiation  = 4.0f;
        p.fbmLacunarity      = 2.0f;
        p.fbmOctaves         = 6;
        p.fbmPersistence     = 0.5f;
        p.iceColor           = {0.9f, 0.93f, 0.97f};
        p.iceLevel           = 0.083f;
        p.noiseStrength      = 0.04f;
        p.noiseType          = NoiseType::Hybrid;
        p.polarCapSize       = 0.12f;
        p.quality            = 1.0f;
        p.radius             = 1.0f;
        p.ridgedStrength     = 0.0f;
        p.rockColor          = {0.4f, 0.35f, 0.3f};
        p.rockLevel          = 0.034f;
        p.rotationOffset     = 0.6f;
        p.rotationSpeed      = 0.1f;
        p.sandColor          = {0.76f, 0.7f, 0.5f};
        p.sandLevel          = 0.001f;
        p.sunColor           = {1.0f, 1.0f, 0.9f};
        p.sunDirection       = {1.0f, 1.0f, 0.5f};
        p.sunIntensity       = 1.541f;
        p.terrainScale       = 0.1f;
        p.transition         = 0.005f;
        p.treeColor          = {0.13f, 0.35f, 0.13f};
        p.treeLevel          = 0.0f;
        p.waterColorDeep     = {0.01f, 0.03f, 0.12f};
        p.waterColorSurface  = {0.02f, 0.08f, 0.25f};
        p.waterLevel         = 0.02f;
        break;
    case 1: // Mars (from ~/marslike)
        p.ambientLight       = 0.01f;
        p.atmosphereColor    = {0.4336283207f, 0.8496358395f, 1.0f};
        p.atmosphereDensity  = 0.05f;
        p.bandingFrequency   = 20.0f;
        p.bandingStrength    = 0.0f;
        p.cloudAltitude      = 0.15f;
        p.cloudColor         = {1.0f, 1.0f, 1.0f};
        p.cloudThickness     = 0.1f;
        p.cloudsDensity      = 0.05f;
        p.cloudsScale        = 1.0f;
        p.cloudsSpeed        = 1.5f;
        p.continentBlend     = 0.15f;
        p.continentScale     = 0.0f;
        p.craterStrength     = 0.7f;
        p.deepSpaceColor     = {0.0f, 0.0f, 0.001f};
        p.domainWarpStrength = 0.0f;
        p.fbmExponentiation  = 8.0f;
        p.fbmLacunarity      = 2.0f;
        p.fbmOctaves         = 6;
        p.fbmPersistence     = 0.6f;
        p.iceColor           = {0.9f, 0.85f, 0.8f};
        p.iceLevel           = 0.04f;
        p.noiseStrength      = 0.103f;
        p.noiseType          = NoiseType::Hybrid;
        p.polarCapSize       = 0.173f;
        p.quality            = 1.0f;
        p.radius             = 0.532f;
        p.ridgedStrength     = 0.2f;
        p.rockColor          = {0.5f, 0.3f, 0.2f};
        p.rockLevel          = 0.02f;
        p.rotationOffset     = 0.6f;
        p.rotationSpeed      = 0.1f;
        p.sandColor          = {0.76f, 0.5f, 0.3f};
        p.sandLevel          = 0.003f;
        p.sunColor           = {1.0f, 1.0f, 0.9f};
        p.sunDirection       = {1.0f, 1.0f, 0.5f};
        p.sunIntensity       = 2.5f;
        p.terrainScale       = 0.1f;
        p.transition         = 0.01f;
        p.treeColor          = {0.45f, 0.2137168199f, 0.1500000358f};
        p.treeLevel          = 0.004f;
        p.waterColorDeep     = {0.2784313858f, 0.0509803928f, 0.0196078438f};
        p.waterColorSurface  = {0.25f, 0.12f, 0.06f};
        p.waterLevel         = 0.3f;
        break;
    case 2: // Venus (from ~/venuslike)
        p.ambientLight       = 0.01f;
        p.atmosphereColor    = {0.1991150379f, 0.0710445717f, 0.0308363941f};
        p.atmosphereDensity  = 1.0f;
        p.bandingFrequency   = 5.0f;
        p.bandingStrength    = 0.005f;
        p.cloudAltitude      = 0.15f;
        p.cloudColor         = {0.95f, 0.85f, 0.5f};
        p.cloudThickness     = 0.1f;
        p.cloudsDensity      = 0.0f;
        p.cloudsScale        = 1.5f;
        p.cloudsSpeed        = 1.5f;
        p.continentBlend     = 0.15f;
        p.continentScale     = 1.0f;
        p.craterStrength     = 0.0f;
        p.deepSpaceColor     = {0.0f, 0.0f, 0.001f};
        p.domainWarpStrength = 0.0f;
        p.fbmExponentiation  = 5.4f;
        p.fbmLacunarity      = 1.96f;
        p.fbmOctaves         = 8;
        p.fbmPersistence     = 0.9f;
        p.iceColor           = {0.3716813922f, 0.2015154064f, 0.1118333414f};
        p.iceLevel           = 0.04f;
        p.noiseStrength      = 0.225f;
        p.noiseType          = NoiseType::Hybrid;
        p.polarCapSize       = 0.15f;
        p.quality            = 1.0f;
        p.radius             = 0.949f;
        p.ridgedStrength     = 0.0f;
        p.rockColor          = {1.0f, 1.0f, 1.0f};
        p.rockLevel          = 0.02f;
        p.rotationOffset     = 0.6f;
        p.rotationSpeed      = 0.054f;
        p.sandColor          = {1.0f, 1.0f, 1.0f};
        p.sandLevel          = 0.003f;
        p.sunColor           = {1.0f, 1.0f, 0.9f};
        p.sunDirection       = {1.0f, 1.0f, 0.5f};
        p.sunIntensity       = 3.0f;
        p.terrainScale       = 0.1f;
        p.transition         = 0.01f;
        p.treeColor          = {0.7831858397f, 0.4980759323f, 0.1559440792f};
        p.treeLevel          = 0.004f;
        p.waterColorDeep     = {0.1194690466f, 0.0412326753f, 0.0f};
        p.waterColorSurface  = {0.8318583965f, 0.3703590930f, 0.1545931697f};
        p.waterLevel         = 0.35f;
        break;
    case 3: // Mercury (from ~/mercurylike)
        p.ambientLight       = 0.015f;
        p.atmosphereColor    = {0.3f, 0.3f, 0.3f};
        p.atmosphereDensity  = 0.0f;
        p.bandingFrequency   = 5.0f;
        p.bandingStrength    = 0.0f;
        p.cloudAltitude      = 0.15f;
        p.cloudColor         = {0.5f, 0.5f, 0.5f};
        p.cloudThickness     = 0.1f;
        p.cloudsDensity      = 0.0f;
        p.cloudsScale        = 1.0f;
        p.cloudsSpeed        = 0.0f;
        p.continentBlend     = 0.1f;
        p.continentScale     = 0.0f;
        p.craterStrength     = 0.85f;
        p.deepSpaceColor     = {0.0f, 0.0f, 0.001f};
        p.domainWarpStrength = 0.0f;
        p.fbmExponentiation  = 3.0f;
        p.fbmLacunarity      = 2.0f;
        p.fbmOctaves         = 6;
        p.fbmPersistence     = 0.55f;
        p.iceColor           = {0.55f, 0.53f, 0.50f};
        p.iceLevel           = 0.1f;
        p.noiseStrength      = 0.035f;
        p.noiseType          = NoiseType::Hybrid;
        p.polarCapSize       = 0.0f;
        p.quality            = 1.0f;
        p.radius             = 0.383f;
        p.ridgedStrength     = 0.0f;
        p.rockColor          = {0.25f, 0.24f, 0.22f};
        p.rockLevel          = 0.02f;
        p.rotationOffset     = 0.0f;
        p.rotationSpeed      = 0.02f;
        p.sandColor          = {0.45f, 0.42f, 0.40f};
        p.sandLevel          = 0.005f;
        p.sunColor           = {1.0f, 1.0f, 0.9f};
        p.sunDirection       = {1.0f, 1.0f, 0.5f};
        p.sunIntensity       = 4.0f;
        p.terrainScale       = 0.15f;
        p.transition         = 0.008f;
        p.treeColor          = {0.35f, 0.33f, 0.30f};
        p.treeLevel          = 0.01f;
        p.waterColorDeep     = {0.2f, 0.19f, 0.18f};
        p.waterColorSurface  = {0.3f, 0.28f, 0.26f};
        p.waterLevel         = -0.5f;
        break;
    case 4: // Jupiter (from ~/jupiterlike)
        p.ambientLight       = 0.01f;
        p.atmosphereColor    = {0.6f, 0.4f, 0.25f};
        p.atmosphereDensity  = 0.5f;
        p.bandingFrequency   = 30.0f;
        p.bandingStrength    = 0.85f;
        p.cloudAltitude      = 0.15f;
        p.cloudColor         = {0.9f, 0.85f, 0.7f};
        p.cloudThickness     = 0.1f;
        p.cloudsDensity      = 0.0f;
        p.cloudsScale        = 2.5f;
        p.cloudsSpeed        = 1.5f;
        p.continentBlend     = 0.15f;
        p.continentScale     = 1.0f;
        p.craterStrength     = 0.0f;
        p.deepSpaceColor     = {0.0f, 0.0f, 0.001f};
        p.domainWarpStrength = 0.4f;
        p.fbmExponentiation  = 5.0f;
        p.fbmLacunarity      = 2.0f;
        p.fbmOctaves         = 6;
        p.fbmPersistence     = 0.5f;
        p.iceColor           = {0.95f, 0.9f, 0.8f};
        p.iceLevel           = 0.04f;
        p.noiseStrength      = 0.008f;
        p.noiseType          = NoiseType::Standard;
        p.polarCapSize       = 0.15f;
        p.quality            = 1.0f;
        p.radius             = 10.97f;
        p.ridgedStrength     = 0.0f;
        p.rockColor          = {0.2787610888f, 0.1115044281f, 0.0f};
        p.rockLevel          = 0.02f;
        p.rotationOffset     = 0.6f;
        p.rotationSpeed      = 0.1f;
        p.sandColor          = {0.85f, 0.75f, 0.6f};
        p.sandLevel          = 0.003f;
        p.sunColor           = {1.0f, 1.0f, 0.9f};
        p.sunDirection       = {1.0f, 1.0f, 0.5f};
        p.sunIntensity       = 3.0f;
        p.terrainScale       = 0.8f;
        p.transition         = 0.01f;
        p.treeColor          = {0.8008849621f, 0.4809359908f, 0.2409742475f};
        p.treeLevel          = 0.004f;
        p.waterColorDeep     = {0.01f, 0.05f, 0.15f};
        p.waterColorSurface  = {0.02f, 0.12f, 0.27f};
        p.waterLevel         = -1.0f;
        break;
    case 5: // Saturn (from ~/saturnlike)
        p.ambientLight       = 0.01f;
        p.atmosphereColor    = {0.8f, 0.7f, 0.4f};
        p.atmosphereDensity  = 0.45f;
        p.bandingFrequency   = 25.0f;
        p.bandingStrength    = 0.7f;
        p.cloudAltitude      = 0.15f;
        p.cloudColor         = {0.95f, 0.9f, 0.75f};
        p.cloudThickness     = 0.1f;
        p.cloudsDensity      = 0.0f;
        p.cloudsScale        = 2.0f;
        p.cloudsSpeed        = 1.2f;
        p.continentBlend     = 0.15f;
        p.continentScale     = 1.0f;
        p.craterStrength     = 0.0f;
        p.deepSpaceColor     = {0.0f, 0.0f, 0.001f};
        p.domainWarpStrength = 0.3f;
        p.fbmExponentiation  = 4.5f;
        p.fbmLacunarity      = 2.0f;
        p.fbmOctaves         = 6;
        p.fbmPersistence     = 0.5f;
        p.iceColor           = {0.95f, 0.92f, 0.8f};
        p.iceLevel           = 0.04f;
        p.noiseStrength      = 0.006f;
        p.noiseType          = NoiseType::Standard;
        p.polarCapSize       = 0.15f;
        p.quality            = 1.0f;
        p.radius             = 9.14f;
        p.ridgedStrength     = 0.0f;
        p.rockColor          = {0.7f, 0.6f, 0.45f};
        p.rockLevel          = 0.02f;
        p.rotationOffset     = 0.0f;
        p.rotationSpeed      = 0.12f;
        p.sandColor          = {0.9f, 0.85f, 0.65f};
        p.sandLevel          = 0.003f;
        p.sunColor           = {1.0f, 1.0f, 0.9f};
        p.sunDirection       = {1.0f, 1.0f, 0.5f};
        p.sunIntensity       = 2.5f;
        p.terrainScale       = 0.8f;
        p.transition         = 0.01f;
        p.treeColor          = {0.8f, 0.7f, 0.5f};
        p.treeLevel          = 0.004f;
        p.waterColorDeep     = {0.01f, 0.05f, 0.15f};
        p.waterColorSurface  = {0.02f, 0.12f, 0.27f};
        p.waterLevel         = -1.0f;
        break;
    case 6: // Neptune (from ~/neptunelike)
        p.ambientLight       = 0.01f;
        p.atmosphereColor    = {0.2f, 0.4f, 0.85f};
        p.atmosphereDensity  = 0.5f;
        p.bandingFrequency   = 18.0f;
        p.bandingStrength    = 0.4f;
        p.cloudAltitude      = 0.15f;
        p.cloudColor         = {0.5f, 0.65f, 0.9f};
        p.cloudThickness     = 0.1f;
        p.cloudsDensity      = 0.184f;
        p.cloudsScale        = 1.8f;
        p.cloudsSpeed        = 1.5f;
        p.continentBlend     = 0.15f;
        p.continentScale     = 1.0f;
        p.craterStrength     = 0.0f;
        p.deepSpaceColor     = {0.0f, 0.0f, 0.001f};
        p.domainWarpStrength = 0.25f;
        p.fbmExponentiation  = 5.0f;
        p.fbmLacunarity      = 2.0f;
        p.fbmOctaves         = 6;
        p.fbmPersistence     = 0.5f;
        p.iceColor           = {0.4f, 0.55f, 0.9f};
        p.iceLevel           = 0.04f;
        p.noiseStrength      = 0.005f;
        p.noiseType          = NoiseType::Standard;
        p.polarCapSize       = 0.15f;
        p.quality            = 1.0f;
        p.radius             = 3.86f;
        p.ridgedStrength     = 0.0f;
        p.rockColor          = {0.15f, 0.3f, 0.6f};
        p.rockLevel          = 0.02f;
        p.rotationOffset     = 0.6f;
        p.rotationSpeed      = 0.1f;
        p.sandColor          = {0.25f, 0.4f, 0.8f};
        p.sandLevel          = 0.003f;
        p.sunColor           = {1.0f, 1.0f, 0.9f};
        p.sunDirection       = {1.0f, 1.0f, 0.5f};
        p.sunIntensity       = 3.0f;
        p.terrainScale       = 0.8f;
        p.transition         = 0.01f;
        p.treeColor          = {0.2f, 0.35f, 0.7f};
        p.treeLevel          = 0.004f;
        p.waterColorDeep     = {0.01f, 0.05f, 0.15f};
        p.waterColorSurface  = {0.02f, 0.12f, 0.27f};
        p.waterLevel         = -1.0f;
        break;
    case 7: // Uranus (from ~/uranuslike)
        p.ambientLight       = 0.01f;
        p.atmosphereColor    = {0.5f, 0.8f, 0.85f};
        p.atmosphereDensity  = 0.4f;
        p.bandingFrequency   = 12.0f;
        p.bandingStrength    = 0.15f;
        p.cloudAltitude      = 0.15f;
        p.cloudColor         = {0.7f, 0.85f, 0.88f};
        p.cloudThickness     = 0.1f;
        p.cloudsDensity      = 0.08f;
        p.cloudsScale        = 1.5f;
        p.cloudsSpeed        = 0.8f;
        p.continentBlend     = 0.15f;
        p.continentScale     = 1.0f;
        p.craterStrength     = 0.0f;
        p.deepSpaceColor     = {0.0f, 0.0f, 0.001f};
        p.domainWarpStrength = 0.1f;
        p.fbmExponentiation  = 4.0f;
        p.fbmLacunarity      = 2.0f;
        p.fbmOctaves         = 6;
        p.fbmPersistence     = 0.5f;
        p.iceColor           = {0.7f, 0.88f, 0.92f};
        p.iceLevel           = 0.04f;
        p.noiseStrength      = 0.003f;
        p.noiseType          = NoiseType::Standard;
        p.polarCapSize       = 0.15f;
        p.quality            = 1.0f;
        p.radius             = 3.98f;
        p.ridgedStrength     = 0.0f;
        p.rockColor          = {0.5f, 0.7f, 0.75f};
        p.rockLevel          = 0.02f;
        p.rotationOffset     = 0.0f;
        p.rotationSpeed      = 0.08f;
        p.sandColor          = {0.6f, 0.8f, 0.85f};
        p.sandLevel          = 0.003f;
        p.sunColor           = {1.0f, 1.0f, 0.9f};
        p.sunDirection       = {1.0f, 1.0f, 0.5f};
        p.sunIntensity       = 2.0f;
        p.terrainScale       = 0.8f;
        p.transition         = 0.01f;
        p.treeColor          = {0.55f, 0.75f, 0.8f};
        p.treeLevel          = 0.004f;
        p.waterColorDeep     = {0.01f, 0.05f, 0.15f};
        p.waterColorSurface  = {0.02f, 0.12f, 0.27f};
        p.waterLevel         = -1.0f;
        break;
    }
    return p;
}

// UIManager

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
    ImGui_ImplOpenGL3_Init("#version 410");

    setupStyle();
    refreshCachedPlanets();

    // Initialize data visualization
    m_dataViz = std::make_unique<DataVisualization>();
    m_dataViz->init();
    m_dataViz->applyTheme(m_theme == Theme::Dark);

    m_initialized = true;
    LOG_INFO("ImGui initialized (OpenGL backend)");
}

void UIManager::shutdown() {
    if (m_dataViz) {
        m_dataViz->shutdown();
        m_dataViz.reset();
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    m_initialized = false;
}

void UIManager::beginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void UIManager::endFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void UIManager::refreshCachedPlanets() {
    m_cachedPlanets = ExoplanetConverter::listCachedPlanets();
    m_cachedSelectedIndex = -1;
    LOG_INFO("Loaded {} cached planets", static_cast<int>(m_cachedPlanets.size()));
}

void UIManager::setupStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark();

    // Shape (shared)
    style.WindowRounding    = 10.0f;
    style.ChildRounding     =  8.0f;
    style.FrameRounding     =  6.0f;
    style.GrabRounding      =  6.0f;
    style.PopupRounding     =  8.0f;
    style.ScrollbarRounding =  6.0f;
    style.TabRounding       =  6.0f;
    style.WindowBorderSize  =  1.0f;
    style.FrameBorderSize   =  0.0f;
    style.WindowPadding     = ImVec2(12, 10);
    style.FramePadding      = ImVec2( 7,  4);
    style.ItemSpacing       = ImVec2( 8,  6);
    style.ScrollbarSize     = 10.0f;

    ImVec4* c = style.Colors;

    if (m_theme == Theme::Dark) {
        // Space grey dark
        c[ImGuiCol_WindowBg]             = ImVec4(0.14f, 0.14f, 0.17f, 0.68f);
        c[ImGuiCol_ChildBg]              = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        c[ImGuiCol_PopupBg]              = ImVec4(0.15f, 0.15f, 0.18f, 0.94f);

        c[ImGuiCol_Border]               = ImVec4(0.36f, 0.38f, 0.46f, 0.50f);
        c[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

        c[ImGuiCol_TitleBg]              = ImVec4(0.12f, 0.12f, 0.15f, 0.80f);
        c[ImGuiCol_TitleBgActive]        = ImVec4(0.15f, 0.15f, 0.18f, 0.88f);
        c[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.10f, 0.10f, 0.12f, 0.60f);

        c[ImGuiCol_FrameBg]              = ImVec4(0.18f, 0.18f, 0.22f, 0.55f);
        c[ImGuiCol_FrameBgHovered]       = ImVec4(0.24f, 0.24f, 0.29f, 0.68f);
        c[ImGuiCol_FrameBgActive]        = ImVec4(0.28f, 0.28f, 0.34f, 0.80f);

        c[ImGuiCol_Header]               = ImVec4(0.22f, 0.24f, 0.30f, 0.50f);
        c[ImGuiCol_HeaderHovered]        = ImVec4(0.28f, 0.30f, 0.38f, 0.62f);
        c[ImGuiCol_HeaderActive]         = ImVec4(0.32f, 0.34f, 0.44f, 0.72f);

        c[ImGuiCol_Tab]                  = ImVec4(0.14f, 0.14f, 0.17f, 0.55f);
        c[ImGuiCol_TabHovered]           = ImVec4(0.24f, 0.26f, 0.34f, 0.70f);
        c[ImGuiCol_TabActive]            = ImVec4(0.30f, 0.32f, 0.42f, 0.88f);
        c[ImGuiCol_TabUnfocused]         = ImVec4(0.10f, 0.10f, 0.13f, 0.40f);
        c[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.18f, 0.18f, 0.23f, 0.60f);

        c[ImGuiCol_SliderGrab]           = ImVec4(0.42f, 0.52f, 0.74f, 0.90f);
        c[ImGuiCol_SliderGrabActive]     = ImVec4(0.54f, 0.66f, 0.90f, 1.00f);

        c[ImGuiCol_ScrollbarBg]          = ImVec4(0.00f, 0.00f, 0.00f, 0.15f);
        c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.30f, 0.32f, 0.40f, 0.50f);
        c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.38f, 0.40f, 0.50f, 0.65f);
        c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.46f, 0.48f, 0.60f, 0.80f);

        c[ImGuiCol_Button]               = ImVec4(0.20f, 0.22f, 0.30f, 0.65f);
        c[ImGuiCol_ButtonHovered]        = ImVec4(0.26f, 0.28f, 0.38f, 0.78f);
        c[ImGuiCol_ButtonActive]         = ImVec4(0.22f, 0.24f, 0.34f, 0.88f);

        c[ImGuiCol_Text]                 = ImVec4(0.90f, 0.90f, 0.94f, 1.00f);
        c[ImGuiCol_TextDisabled]         = ImVec4(0.52f, 0.54f, 0.62f, 0.85f);

        c[ImGuiCol_CheckMark]            = ImVec4(0.52f, 0.66f, 0.92f, 1.00f);
        c[ImGuiCol_Separator]            = ImVec4(0.28f, 0.30f, 0.38f, 0.38f);
        c[ImGuiCol_SeparatorHovered]     = ImVec4(0.38f, 0.40f, 0.50f, 0.55f);
        c[ImGuiCol_SeparatorActive]      = ImVec4(0.48f, 0.50f, 0.62f, 0.72f);

    } else { // Theme::Light
        // Atmosphere glass
        c[ImGuiCol_WindowBg]             = ImVec4(0.52f, 0.78f, 1.00f, 0.12f);
        c[ImGuiCol_ChildBg]              = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        c[ImGuiCol_PopupBg]              = ImVec4(0.18f, 0.25f, 0.38f, 0.94f);

        c[ImGuiCol_Border]               = ImVec4(0.60f, 0.82f, 1.00f, 0.28f);
        c[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

        c[ImGuiCol_TitleBg]              = ImVec4(0.50f, 0.76f, 1.00f, 0.10f);
        c[ImGuiCol_TitleBgActive]        = ImVec4(0.52f, 0.78f, 1.00f, 0.13f);
        c[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.46f, 0.70f, 0.96f, 0.08f);

        c[ImGuiCol_FrameBg]              = ImVec4(0.55f, 0.80f, 1.00f, 0.14f);
        c[ImGuiCol_FrameBgHovered]       = ImVec4(0.60f, 0.85f, 1.00f, 0.24f);
        c[ImGuiCol_FrameBgActive]        = ImVec4(0.64f, 0.88f, 1.00f, 0.34f);

        c[ImGuiCol_Header]               = ImVec4(0.52f, 0.78f, 1.00f, 0.18f);
        c[ImGuiCol_HeaderHovered]        = ImVec4(0.58f, 0.84f, 1.00f, 0.28f);
        c[ImGuiCol_HeaderActive]         = ImVec4(0.64f, 0.90f, 1.00f, 0.38f);

        c[ImGuiCol_Tab]                  = ImVec4(0.48f, 0.74f, 1.00f, 0.14f);
        c[ImGuiCol_TabHovered]           = ImVec4(0.55f, 0.82f, 1.00f, 0.28f);
        c[ImGuiCol_TabActive]            = ImVec4(0.60f, 0.86f, 1.00f, 0.48f);
        c[ImGuiCol_TabUnfocused]         = ImVec4(0.42f, 0.68f, 0.94f, 0.08f);
        c[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.50f, 0.78f, 1.00f, 0.26f);

        c[ImGuiCol_SliderGrab]           = ImVec4(0.58f, 0.84f, 1.00f, 0.85f);
        c[ImGuiCol_SliderGrabActive]     = ImVec4(0.72f, 0.92f, 1.00f, 1.00f);

        c[ImGuiCol_ScrollbarBg]          = ImVec4(0.48f, 0.74f, 1.00f, 0.06f);
        c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.52f, 0.80f, 1.00f, 0.28f);
        c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.60f, 0.86f, 1.00f, 0.42f);
        c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.70f, 0.92f, 1.00f, 0.58f);

        c[ImGuiCol_Button]               = ImVec4(0.50f, 0.78f, 1.00f, 0.18f);
        c[ImGuiCol_ButtonHovered]        = ImVec4(0.58f, 0.84f, 1.00f, 0.30f);
        c[ImGuiCol_ButtonActive]         = ImVec4(0.64f, 0.90f, 1.00f, 0.40f);

        c[ImGuiCol_Text]                 = ImVec4(0.92f, 0.96f, 1.00f, 1.00f);
        c[ImGuiCol_TextDisabled]         = ImVec4(0.68f, 0.84f, 1.00f, 0.78f);

        c[ImGuiCol_CheckMark]            = ImVec4(0.78f, 0.94f, 1.00f, 1.00f);
        c[ImGuiCol_Separator]            = ImVec4(0.56f, 0.82f, 1.00f, 0.22f);
        c[ImGuiCol_SeparatorHovered]     = ImVec4(0.66f, 0.88f, 1.00f, 0.38f);
        c[ImGuiCol_SeparatorActive]      = ImVec4(0.76f, 0.94f, 1.00f, 0.55f);
    }
}

void UIManager::render(PlanetParams& p, ImVec2* outPos, ImVec2* outSize) {
    ImGui::SetNextWindowSize(ImVec2(340, 720), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(10, 10),  ImGuiCond_Always);

    if (!ImGui::Begin("Planet Editor")) {
        if (outPos)  *outPos  = ImGui::GetWindowPos();
        if (outSize) *outSize = ImGui::GetWindowSize();
        ImGui::End();
        return;
    }

    // Back to Galaxy button
    {
        ImGui::PushStyleColor(ImGuiCol_Button,
            ImVec4(0.08f, 0.20f, 0.40f, 0.70f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            ImVec4(0.14f, 0.32f, 0.60f, 0.85f));
        if (ImGui::SmallButton("  Back to Galaxy  "))
            m_backPressed = true;
        ImGui::PopStyleColor(2);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Tab bar
    if (ImGui::BeginTabBar("##tabs")) {

        // DATA tab
        if (ImGui::BeginTabItem("  DATA  ")) {
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.88f, 1.0f, 1.0f));
            ImGui::TextWrapped("%s", m_exoStatus.c_str());
            ImGui::PopStyleColor();
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            auto row = [](const char* label, const char* value, bool ai) {
                ImGui::TextDisabled("%s", label);
                ImGui::SameLine(110.f);
                ImGui::Text("%s", value);
                if (ai) {
                    ImGui::SameLine();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.75f, 1.0f, 0.90f));
                    ImGui::Text(" AI");
                    ImGui::PopStyleColor();
                }
            };

            auto rowFmt = [](const char* label, bool ai, const char* fmt, auto... args) {
                char buf[64];
                snprintf(buf, sizeof(buf), fmt, args...);
                ImGui::TextDisabled("%s", label);
                ImGui::SameLine(110.f);
                ImGui::Text("%s", buf);
                if (ai) {
                    ImGui::SameLine();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.75f, 1.0f, 0.90f));
                    ImGui::Text(" AI");
                    ImGui::PopStyleColor();
                }
            };

            const auto& d = m_currentExoData;

            ImGui::TextDisabled("CLASSIFICATION");
            ImGui::Spacing();
            if (m_hasExoData && d.planet_type.hasValue())
                row("Type", d.planet_type.value.c_str(), d.planet_type.isAIInferred());
            else
                row("Type", "--", false);

            if (m_hasExoData && !d.host_star.name.empty())
                row("Host Star", d.host_star.name.c_str(), false);
            else
                row("Host Star", "--", false);

            if (m_hasExoData && d.distance_ly.hasValue())
                rowFmt("Distance", false, "%.1f ly", d.distance_ly.value);
            else if (m_hasExoData && d.host_star.distance_pc.hasValue())
                rowFmt("Distance", false, "%.1f ly", d.host_star.distance_pc.value * 3.26156);
            else
                row("Distance", "--", false);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::TextDisabled("PHYSICAL");
            ImGui::Spacing();
            if (m_hasExoData && d.mass_earth.hasValue())
                rowFmt("Mass", d.mass_earth.isAIInferred(), "%.2f M⊕", d.mass_earth.value);
            else
                row("Mass", "--", false);

            if (m_hasExoData && d.radius_earth.hasValue())
                rowFmt("Radius", d.radius_earth.isAIInferred(), "%.2f R⊕", d.radius_earth.value);
            else
                row("Radius", "--", false);

            if (m_hasExoData && d.surface_gravity_g.hasValue())
                rowFmt("Gravity", d.surface_gravity_g.isAIInferred(), "%.2f g", d.surface_gravity_g.value);
            else
                row("Gravity", "--", false);

            if (m_hasExoData && d.density_gcc.hasValue())
                rowFmt("Density", d.density_gcc.isAIInferred(), "%.2f g/cm³", d.density_gcc.value);
            else
                row("Density", "--", false);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::TextDisabled("ORBIT");
            ImGui::Spacing();
            if (m_hasExoData && d.orbital_period_days.hasValue())
                rowFmt("Period", d.orbital_period_days.isAIInferred(), "%.2f days", d.orbital_period_days.value);
            else
                row("Period", "--", false);

            if (m_hasExoData && d.semi_major_axis_au.hasValue())
                rowFmt("Semi-major", d.semi_major_axis_au.isAIInferred(), "%.4f AU", d.semi_major_axis_au.value);
            else
                row("Semi-major", "--", false);

            if (m_hasExoData && d.eccentricity.hasValue())
                rowFmt("Eccentricity", d.eccentricity.isAIInferred(), "%.3f", d.eccentricity.value);
            else
                row("Eccentricity", "--", false);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::TextDisabled("ENVIRONMENT");
            ImGui::Spacing();
            if (m_hasExoData && d.equilibrium_temp_k.hasValue())
                rowFmt("Eq. Temp", d.equilibrium_temp_k.isAIInferred(), "%.0f K", d.equilibrium_temp_k.value);
            else
                row("Eq. Temp", "--", false);

            if (m_hasExoData && d.insolation_flux.hasValue())
                rowFmt("Insolation", d.insolation_flux.isAIInferred(), "%.2f S⊕", d.insolation_flux.value);
            else
                row("Insolation", "--", false);

            if (m_hasExoData && d.ocean_coverage_fraction.hasValue())
                rowFmt("Water", d.ocean_coverage_fraction.isAIInferred(), "%.0f%%", d.ocean_coverage_fraction.value * 100.0);
            else
                row("Water", "--", false);

            if (m_hasExoData && d.earth_similarity_index.hasValue())
                rowFmt("ESI", d.earth_similarity_index.isAIInferred(), "%.2f", d.earth_similarity_index.value);
            else
                row("ESI", "--", false);

            ImGui::EndTabItem();
        }

        // GRAPHS tab
        if (ImGui::BeginTabItem(" GRAPHS ")) {
            if (m_dataViz) {
                m_dataViz->renderGraphsTab();
            } else {
                ImGui::TextDisabled("Data visualization not available");
            }
            ImGui::EndTabItem();
        }

        // WORLD tab
        if (ImGui::BeginTabItem(" WORLD  ")) {
            ImGui::Spacing();
            ImGui::TextDisabled("Planet");
            ImGui::SliderFloat("Radius",        &p.radius,        0.5f,  5.0f);
            ImGui::SliderFloat("Rot Offset",    &p.rotationOffset, 0.0f, 6.28f);
            ImGui::SliderFloat("Quality",       &p.quality,       0.0f,  2.0f);
            ImGui::SliderFloat("Rot Speed",     &p.rotationSpeed, 0.0f,  1.0f);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextDisabled("Terrain");
            ImGui::SliderFloat("Noise",         &p.noiseStrength,     0.0f, 0.5f);
            ImGui::SliderFloat("Scale",         &p.terrainScale,      0.1f, 3.0f);
            ImGui::SliderFloat("Water Level",   &p.waterLevel,       -0.2f, 0.3f);

            ImGui::Spacing();
            ImGui::TextDisabled("Noise Shape");
            ImGui::SliderInt  ("Octaves",       &p.fbmOctaves,         1,    8);
            ImGui::SliderFloat("Persistence",   &p.fbmPersistence,    0.1f, 0.9f, "%.2f");
            ImGui::SliderFloat("Lacunarity",    &p.fbmLacunarity,     1.0f, 4.0f, "%.2f");
            ImGui::SliderFloat("Exponent",      &p.fbmExponentiation, 0.5f,10.0f, "%.1f");
            ImGui::SliderFloat("Domain Warp",   &p.domainWarpStrength,0.0f, 2.0f, "%.2f");

            ImGui::Spacing();
            ImGui::TextDisabled("Features");
            ImGui::SliderFloat("Ridged",        &p.ridgedStrength,  0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Craters",       &p.craterStrength,  0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Continents",    &p.continentScale,  0.0f, 3.0f, "%.2f");

            ImGui::Spacing();
            ImGui::TextDisabled("Latitude");
            ImGui::SliderFloat("Polar Cap",     &p.polarCapSize,    0.0f, 1.0f);
            ImGui::SliderFloat("Banding",       &p.bandingStrength, 0.0f, 1.0f);
            ImGui::SliderFloat("Band Freq",     &p.bandingFrequency,5.0f,50.0f,"%.0f");

            ImGui::Spacing();
            ImGui::TextDisabled("Biome Levels");
            ImGui::SliderFloat("Sand",          &p.sandLevel,  0.0f, 0.2f);
            ImGui::SliderFloat("Trees",         &p.treeLevel,  0.0f, 0.2f);
            ImGui::SliderFloat("Rock",          &p.rockLevel,  0.0f, 0.3f);
            ImGui::SliderFloat("Ice",           &p.iceLevel,   0.0f, 0.4f);
            ImGui::SliderFloat("Transition",    &p.transition, 0.001f, 0.1f);
            ImGui::EndTabItem();
        }

        // VISUAL tab
        if (ImGui::BeginTabItem(" VISUAL ")) {
            ImGui::Spacing();
            ImGui::TextDisabled("Surface");
            ImGui::ColorEdit3("Water Deep",    &p.waterColorDeep.x);
            ImGui::ColorEdit3("Water Surface", &p.waterColorSurface.x);
            ImGui::ColorEdit3("Sand",          &p.sandColor.x);
            ImGui::ColorEdit3("Trees",         &p.treeColor.x);
            ImGui::ColorEdit3("Rock",          &p.rockColor.x);
            ImGui::ColorEdit3("Ice",           &p.iceColor.x);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextDisabled("Clouds");
            ImGui::SliderFloat("Density##c",   &p.cloudsDensity,  0.0f, 1.0f);
            ImGui::SliderFloat("Scale##c",     &p.cloudsScale,    0.1f, 4.0f);
            ImGui::SliderFloat("Speed##c",     &p.cloudsSpeed,    0.0f, 5.0f);
            ImGui::SliderFloat("Altitude##c",  &p.cloudAltitude,  0.02f,0.5f,"%.3f");
            ImGui::SliderFloat("Thickness##c", &p.cloudThickness, 0.02f,0.3f,"%.3f");
            ImGui::ColorEdit3("Cloud Color",   &p.cloudColor.x);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextDisabled("Atmosphere");
            ImGui::ColorEdit3("Atmo Color",    &p.atmosphereColor.x);
            ImGui::SliderFloat("Density##a",   &p.atmosphereDensity, 0.0f, 1.0f);
            ImGui::EndTabItem();
        }

        // LIGHT tab
        if (ImGui::BeginTabItem(" LIGHT  ")) {
            ImGui::Spacing();
            ImGui::SliderFloat("Sun X",      &p.sunDirection.x, -2.0f, 2.0f);
            ImGui::SliderFloat("Sun Y",      &p.sunDirection.y, -2.0f, 2.0f);
            ImGui::SliderFloat("Sun Z",      &p.sunDirection.z, -2.0f, 2.0f);
            ImGui::SliderFloat("Intensity",  &p.sunIntensity,    0.0f, 6.0f);
            ImGui::SliderFloat("Ambient",    &p.ambientLight,    0.0f, 0.2f);
            ImGui::ColorEdit3("Sun Color",   &p.sunColor.x);
            ImGui::ColorEdit3("Deep Space",  &p.deepSpaceColor.x);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            if (ImGui::Button("Reset to Defaults", ImVec2(-1, 0))) {
                p = PlanetParams{};
                m_presetIndex = 0;
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    if (outPos)  *outPos  = ImGui::GetWindowPos();
    if (outSize) *outSize = ImGui::GetWindowSize();
    ImGui::End();
}

void UIManager::setExoplanetCallback(std::function<void(const std::string&)> onLoad) {
    m_exoCallback = std::move(onLoad);
}

void UIManager::setExoplanetStatus(const std::string& status) {
    m_exoStatus = status;
}

void UIManager::setCurrentExoplanetData(const ExoplanetData& data) {
    m_currentExoData = data;
    m_hasExoData = true;
    if (m_dataViz) {
        m_dataViz->setCurrentPlanet(data);
    }
}

void UIManager::clearCurrentExoplanetData() {
    m_currentExoData = ExoplanetData{};
    m_hasExoData = false;
    if (m_dataViz) {
        m_dataViz->clearCurrentPlanet();
    }
}

PlanetParams UIManager::getPreset(int index) {
    return makePreset(index);
}

bool UIManager::wasBackPressed() {
    bool v = m_backPressed;
    m_backPressed = false;
    return v;
}

void UIManager::applyThemeTo(Theme t) {
    m_theme = t;
    setupStyle();
    if (m_dataViz) {
        m_dataViz->applyTheme(t == Theme::Dark);
    }
}

void UIManager::renderThemeToggle() {
    const float kW   = 54.f;
    const float kH   = 22.f;
    const float kPad =  9.f;

    ImGuiIO& io = ImGui::GetIO();

    // Smooth knob animation
    float target = (m_theme == Theme::Light) ? 1.f : 0.f;
    float speed  = io.DeltaTime * 10.f;
    if (m_toggleAnimT < target)
        m_toggleAnimT = std::min(m_toggleAnimT + speed, target);
    else
        m_toggleAnimT = std::max(m_toggleAnimT - speed, target);

    const float x = io.DisplaySize.x - kW - kPad;
    const float y = kPad;
    const bool  dk = (m_theme == Theme::Dark);

    ImDrawList* dl = ImGui::GetForegroundDrawList();

    ImU32 bg = dk ? IM_COL32(36, 37, 44, 230) : IM_COL32(120, 190, 255, 220);
    dl->AddRectFilled({ x, y }, { x + kW, y + kH }, bg, kH * 0.5f);

    ImU32 bd = dk ? IM_COL32(90, 95, 118, 180) : IM_COL32(100, 175, 255, 180);
    dl->AddRect({ x, y }, { x + kW, y + kH }, bd, kH * 0.5f, 0, 1.5f);

    const float r   = kH * 0.5f - 3.f;
    const float kxL = x + 3.f + r;
    const float kxR = x + kW - 3.f - r;
    const float kx  = kxL + (kxR - kxL) * m_toggleAnimT;
    const float ky  = y + kH * 0.5f;

    ImU32 glow = dk ? IM_COL32(80, 90, 130, 55) : IM_COL32(160, 220, 255, 65);
    dl->AddCircleFilled({ kx, ky }, r + 3.f, glow);

    ImU32 knob = dk ? IM_COL32(170, 175, 195, 248) : IM_COL32(230, 245, 255, 248);
    dl->AddCircleFilled({ kx, ky }, r, knob);

    dl->AddCircleFilled({ kx - r * 0.28f, ky - r * 0.30f },
        r * 0.25f, IM_COL32(255, 255, 255, 110));

    const char* lbl = dk ? "L" : "D";
    ImVec2 tsz = ImGui::CalcTextSize(lbl);
    float  lx  = dk ? x + kW - 3.f - r - tsz.x - 2.f : x + 3.f + r + 2.f;
    float  ly  = y + (kH - tsz.y) * 0.5f;
    ImU32  tc  = dk ? IM_COL32(190, 195, 215, 120) : IM_COL32(60, 120, 200, 120);
    dl->AddText({ lx, ly }, tc, lbl);

    // Hit-test window
    ImGui::SetNextWindowPos({ x - 2.f, y - 2.f }, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ kW + 4.f, kH + 4.f }, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 2.f, 2.f });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);

    constexpr ImGuiWindowFlags kHitFlags =
        ImGuiWindowFlags_NoDecoration    |
        ImGuiWindowFlags_NoMove          |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoNav           |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    if (ImGui::Begin("##theme_hit", nullptr, kHitFlags)) {
        ImGui::PopStyleVar(2);
        ImGui::InvisibleButton("##tog", { kW, kH });
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(dk ? "Switch to Light mode" : "Switch to Dark mode");
        if (ImGui::IsItemClicked()) {
            m_theme = dk ? Theme::Light : Theme::Dark;
            setupStyle();
        }
    } else {
        ImGui::PopStyleVar(2);
    }
    ImGui::End();
}

// Legacy planet editor (keep for compatibility)
void UIManager::renderPlanetEditor(const std::string& bodyName, PlanetParams& p,
                                    RingParams* ringParams,
                                    std::function<void()> onRingChanged) {
    ImGui::SetNextWindowSize(ImVec2(340, 720), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Properties")) {
        ImGui::End();
        return;
    }

    ImGui::Text("Editing: %s", bodyName.c_str());
    ImGui::Separator();

    // Presets
    static const char* presetNames[] = {
        "Earth", "Mars", "Lava World", "Ice World",
        "Gas Giant", "Ocean World", "Desert", "Alien"
    };
    static constexpr int presetCount = sizeof(presetNames) / sizeof(presetNames[0]);

    if (ImGui::CollapsingHeader("Presets", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SetNextItemWidth(-70);
        ImGui::Combo("##preset", &m_presetIndex, presetNames, presetCount);
        ImGui::SameLine();
        if (ImGui::Button("Apply")) {
            p = makePreset(m_presetIndex);
        }
    }

    if (ImGui::CollapsingHeader("Planet", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Radius", &p.radius, 0.5f, 5.0f);
        ImGui::SliderFloat("Rotation Offset", &p.rotationOffset, 0.0f, 6.28f);
        ImGui::SliderFloat("Quality", &p.quality, 0.0f, 2.0f);
        ImGui::SliderFloat("Rotation Speed", &p.rotationSpeed, 0.0f, 1.0f);
    }

    if (ImGui::CollapsingHeader("Terrain", ImGuiTreeNodeFlags_DefaultOpen)) {
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

    if (ImGui::CollapsingHeader("Latitude Effects")) {
        ImGui::SliderFloat("Polar Cap Size", &p.polarCapSize, 0.0f, 1.0f);
        ImGui::SliderFloat("Banding Strength", &p.bandingStrength, 0.0f, 1.0f);
        ImGui::SliderFloat("Banding Frequency", &p.bandingFrequency, 5.0f, 50.0f, "%.0f");
    }

    if (ImGui::CollapsingHeader("Surface Colors")) {
        ImGui::ColorEdit3("Water Deep", &p.waterColorDeep.x);
        ImGui::ColorEdit3("Water Surface", &p.waterColorSurface.x);
        ImGui::ColorEdit3("Sand", &p.sandColor.x);
        ImGui::ColorEdit3("Trees", &p.treeColor.x);
        ImGui::ColorEdit3("Rock", &p.rockColor.x);
        ImGui::ColorEdit3("Ice", &p.iceColor.x);
    }

    if (ImGui::CollapsingHeader("Biome Levels")) {
        ImGui::SliderFloat("Sand Level", &p.sandLevel, 0.0f, 0.2f);
        ImGui::SliderFloat("Tree Level", &p.treeLevel, 0.0f, 0.2f);
        ImGui::SliderFloat("Rock Level", &p.rockLevel, 0.0f, 0.3f);
        ImGui::SliderFloat("Ice Level", &p.iceLevel, 0.0f, 0.4f);
        ImGui::SliderFloat("Transition", &p.transition, 0.001f, 0.1f);
    }

    if (ImGui::CollapsingHeader("Clouds")) {
        ImGui::SliderFloat("Density##clouds", &p.cloudsDensity, 0.0f, 1.0f);
        ImGui::SliderFloat("Scale##clouds", &p.cloudsScale, 0.1f, 4.0f);
        ImGui::SliderFloat("Speed##clouds", &p.cloudsSpeed, 0.0f, 5.0f);
        ImGui::SliderFloat("Altitude##clouds", &p.cloudAltitude, 0.02f, 0.5f, "%.3f");
        ImGui::SliderFloat("Thickness##clouds", &p.cloudThickness, 0.02f, 0.3f, "%.3f");
        ImGui::ColorEdit3("Cloud Color", &p.cloudColor.x);
    }

    if (ImGui::CollapsingHeader("Atmosphere")) {
        ImGui::ColorEdit3("Atmo Color", &p.atmosphereColor.x);
        ImGui::SliderFloat("Density##atmo", &p.atmosphereDensity, 0.0f, 1.0f);
    }

    if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Sun X", &p.sunDirection.x, -2.0f, 2.0f);
        ImGui::SliderFloat("Sun Y", &p.sunDirection.y, -2.0f, 2.0f);
        ImGui::SliderFloat("Sun Z", &p.sunDirection.z, -2.0f, 2.0f);
        ImGui::SliderFloat("Intensity", &p.sunIntensity, 0.0f, 6.0f);
        ImGui::SliderFloat("Ambient", &p.ambientLight, 0.0f, 0.2f);
        ImGui::ColorEdit3("Sun Color", &p.sunColor.x);
        ImGui::ColorEdit3("Deep Space", &p.deepSpaceColor.x);
    }

    // Rings
    if (ringParams && ImGui::CollapsingHeader("Rings")) {
        bool changed = false;
        if (ImGui::Checkbox("Enable Rings", &ringParams->enabled)) changed = true;
        if (ringParams->enabled) {
            if (ImGui::SliderInt("Total Particles", &ringParams->totalParticles, 1000, 100000)) changed = true;
            if (ImGui::SliderFloat("Thickness", &ringParams->thickness, 0.001f, 0.1f, "%.3f")) changed = true;

            ImGui::Separator();
            ImGui::TextDisabled("Ring Bands");

            if (ImGui::Button("+ Add Band")) {
                float newInner = 1.5f;
                if (!ringParams->bands.empty()) {
                    newInner = ringParams->bands.back().outerRadius + 0.05f;
                }
                ringParams->addBand(newInner, newInner + 0.3f,
                                    {0.8f, 0.75f, 0.65f}, {0.6f, 0.55f, 0.5f}, 0.8f);
                changed = true;
            }

            int bandToRemove = -1;
            for (size_t i = 0; i < ringParams->bands.size(); i++) {
                auto& band = ringParams->bands[i];
                ImGui::PushID(static_cast<int>(i));

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

            if (bandToRemove >= 0 && bandToRemove < static_cast<int>(ringParams->bands.size())) {
                ringParams->bands.erase(ringParams->bands.begin() + bandToRemove);
                changed = true;
            }

            if (changed && onRingChanged) {
                onRingChanged();
            }
        }
    }

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

UIManager::ExoplanetSearchResult UIManager::renderExoplanetSearch(
    const std::vector<ExoplanetData>& results, bool isSearching) {

    ExoplanetSearchResult result;

    ImGui::SetNextWindowPos(ImVec2(360, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 500), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Exoplanet Search")) {
        ImGui::Text("Search NASA Exoplanet Archive");
        ImGui::SetNextItemWidth(-80);
        bool enterPressed = ImGui::InputText("##exosearch", m_exoSearchBuffer,
                                              sizeof(m_exoSearchBuffer),
                                              ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if ((ImGui::Button("Search") || enterPressed) && m_exoSearchBuffer[0] != '\0') {
            result.searchRequested = true;
            result.searchQuery = m_exoSearchBuffer;
        }

        if (isSearching) {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Searching...");
        }

        ImGui::Separator();

        if (!results.empty()) {
            ImGui::Text("Found %zu exoplanets:", results.size());

            ImGui::BeginChild("ExoResults", ImVec2(0, 250), true);
            for (size_t i = 0; i < results.size(); i++) {
                const auto& exo = results[i];
                bool isSelected = (m_exoSelectedIndex == static_cast<int>(i));

                int dataFields = 0;
                int totalFields = 5;
                if (exo.mass_earth.hasValue()) dataFields++;
                if (exo.radius_earth.hasValue()) dataFields++;
                if (exo.equilibrium_temp_k.hasValue()) dataFields++;
                if (exo.semi_major_axis_au.hasValue()) dataFields++;
                if (exo.orbital_period_days.hasValue()) dataFields++;

                ImVec4 color;
                if (dataFields >= 4) {
                    color = ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
                } else if (dataFields >= 2) {
                    color = ImVec4(1.0f, 1.0f, 0.3f, 1.0f);
                } else {
                    color = ImVec4(1.0f, 0.5f, 0.3f, 1.0f);
                }

                ImGui::PushStyleColor(ImGuiCol_Text, color);
                if (ImGui::Selectable(exo.name.c_str(), isSelected)) {
                    m_exoSelectedIndex = static_cast<int>(i);
                }
                ImGui::PopStyleColor();

                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Host: %s", exo.host_star.name.c_str());
                    ImGui::Text("Data: %d/%d fields", dataFields, totalFields);
                    ImGui::EndTooltip();
                }
            }
            ImGui::EndChild();

            if (m_exoSelectedIndex >= 0 && m_exoSelectedIndex < static_cast<int>(results.size())) {
                const auto& selected = results[static_cast<size_t>(m_exoSelectedIndex)];

                ImGui::Separator();
                ImGui::Text("Selected: %s", selected.name.c_str());

                auto showValue = [](const char* label, const auto& val, const char* unit) {
                    if (val.hasValue()) {
                        ImVec4 color = val.isAIInferred() ?
                            ImVec4(0.3f, 0.8f, 1.0f, 1.0f) :
                            ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                        ImGui::TextColored(color, "%s: %.3g %s", label, val.value, unit);
                    } else {
                        ImGui::TextDisabled("%s: unknown", label);
                    }
                };

                showValue("Mass", selected.mass_earth, "Earth");
                showValue("Radius", selected.radius_earth, "Earth");
                showValue("Temp", selected.equilibrium_temp_k, "K");
                showValue("Orbit", selected.semi_major_axis_au, "AU");
                showValue("Period", selected.orbital_period_days, "days");

                ImGui::Text("Host star: %s (%s)",
                           selected.host_star.name.c_str(),
                           selected.host_star.spectral_type.c_str());

                ImGui::Separator();
                if (ImGui::Button("View Planet", ImVec2(-1, 0))) {
                    result.viewRequested = true;
                    result.selectedIndex = m_exoSelectedIndex;
                }
            }
        } else if (!isSearching && m_exoSearchBuffer[0] != '\0') {
            ImGui::TextDisabled("No results. Try another search.");
        } else {
            ImGui::TextDisabled("Enter a planet name (e.g., 'Kepler-442')");
            ImGui::TextDisabled("or host star (e.g., 'TRAPPIST-1')");
        }

        // Cached planets section
        ImGui::Separator();
        if (!m_cachedPlanets.empty()) {
            ImGui::Text("Cached Planets (%zu):", m_cachedPlanets.size());

            ImGui::SetNextItemWidth(-60);
            ImGui::InputTextWithHint("##cachefilter", "Filter...", m_cachedFilterBuffer,
                                      sizeof(m_cachedFilterBuffer));
            ImGui::SameLine();
            if (ImGui::SmallButton("Refresh")) {
                refreshCachedPlanets();
            }

            ImGui::BeginChild("CachedPlanets", ImVec2(0, 180), true);

            std::string filterLower;
            if (m_cachedFilterBuffer[0] != '\0') {
                filterLower = m_cachedFilterBuffer;
                std::transform(filterLower.begin(), filterLower.end(),
                               filterLower.begin(), ::tolower);
            }

            for (size_t i = 0; i < m_cachedPlanets.size(); i++) {
                const auto& planet = m_cachedPlanets[i];

                if (!filterLower.empty()) {
                    std::string nameLower = planet.name;
                    std::transform(nameLower.begin(), nameLower.end(),
                                   nameLower.begin(), ::tolower);
                    if (nameLower.find(filterLower) == std::string::npos) {
                        continue;
                    }
                }

                bool isSelected = (m_cachedSelectedIndex == static_cast<int>(i));

                ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                if (planet.type.find("Gas") != std::string::npos ||
                    planet.type.find("Jupiter") != std::string::npos) {
                    color = ImVec4(1.0f, 0.8f, 0.4f, 1.0f);
                } else if (planet.type.find("Neptune") != std::string::npos) {
                    color = ImVec4(0.4f, 0.7f, 1.0f, 1.0f);
                } else if (planet.type.find("Super") != std::string::npos ||
                           planet.type.find("Earth") != std::string::npos ||
                           planet.type.find("Terrestrial") != std::string::npos) {
                    color = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
                } else if (planet.type.find("Lava") != std::string::npos) {
                    color = ImVec4(1.0f, 0.4f, 0.2f, 1.0f);
                }

                ImGui::PushStyleColor(ImGuiCol_Text, color);
                std::string label = planet.name + " [" + planet.type + "]";
                if (ImGui::Selectable(label.c_str(), isSelected)) {
                    m_cachedSelectedIndex = static_cast<int>(i);
                }
                ImGui::PopStyleColor();

                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                    result.cachedPlanetSelected = true;
                    result.cachedPlanetName = planet.name;
                }
            }
            ImGui::EndChild();

            if (m_cachedSelectedIndex >= 0 &&
                m_cachedSelectedIndex < static_cast<int>(m_cachedPlanets.size())) {
                const auto& selected = m_cachedPlanets[static_cast<size_t>(m_cachedSelectedIndex)];
                if (ImGui::Button(("Load " + selected.name).c_str(), ImVec2(-1, 0))) {
                    result.cachedPlanetSelected = true;
                    result.cachedPlanetName = selected.name;
                }
            }
        } else {
            ImGui::TextDisabled("No cached planets.");
            ImGui::TextDisabled("Run batch_generate.py to pre-cache.");
            if (ImGui::SmallButton("Refresh Cache")) {
                refreshCachedPlanets();
            }
        }

        ImGui::Separator();
        ImGui::Text("Quick Search:");
        if (ImGui::SmallButton("TRAPPIST-1")) {
            strcpy(m_exoSearchBuffer, "TRAPPIST-1");
            result.searchRequested = true;
            result.searchQuery = "TRAPPIST-1";
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Kepler-442")) {
            strcpy(m_exoSearchBuffer, "Kepler-442");
            result.searchRequested = true;
            result.searchQuery = "Kepler-442";
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Proxima")) {
            strcpy(m_exoSearchBuffer, "Proxima");
            result.searchRequested = true;
            result.searchQuery = "Proxima";
        }
    }
    ImGui::End();

    return result;
}

bool UIManager::renderSystemPresets(PresetManager& presetManager, int& selectedPresetIndex) {
    bool presetSelected = false;

    ImGui::SetNextWindowPos(ImVec2(10, 620), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(280, 150), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("System Presets")) {
        const auto& presets = presetManager.getAvailablePresets();

        if (presets.empty()) {
            ImGui::TextWrapped("No presets found. Click 'Generate' to create default presets.");
            if (ImGui::Button("Generate Presets")) {
                presetManager.generateBuiltInPresets();
            }
        } else {
            std::vector<const char*> systemPresetNames;
            for (const auto& preset : presets) {
                systemPresetNames.push_back(preset.name.c_str());
            }

            ImGui::SetNextItemWidth(-80);
            if (ImGui::Combo("##systemPreset", &m_systemPresetIndex,
                             systemPresetNames.data(), static_cast<int>(systemPresetNames.size()))) {
            }

            ImGui::SameLine();
            if (ImGui::Button("Load")) {
                selectedPresetIndex = m_systemPresetIndex;
                presetSelected = true;
            }

            if (m_systemPresetIndex >= 0 && m_systemPresetIndex < static_cast<int>(presets.size())) {
                const auto& info = presets[static_cast<size_t>(m_systemPresetIndex)];
                ImGui::TextWrapped("%s", info.description.c_str());
                ImGui::Text("Bodies: %d", info.bodyCount);
            }

            ImGui::Separator();
            if (ImGui::Button("Rescan Presets")) {
                presetManager.scanPresets();
            }
        }
    }
    ImGui::End();

    return presetSelected;
}

bool UIManager::renderInferenceSettings(InferenceEngine* engine) {
    if (!engine) return false;

    bool backendChanged = false;

    ImGui::SetNextWindowPos(ImVec2(770, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(280, 150), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("AI Inference")) {
        auto availableBackends = engine->getAvailableBackends();
        InferenceBackend currentBackend = engine->getBackend();

        if (availableBackends.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "No AI backends available");
            ImGui::TextWrapped("Configure AWS credentials or check network connectivity.");
        } else {
            ImGui::Text("Backend:");

            std::vector<std::string> backendNames;
            std::vector<const char*> backendNamePtrs;
            int currentIndex = 0;

            for (size_t i = 0; i < availableBackends.size(); i++) {
                backendNames.push_back(InferenceEngine::backendToString(availableBackends[i]));
                if (availableBackends[i] == currentBackend) {
                    currentIndex = static_cast<int>(i);
                }
            }
            for (const auto& name : backendNames) {
                backendNamePtrs.push_back(name.c_str());
            }

            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##inferenceBackend", &currentIndex,
                             backendNamePtrs.data(), static_cast<int>(backendNamePtrs.size()))) {
                engine->setBackend(availableBackends[static_cast<size_t>(currentIndex)]);
                backendChanged = true;
            }

            ImGui::Separator();
            switch (currentBackend) {
                case InferenceBackend::AWS_BEDROCK:
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Claude Sonnet via AWS Bedrock");
                    ImGui::TextWrapped("Highest quality, slower (6-12s)");
                    break;
                case InferenceBackend::AWS_BEDROCK_HAIKU:
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.8f, 1.0f), "Claude Haiku via AWS Bedrock");
                    ImGui::TextWrapped("Fast (~1-2s), good quality");
                    break;
                case InferenceBackend::GROQ_KIMI_K2:
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 1.0f, 1.0f), "Kimi K2 via Groq");
                    ImGui::TextWrapped("Very fast (~100ms), excellent quality");
                    break;
                case InferenceBackend::JIMMY_QWEN_72B:
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "Qwen 72B via chatjimmy.ai");
                    ImGui::TextWrapped("Fast (~20ms), excellent quality");
                    break;
                case InferenceBackend::JIMMY_LLAMA_70B:
                    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Llama 70B via chatjimmy.ai");
                    ImGui::TextWrapped("Fast (~20ms), good quality");
                    break;
                case InferenceBackend::JIMMY_LLAMA_8B:
                    ImGui::TextColored(ImVec4(0.8f, 0.6f, 1.0f, 1.0f), "Llama 8B via chatjimmy.ai");
                    ImGui::TextWrapped("Fastest (~10ms), basic quality");
                    break;
                case InferenceBackend::NONE:
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Disabled");
                    break;
            }

            ImGui::Separator();
            if (engine->isAvailable()) {
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Ready");
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Unavailable");
            }
        }
    }
    ImGui::End();

    return backendChanged;
}

UIManager::SimulationControlsResult UIManager::renderSimulationControls(
    bool isPaused, double timeScale, bool /*isLocked*/, const std::string& /*lockedBodyName*/) {

    SimulationControlsResult result;

    ImGuiIO& io = ImGui::GetIO();

    // Position at bottom center
    const float panelWidth = 320.f;
    const float panelHeight = 44.f;
    const float padding = 20.f;

    ImGui::SetNextWindowPos(
        ImVec2((io.DisplaySize.x - panelWidth) * 0.5f, io.DisplaySize.y - panelHeight - padding),
        ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.85f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoNav;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.f, 10.f));

    if (ImGui::Begin("##sim_controls", nullptr, flags)) {
        // Play/Pause button
        if (isPaused) {
            if (ImGui::Button("Play", ImVec2(50, 0))) {
                result.pauseToggled = true;
            }
        } else {
            if (ImGui::Button("Pause", ImVec2(50, 0))) {
                result.pauseToggled = true;
            }
        }

        ImGui::SameLine();

        // Time scale presets - simpler for rotation
        static const char* scaleNames[] = { "0.1x", "0.5x", "1x", "2x", "5x", "10x" };
        static const double scaleValues[] = { 0.1, 0.5, 1.0, 2.0, 5.0, 10.0 };

        // Find current preset
        int currentPreset = -1;
        for (int i = 0; i < 6; ++i) {
            if (std::abs(timeScale - scaleValues[i]) < 0.01) {
                currentPreset = i;
                break;
            }
        }

        ImGui::Text("Speed:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        if (ImGui::BeginCombo("##timescale", currentPreset >= 0 ? scaleNames[currentPreset] : "Custom")) {
            for (int i = 0; i < 6; ++i) {
                if (ImGui::Selectable(scaleNames[i], currentPreset == i)) {
                    result.timeScaleChanged = true;
                    result.newTimeScale = scaleValues[i];
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();

        // Reset rotation button
        if (ImGui::Button("Reset")) {
            result.timeScaleChanged = true;
            result.newTimeScale = 1.0;
        }
    }
    ImGui::PopStyleVar(2);
    ImGui::End();

    return result;
}

}  // namespace astrocore
