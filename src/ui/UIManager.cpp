#include "ui/UIManager.hpp"
#include "render/IRenderer.hpp"
#include "render/VulkanRenderer.hpp"
#include "data/ExoplanetData.hpp"
#include "core/Logger.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstring>

namespace astrocore {

// ── Presets ──────────────────────────────────────────────────────────────────

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

    // ── Shape (shared) ───────────────────────────────────────────────────────
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
        // ── Space grey dark — semi-transparent so the starfield shows through ─────
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
        // ── Subtle atmosphere glass — mirrors the galaxy sidebar look:
        //    very low alpha so the starfield bleeds through uniformly.
        //    Title bar alpha almost matches WindowBg so there's no heavy
        //    coloured band at the top of the planet editor.
        c[ImGuiCol_WindowBg]             = ImVec4(0.52f, 0.78f, 1.00f, 0.12f);
        c[ImGuiCol_ChildBg]              = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        c[ImGuiCol_PopupBg]              = ImVec4(0.18f, 0.25f, 0.38f, 0.94f);

        c[ImGuiCol_Border]               = ImVec4(0.60f, 0.82f, 1.00f, 0.28f);
        c[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

        // Title bar blends into window — same alpha as WindowBg so it doesn't pop
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

    // ── Back to Catalogue button ────────────────────────────────────────
    {
        ImGui::PushStyleColor(ImGuiCol_Button,
            ImVec4(0.08f, 0.20f, 0.40f, 0.70f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            ImVec4(0.14f, 0.32f, 0.60f, 0.85f));
        if (ImGui::SmallButton("  << Catalogue  "))
            m_backPressed = true;
        ImGui::PopStyleColor(2);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Pokédex-style liquid glass tab bar ───────────────────────────────
    if (ImGui::BeginTabBar("##tabs")) {

        // ━━ DATA ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
        if (ImGui::BeginTabItem("  DATA  ")) {
            ImGui::Spacing();

            // Current planet / load status line
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.88f, 1.0f, 1.0f));
            ImGui::TextWrapped("%s", m_exoStatus.c_str());
            ImGui::PopStyleColor();
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Helper: two-column data row.
            // AI-predicted fields get a small blue badge.
            auto row = [](const char* label, const char* value, bool ai) {
                ImGui::TextDisabled("%s", label);
                ImGui::SameLine(110.f);
                ImGui::Text("%s", value);
                if (ai) {
                    ImGui::SameLine();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.75f, 1.0f, 0.90f));
                    ImGui::Text(" \xe2\x97\x86 AI");   // ◆ AI
                    ImGui::PopStyleColor();
                }
            };

            // ── Classification ──────────────────────────────────────────
            ImGui::TextDisabled("CLASSIFICATION");
            ImGui::Spacing();
            row("Type",         "number",     false);
            row("Sub-type",     "number",     true);
            row("Distance",     "number ly",  false);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // ── Physical ────────────────────────────────────────────────
            ImGui::TextDisabled("PHYSICAL");
            ImGui::Spacing();
            row("Mass",         "number M\xe2\x8a\x95",     true);   // M⊕
            row("Radius",       "number R\xe2\x8a\x95",     true);   // R⊕
            row("Gravity",      "number g",   true);
            row("Density",      "number g/cm\xc2\xb3", true); // g/cm³

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // ── Orbit ───────────────────────────────────────────────────
            ImGui::TextDisabled("ORBIT");
            ImGui::Spacing();
            row("Period",       "number days", true);
            row("Semi-major",   "number AU",   false);
            row("Eccentricity", "number",      true);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // ── Environment ─────────────────────────────────────────────
            ImGui::TextDisabled("ENVIRONMENT");
            ImGui::Spacing();
            row("Surf. Temp",   "number K",   true);
            row("Atmosphere",   "number",     true);
            row("Water",        "number %",   true);
            row("Habitability", "number",     true);

            ImGui::EndTabItem();
        }

        // ━━ WORLD ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
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

        // ━━ VISUAL ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
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

        // ━━ LIGHT ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
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
}

void UIManager::renderThemeToggle() {
    const float kW   = 54.f;
    const float kH   = 22.f;
    const float kPad =  9.f;

    ImGuiIO& io = ImGui::GetIO();

    // ── Smooth knob animation ────────────────────────────────────────────────
    // Animate toward the current theme target every frame using DeltaTime.
    float target = (m_theme == Theme::Light) ? 1.f : 0.f;
    float speed  = io.DeltaTime * 10.f;  // ~0.1 s to slide across
    if (m_toggleAnimT < target)
        m_toggleAnimT = std::min(m_toggleAnimT + speed, target);
    else
        m_toggleAnimT = std::max(m_toggleAnimT - speed, target);

    // ── Screen position ──────────────────────────────────────────────────────
    const float x = io.DisplaySize.x - kW - kPad;
    const float y = kPad;
    const bool  dk = (m_theme == Theme::Dark);

    // ── Draw on foreground draw list — always renders on top of every window ──
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    // Pill background colours match the two UI modes shown in-app:
    //   Dark  → space grey  (matches WindowBg #242428 family)
    //   Light → atmosphere blue (matches WindowBg #8CCCFF family)
    ImU32 bg = dk ? IM_COL32(36, 37, 44, 230) : IM_COL32(120, 190, 255, 220);
    dl->AddRectFilled({ x, y }, { x + kW, y + kH }, bg, kH * 0.5f);

    // Pill border
    ImU32 bd = dk ? IM_COL32(90, 95, 118, 180) : IM_COL32(100, 175, 255, 180);
    dl->AddRect({ x, y }, { x + kW, y + kH }, bd, kH * 0.5f, 0, 1.5f);

    // Knob: slides smoothly between left (dark) and right (light)
    const float r   = kH * 0.5f - 3.f;
    const float kxL = x + 3.f + r;            // fully-dark position
    const float kxR = x + kW - 3.f - r;       // fully-light position
    const float kx  = kxL + (kxR - kxL) * m_toggleAnimT;
    const float ky  = y + kH * 0.5f;

    // Glow behind knob (dark = cool blue-grey, light = sky glow)
    ImU32 glow = dk ? IM_COL32(80, 90, 130, 55) : IM_COL32(160, 220, 255, 65);
    dl->AddCircleFilled({ kx, ky }, r + 3.f, glow);

    // Knob fill: dark = cool grey-white  /  light = bright sky white
    ImU32 knob = dk ? IM_COL32(170, 175, 195, 248) : IM_COL32(230, 245, 255, 248);
    dl->AddCircleFilled({ kx, ky }, r, knob);

    // Specular highlight
    dl->AddCircleFilled({ kx - r * 0.28f, ky - r * 0.30f },
        r * 0.25f, IM_COL32(255, 255, 255, 110));

    // Inactive-side micro-label
    const char* lbl = dk ? "L" : "D";
    ImVec2 tsz = ImGui::CalcTextSize(lbl);
    float  lx  = dk ? x + kW - 3.f - r - tsz.x - 2.f : x + 3.f + r + 2.f;
    float  ly  = y + (kH - tsz.y) * 0.5f;
    ImU32  tc  = dk ? IM_COL32(190, 195, 215, 120) : IM_COL32(60, 120, 200, 120);
    dl->AddText({ lx, ly }, tc, lbl);

    // ── Hit-test window — always the last window created → always front-most ──
    // Zero background + no decoration; invisible to the user but handles input.
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

void UIManager::setCachedNames(const std::vector<std::string>& names) {
    m_cachedNames = names;
}

void UIManager::setLoading(bool loading) {
    m_isLoading = loading;
}

void UIManager::setExoplanetData(const ExoplanetData* data) {
    m_exoData = data;
}

}  // namespace astrocore
