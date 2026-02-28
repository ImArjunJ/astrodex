#include "ui/UIManager.hpp"
#include "render/IRenderer.hpp"
#include "core/Logger.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <GLFW/glfw3.h>

#ifdef ASTRO_METAL
#  import <Metal/Metal.h>
#  include <imgui_impl_metal.h>
#else
#  include <imgui_impl_opengl3.h>
#endif

namespace astrocore {

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

#ifdef ASTRO_METAL

void UIManager::init(GLFWwindow* window, void* metalDevice) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui_ImplGlfw_InitForOther(window, true);
    ImGui_ImplMetal_Init((__bridge id<MTLDevice>)metalDevice);

    setupStyle();
    m_initialized = true;
    LOG_INFO("ImGui initialized (Metal backend)");
}

void UIManager::shutdown() {
    ImGui_ImplMetal_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    m_initialized = false;
}

void UIManager::beginFrame(void* renderPassDescriptor) {
    ImGui_ImplMetal_NewFrame((__bridge MTLRenderPassDescriptor*)renderPassDescriptor);
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void UIManager::endFrame(void* commandBuffer, void* commandEncoder) {
    ImGui::Render();
    ImGui_ImplMetal_RenderDrawData(
        ImGui::GetDrawData(),
        (__bridge id<MTLCommandBuffer>)commandBuffer,
        (__bridge id<MTLRenderCommandEncoder>)commandEncoder);
}

#else  // OpenGL path

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
    LOG_INFO("ImGui initialized (OpenGL backend)");
}

void UIManager::shutdown() {
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

#endif  // ASTRO_METAL

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

void UIManager::render(PlanetParams& p) {
    ImGui::SetNextWindowSize(ImVec2(340, 720), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Planet Editor")) {
        ImGui::End();
        return;
    }

    // ── Exoplanet Search ─────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Exoplanet Lookup", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextDisabled("Search NASA Exoplanet Archive");
        ImGui::SetNextItemWidth(-80);
        bool hitEnter = ImGui::InputText("##planet", m_searchBuf, sizeof(m_searchBuf),
                                         ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        bool clicked = ImGui::Button("Load");
        if ((hitEnter || clicked) && m_exoCallback && m_searchBuf[0] != '\0') {
            m_exoStatus = "Loading...";
            m_exoCallback(std::string(m_searchBuf));
        }
        ImGui::TextDisabled("%s", m_exoStatus.c_str());
        ImGui::Spacing();
    }

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

    // ── Reset ───────────────────────────────────────────────────────────
    ImGui::Separator();
    if (ImGui::Button("Reset to Defaults")) {
        p = PlanetParams{};
        m_presetIndex = 0;
    }

    ImGui::End();
}

void UIManager::setExoplanetCallback(std::function<void(const std::string&)> onLoad) {
    m_exoCallback = std::move(onLoad);
}

void UIManager::setExoplanetStatus(const std::string& status) {
    m_exoStatus = status;
}

}  // namespace astrocore
