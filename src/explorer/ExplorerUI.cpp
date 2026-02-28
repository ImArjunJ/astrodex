#include "explorer/ExplorerUI.hpp"
#include "explorer/StarRenderer.hpp"
#include "explorer/FreeFlyCamera.hpp"
#include "explorer/StarData.hpp"
#include "core/Logger.hpp"

#include <vulkan/vulkan.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <GLFW/glfw3.h>

namespace astrocore {

ExplorerUI::ExplorerUI() = default;

ExplorerUI::~ExplorerUI() {
    if (m_initialized) shutdown();
}

void ExplorerUI::init(GLFWwindow* window, StarRenderer* renderer) {
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
    LOG_INFO("ImGui initialized (Star Explorer)");
}

void ExplorerUI::shutdown() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    m_initialized = false;
}

void ExplorerUI::beginFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ExplorerUI::endFrame(StarRenderer* renderer) {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(
        ImGui::GetDrawData(),
        static_cast<VkCommandBuffer>(renderer->getCurrentCommandBuffer()));
}

void ExplorerUI::setupStyle() {
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
    colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.10f, 0.90f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.14f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.20f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.15f, 0.15f, 0.25f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.25f, 0.35f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.30f, 0.30f, 0.40f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.15f, 0.30f, 0.55f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.40f, 0.65f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.35f, 0.60f, 1.00f);
}

bool ExplorerUI::render(const FreeFlyCamera& camera, const StarInfo& nearest,
                        float& pointScale, float& brightnessBoost, float& speed,
                        int totalStars, bool useOctree, int visibleStars,
                        float& lodThreshold, int& maxVisibleStars) {
    bool resetPressed = false;

    ImGui::SetNextWindowSize(ImVec2(300, 480), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Star Explorer")) {
        ImGui::End();
        return false;
    }

    // Camera info
    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        glm::vec3 pos = camera.getPosition();
        ImGui::Text("Position: (%.2f, %.2f, %.2f) pc", pos.x, pos.y, pos.z);
        ImGui::Text("Yaw: %.1f  Pitch: %.1f", camera.getYaw(), camera.getPitch());
        ImGui::SliderFloat("Speed (pc/s)", &speed, 0.001f, 10000.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::Text("FOV: %.0f", camera.getFov());
    }

    // Nearest star
    if (ImGui::CollapsingHeader("Nearest Star", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (!nearest.name.empty()) {
            ImGui::Text("Name: %s", nearest.name.c_str());
        } else {
            ImGui::TextDisabled("(unnamed)");
        }
        if (!nearest.spectralType.empty()) {
            ImGui::Text("Spectral: %s", nearest.spectralType.c_str());
        }
        if (!nearest.constellation.empty()) {
            ImGui::Text("Constellation: %s", nearest.constellation.c_str());
        }
        ImGui::Text("Magnitude: %.2f", nearest.magnitude);
        ImGui::Text("Distance: %.4f pc", nearest.distance);
        ImGui::Text("Position: (%.2f, %.2f, %.2f)",
                    nearest.position.x, nearest.position.y, nearest.position.z);
    }

    // Render settings
    if (ImGui::CollapsingHeader("Render Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Point Scale", &pointScale, 0.1f, 20.0f, "%.1f");
        ImGui::SliderFloat("Brightness", &brightnessBoost, 0.1f, 100.0f, "%.1f", ImGuiSliderFlags_Logarithmic);

        if (useOctree) {
            ImGui::Separator();
            ImGui::Text("Source: Gaia DR3 (octree)");
            ImGui::Text("Total: %d", totalStars);
            ImGui::Text("Visible: %d", visibleStars);
            ImGui::SliderFloat("LOD Threshold", &lodThreshold, 0.001f, 0.5f, "%.3f", ImGuiSliderFlags_Logarithmic);
            int maxStars = maxVisibleStars;
            if (ImGui::SliderInt("Max Points", &maxStars, 100000, 2000000, "%d", ImGuiSliderFlags_Logarithmic)) {
                maxVisibleStars = maxStars;
            }
        } else {
            ImGui::Text("Source: HYG (%d stars)", totalStars);
        }
    }

    // Controls help
    if (ImGui::CollapsingHeader("Controls")) {
        ImGui::TextDisabled("WASD - Move");
        ImGui::TextDisabled("Arrow Keys - Look");
        ImGui::TextDisabled("Space/Shift - Up/Down");
        ImGui::TextDisabled("Mouse - Look (when captured)");
        ImGui::TextDisabled("Scroll - Adjust speed");
        ImGui::TextDisabled("Tab - Toggle cursor capture");
        ImGui::TextDisabled("Esc - Quit");
    }

    ImGui::Separator();
    if (ImGui::Button("Reset to Sol")) {
        resetPressed = true;
    }

    ImGui::End();
    return resetPressed;
}

} // namespace astrocore
