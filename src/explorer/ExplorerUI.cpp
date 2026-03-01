#include "explorer/ExplorerUI.hpp"
#include "explorer/FreeFlyCamera.hpp"
#include "explorer/StarData.hpp"
#include "core/Logger.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

namespace astrocore {

ExplorerUI::ExplorerUI() = default;

ExplorerUI::~ExplorerUI() {
    if (m_initialized) shutdown();
}

void ExplorerUI::init(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 410");

    setupStyle();
    m_initialized = true;
    LOG_INFO("ImGui initialized (Star Explorer, OpenGL)");
}

void ExplorerUI::shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    m_initialized = false;
}

void ExplorerUI::beginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ExplorerUI::endFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
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
                        int totalStars, bool useLOD, int visibleStars,
                        float& shellMultiplier, bool& showMarkers, bool& debugRender) {
    bool resetPressed = false;

    ImGui::SetNextWindowSize(ImVec2(320, 550), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Star Explorer")) {
        ImGui::End();
        return false;
    }

    // Camera
    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        glm::vec3 pos = camera.getPosition();
        ImGui::Text("Position: (%.2f, %.2f, %.2f) pc", pos.x, pos.y, pos.z);
        ImGui::Text("Yaw: %.1f  Pitch: %.1f", camera.getYaw(), camera.getPitch());
        ImGui::SliderFloat("Speed (pc/s)", &speed, 0.001f, 10000.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
    }

    // Nearest star
    if (ImGui::CollapsingHeader("Nearest Star", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (!nearest.name.empty()) {
            ImGui::Text("Name: %s", nearest.name.c_str());
        } else {
            ImGui::TextDisabled("(unnamed)");
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

        if (useLOD) {
            ImGui::Separator();
            ImGui::Text("Source: Gaia DR3 (multi-LOD)");
            ImGui::Text("Total: %d", totalStars);
            ImGui::Text("Visible: %d", visibleStars);
            ImGui::SliderFloat("Detail Range", &shellMultiplier, 0.1f, 10.0f, "%.1fx", ImGuiSliderFlags_Logarithmic);
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Multiplier for shell distances.\n"
                                  "Higher = more detail at greater distance.\n"
                                  "Lower = detail only very close.");
            }
        } else {
            ImGui::Text("Source: HYG (%d stars)", totalStars);
        }
    }

    // Debug
    if (ImGui::CollapsingHeader("Debug")) {
        ImGui::Checkbox("Show Sol Marker", &showMarkers);
        ImGui::Checkbox("Debug Flat White", &debugRender);
    }

    // Controls
    if (ImGui::CollapsingHeader("Controls")) {
        ImGui::TextDisabled("WASD - Move");
        ImGui::TextDisabled("Arrow Keys - Look");
        ImGui::TextDisabled("Space/Shift - Up/Down");
        ImGui::TextDisabled("Mouse - Look (when captured)");
        ImGui::TextDisabled("Scroll - Adjust speed");
        ImGui::TextDisabled("Tab - Toggle cursor capture");
    }

    ImGui::Separator();
    if (ImGui::Button("Reset to Sol")) {
        resetPressed = true;
    }

    ImGui::End();
    return resetPressed;
}

} // namespace astrocore
