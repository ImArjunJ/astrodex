#include "ui/UIManager.hpp"
#include "render/IRenderer.hpp"
#include "render/VulkanRenderer.hpp"
#include "core/Logger.hpp"

#include <vulkan/vulkan.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstring>

namespace astrocore {

// ── Presets ──────────────────────────────────────────────────────────────────

static const char* presetNames[] = {
    "Earth", "Mars", "Lava World", "Ice World",
    "Gas Giant", "Ocean World", "Desert", "Alien", "Black Hole"
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
    case 8: // Black Hole
        p.isBlackHole           = true;
        p.bhMass                = 1.0f;
        p.bhAccretionInner      = 3.0f;
        p.bhAccretionOuter      = 10.0f;
        p.bhDiskSpeed           = 1.0f;
        p.bhDiskTurbulence      = 0.3f;
        p.bhDiskBrightness      = 2.0f;
        p.bhDiskTemperatureInner = 10000.0f;
        p.bhDiskTemperatureOuter = 3000.0f;
        p.bhDiskTint            = {1.0f, 0.95f, 0.9f};
        p.bhRaySteps            = 128;
        p.bhDopplerStrength     = 1.0f;
        p.radius                = 2.0f;
        p.sunIntensity          = 3.0f;
        p.deepSpaceColor        = {0.0f, 0.0f, 0.002f};
        p.atmosphereColor       = {0.05f, 0.3f, 0.9f};
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

void UIManager::init(GLFWwindow* window, VulkanRenderer* renderer) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui_ImplGlfw_InitForVulkan(window, true);

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance       = static_cast<VkInstance>(renderer->getInstance());
    initInfo.PhysicalDevice = static_cast<VkPhysicalDevice>(renderer->getPhysicalDevice());
    initInfo.Device         = static_cast<VkDevice>(renderer->getDevice());
    initInfo.QueueFamily    = renderer->getGraphicsQueueFamily();
    initInfo.Queue          = static_cast<VkQueue>(renderer->getGraphicsQueue());
    initInfo.DescriptorPool = static_cast<VkDescriptorPool>(renderer->getDescriptorPool());
    initInfo.RenderPass     = static_cast<VkRenderPass>(renderer->getRenderPass());
    initInfo.MinImageCount  = 2;
    initInfo.ImageCount     = renderer->getSwapchainImageCount();
    initInfo.MSAASamples    = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&initInfo);
    ImGui_ImplVulkan_CreateFontsTexture();

    setupStyle();
    m_initialized = true;
    LOG_INFO("ImGui initialized (Vulkan backend)");
}

void UIManager::shutdown() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    m_initialized = false;
}

void UIManager::beginFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void UIManager::endFrame(VulkanRenderer* renderer) {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(
        ImGui::GetDrawData(),
        static_cast<VkCommandBuffer>(renderer->getCurrentCommandBuffer()));
}

void UIManager::setupStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark();

    // ── Shape ────────────────────────────────────────────────────────────────
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

    // ── Glass window background — very low alpha so stars bleed through ──────
    c[ImGuiCol_WindowBg]          = ImVec4(0.04f, 0.07f, 0.12f, 0.18f);
    c[ImGuiCol_ChildBg]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_PopupBg]           = ImVec4(0.05f, 0.08f, 0.14f, 0.88f);

    // ── Glassy frost border ──────────────────────────────────────────────────
    c[ImGuiCol_Border]            = ImVec4(0.55f, 0.80f, 1.00f, 0.32f);
    c[ImGuiCol_BorderShadow]      = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    // ── Title bar — slightly more opaque for legibility ──────────────────────
    c[ImGuiCol_TitleBg]           = ImVec4(0.04f, 0.10f, 0.18f, 0.72f);
    c[ImGuiCol_TitleBgActive]     = ImVec4(0.06f, 0.15f, 0.26f, 0.80f);
    c[ImGuiCol_TitleBgCollapsed]  = ImVec4(0.02f, 0.05f, 0.10f, 0.55f);

    // ── Frame / input backgrounds ────────────────────────────────────────────
    c[ImGuiCol_FrameBg]           = ImVec4(0.10f, 0.18f, 0.28f, 0.42f);
    c[ImGuiCol_FrameBgHovered]    = ImVec4(0.16f, 0.26f, 0.40f, 0.55f);
    c[ImGuiCol_FrameBgActive]     = ImVec4(0.20f, 0.32f, 0.48f, 0.65f);

    // ── Collapsing headers ───────────────────────────────────────────────────
    c[ImGuiCol_Header]            = ImVec4(0.20f, 0.38f, 0.60f, 0.32f);
    c[ImGuiCol_HeaderHovered]     = ImVec4(0.28f, 0.50f, 0.76f, 0.42f);
    c[ImGuiCol_HeaderActive]      = ImVec4(0.32f, 0.56f, 0.82f, 0.52f);

    // ── Slider ───────────────────────────────────────────────────────────────
    c[ImGuiCol_SliderGrab]        = ImVec4(0.35f, 0.78f, 1.00f, 0.85f);
    c[ImGuiCol_SliderGrabActive]  = ImVec4(0.50f, 0.90f, 1.00f, 1.00f);

    // ── Scrollbar ────────────────────────────────────────────────────────────
    c[ImGuiCol_ScrollbarBg]       = ImVec4(0.00f, 0.00f, 0.00f, 0.10f);
    c[ImGuiCol_ScrollbarGrab]     = ImVec4(0.30f, 0.60f, 0.90f, 0.40f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.72f, 1.00f, 0.55f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.50f, 0.82f, 1.00f, 0.70f);

    // ── Buttons ──────────────────────────────────────────────────────────────
    c[ImGuiCol_Button]            = ImVec4(0.16f, 0.36f, 0.58f, 0.68f);
    c[ImGuiCol_ButtonHovered]     = ImVec4(0.24f, 0.50f, 0.76f, 0.78f);
    c[ImGuiCol_ButtonActive]      = ImVec4(0.20f, 0.44f, 0.70f, 0.90f);

    // ── Text — bright white for legibility against transparent background ─────
    c[ImGuiCol_Text]              = ImVec4(0.96f, 0.97f, 1.00f, 1.00f);
    c[ImGuiCol_TextDisabled]      = ImVec4(0.55f, 0.68f, 0.82f, 0.80f);

    // ── Check / combo ────────────────────────────────────────────────────────
    c[ImGuiCol_CheckMark]         = ImVec4(0.45f, 0.85f, 1.00f, 1.00f);
    c[ImGuiCol_Separator]         = ImVec4(0.45f, 0.70f, 1.00f, 0.25f);
    c[ImGuiCol_SeparatorHovered]  = ImVec4(0.55f, 0.80f, 1.00f, 0.45f);
    c[ImGuiCol_SeparatorActive]   = ImVec4(0.60f, 0.88f, 1.00f, 0.60f);
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

    // ── Exoplanet Search ─────────────────────────────────────────────────
    // Track autocomplete state across frames (need to render popup after Planet Editor window ends)
    bool showAutocomplete = false;
    ImVec2 acInputPos, acInputSize;

    if (ImGui::CollapsingHeader("Exoplanet Lookup", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextDisabled("Search NASA Exoplanet Archive");

        // Disable input and button during loading
        if (m_isLoading) ImGui::BeginDisabled();

        ImGui::SetNextItemWidth(-80);
        bool hitEnter = ImGui::InputText("##planet", m_searchBuf, sizeof(m_searchBuf),
                                         ImGuiInputTextFlags_EnterReturnsTrue);

        // Capture input rect for autocomplete positioning
        bool inputActive = ImGui::IsItemActive();
        acInputPos  = ImGui::GetItemRectMin();
        acInputSize = ImGui::GetItemRectSize();

        ImGui::SameLine();
        bool clicked = ImGui::Button("Load");

        if (m_isLoading) ImGui::EndDisabled();

        if ((hitEnter || clicked) && m_exoCallback && m_searchBuf[0] != '\0') {
            m_exoStatus = "Loading...";
            m_exoCallback(std::string(m_searchBuf));
        }

        // Determine if autocomplete should show
        showAutocomplete = inputActive && m_searchBuf[0] != '\0' && !m_isLoading;

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

    // ── Black Hole ──────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Black Hole")) {
        ImGui::Checkbox("Enable Black Hole", &p.isBlackHole);
        if (p.isBlackHole) {
            ImGui::SliderFloat("Mass", &p.bhMass, 0.1f, 5.0f);
            ImGui::SliderInt("Ray Steps", &p.bhRaySteps, 32, 256);
            ImGui::Separator();
            ImGui::TextDisabled("Accretion Disk");
            ImGui::SliderFloat("Inner Edge (Rs)", &p.bhAccretionInner, 1.5f, 6.0f, "%.1f");
            ImGui::SliderFloat("Outer Edge (Rs)", &p.bhAccretionOuter, 4.0f, 30.0f, "%.1f");
            ImGui::SliderFloat("Disk Speed", &p.bhDiskSpeed, 0.0f, 3.0f, "%.2f");
            ImGui::SliderFloat("Turbulence", &p.bhDiskTurbulence, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Brightness", &p.bhDiskBrightness, 0.1f, 10.0f, "%.1f");
            ImGui::Separator();
            ImGui::TextDisabled("Temperature");
            ImGui::SliderFloat("Inner Temp (K)", &p.bhDiskTemperatureInner, 3000.0f, 30000.0f, "%.0f");
            ImGui::SliderFloat("Outer Temp (K)", &p.bhDiskTemperatureOuter, 1000.0f, 10000.0f, "%.0f");
            ImGui::ColorEdit3("Disk Tint", &p.bhDiskTint.x);
            ImGui::Separator();
            ImGui::SliderFloat("Doppler Strength", &p.bhDopplerStrength, 0.0f, 2.0f, "%.2f");
        }
    }

    if (!p.isBlackHole) {
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
    } // !isBlackHole

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

    if (outPos)  *outPos  = ImGui::GetWindowPos();
    if (outSize) *outSize = ImGui::GetWindowSize();
    ImGui::End();

    // ── Autocomplete Popup (rendered after Planet Editor for z-order) ────
    if (showAutocomplete && !m_cachedNames.empty()) {
        // Case-insensitive prefix matcher
        auto matchesPrefix = [](const std::string& name, const char* prefix) -> bool {
            std::string lowerName = name;
            std::string lowerPrefix = prefix;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
            std::transform(lowerPrefix.begin(), lowerPrefix.end(), lowerPrefix.begin(), ::tolower);
            return lowerName.find(lowerPrefix) == 0;
        };

        // Count matches first to avoid showing an empty window
        int matchCount = 0;
        for (const auto& name : m_cachedNames) {
            if (matchesPrefix(name, m_searchBuf)) matchCount++;
        }

        if (matchCount > 0) {
            ImGui::SetNextWindowPos(ImVec2(acInputPos.x, acInputPos.y + acInputSize.y));
            ImGui::SetNextWindowSize(ImVec2(acInputSize.x, 0));  // auto-height
            ImGui::SetNextWindowFocus();

            if (ImGui::Begin("##autocomplete", nullptr,
                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing)) {
                int shown = 0;
                for (const auto& name : m_cachedNames) {
                    if (!matchesPrefix(name, m_searchBuf)) continue;
                    if (shown >= 8) break;

                    if (ImGui::Selectable(name.c_str())) {
                        std::strncpy(m_searchBuf, name.c_str(), sizeof(m_searchBuf) - 1);
                        m_searchBuf[sizeof(m_searchBuf) - 1] = '\0';
                        // Trigger load on selection
                        if (m_exoCallback) {
                            m_exoStatus = "Loading...";
                            m_exoCallback(name);
                        }
                    }
                    shown++;
                }
            }
            ImGui::End();
        }
    }
}

void UIManager::setExoplanetCallback(std::function<void(const std::string&)> onLoad) {
    m_exoCallback = std::move(onLoad);
}

void UIManager::setExoplanetStatus(const std::string& status) {
    m_exoStatus = status;
}

void UIManager::setCachedNames(const std::vector<std::string>& names) {
    m_cachedNames = names;
}

void UIManager::setLoading(bool loading) {
    m_isLoading = loading;
}

}  // namespace astrocore
