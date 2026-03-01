#include "core/Application.hpp"
#include "core/Logger.hpp"
#include "render/Camera.hpp"
#include "intro/IntroAnimation.hpp"
#include "render/VulkanRenderer.hpp"
#include "data/SolarSystemDatabase.hpp"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <chrono>
#include <spdlog/fmt/fmt.h>
#include <set>
#include <algorithm>

namespace astrocore {

// Pipeline stage display messages (indexed by PipelineStage enum)
static constexpr const char* kStageMessages[] = {
    "",                          // Idle
    "Querying NASA...",          // QueryingNasa
    "Running AI inference...",   // RunningAI
    "Mapping parameters...",     // MappingParams
    "Done",                      // Done
    "Error"                      // Failed
};

Application::Application() { init(); }
Application::~Application() { shutdown(); }

void Application::init() {
    Logger::init();
    LOG_INFO("AstroSplat starting...");

    WindowConfig config;
    config.title  = "AstroSplat - Procedural Planet Generator";
    config.width  = 1280;
    config.height = 720;
    config.vsync  = true;
    m_window = std::make_unique<Window>(config);

    m_camera = std::make_unique<Camera>();
    m_camera->setTarget(glm::vec3(0.0f, 0.0f, -10.0f));
    m_camera->setPosition(glm::vec3(0.0f, 0.0f, 6.0f));

    m_window->setScrollCallback([this](double, double yoffset) {
        if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse) return;
        m_camera->zoom(static_cast<float>(-yoffset) * 0.3f);
    });

    m_window->setResizeCallback([this](int width, int height) {
        m_renderer->resize(width, height);
        m_camera->setAspectRatio(static_cast<float>(width) / static_cast<float>(height));
    });

    m_renderer = std::make_unique<VulkanRenderer>();
    LOG_INFO("Using Vulkan rendering backend");
    m_renderer->init(m_window->getWidth(), m_window->getHeight(),
                     m_window->getHandle());

    m_ui = std::make_unique<UIManager>();
    m_ui->init(m_window->getHandle(),
               static_cast<VulkanRenderer*>(m_renderer.get()));

    // ML pipeline
    m_nasa      = std::make_unique<NasaApiClient>();
    m_inference = std::make_unique<InferenceEngine>();
    m_cacheManager = std::make_unique<CacheManager>();

    m_ui->setExoplanetCallback([this](const std::string& name) {
        loadPlanet(name);
    });

    // Seed autocomplete name list from solar system database + cache
    buildPlanetNameList();

    m_lastFrameTime = m_window->getTime();
    LOG_INFO("Ready");
}

void Application::buildPlanetNameList() {
    m_knownNames.clear();

    // Seed from solar system database
    for (const auto& entry : SolarSystemDatabase::instance().entries()) {
        m_knownNames.insert(entry.name);
    }

    // Add cached exoplanet names — retrieve proper casing from cached JSON
    auto cached = m_cacheManager->listCached();
    for (const auto& cachedName : cached) {
        auto data = m_cacheManager->retrieve(cachedName);
        if (data && !data->name.empty()) {
            m_knownNames.insert(data->name);
        } else {
            m_knownNames.insert(cachedName);
        }
    }

    // Convert to sorted vector and pass to UI
    std::vector<std::string> sortedNames(m_knownNames.begin(), m_knownNames.end());
    std::sort(sortedNames.begin(), sortedNames.end());
    m_ui->setCachedNames(sortedNames);

    LOG_INFO("Autocomplete: {} planet names loaded", sortedNames.size());
}

void Application::runIntro() {
    // Hide the raymarched planet body while keeping the procedural starfield live
    auto savedParams               = m_renderer->params();
    m_renderer->params().radius            = 0.001f;
    m_renderer->params().atmosphereDensity = 0.0f;
    m_renderer->params().cloudsDensity     = 0.0f;

    IntroAnimation intro;
    bool borderSynced = false;
    m_lastFrameTime = m_window->getTime();

    while (!m_window->shouldClose() && !intro.isDone()) {
        double currentTime = m_window->getTime();
        float  dt          = static_cast<float>(currentTime - m_lastFrameTime);
        m_lastFrameTime    = currentTime;
        dt = std::min(dt, 0.05f);   // clamp to avoid spiral-of-death on stall

        m_window->pollEvents();

        // Skip on Escape
        GLFWwindow* w = m_window->getHandle();
        if (glfwGetKey(w, GLFW_KEY_ESCAPE) == GLFW_PRESS ||
            glfwGetKey(w, GLFW_KEY_SPACE)  == GLFW_PRESS)
            break;

        intro.update(dt);

        m_renderer->beginFrame();
        m_renderer->render(*m_camera);

        m_ui->beginFrame();
        ImGuiIO& io = ImGui::GetIO();

        // ── Fade in the real UI panel once the border is assembled ────────────
        float uiAlpha = intro.getUIAlpha();
        {
            // Lerp planet params hidden→visible so the planet fades in with the UI
            auto& rp = m_renderer->params();
            rp.radius            = savedParams.radius            * uiAlpha;
            rp.atmosphereDensity = savedParams.atmosphereDensity * uiAlpha;
            rp.cloudsDensity     = savedParams.cloudsDensity     * uiAlpha;
        }
        if (uiAlpha > 0.f) {
            ImVec2 winPos, winSize;
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, uiAlpha);
            m_ui->render(m_renderer->params(), &winPos, &winSize);
            ImGui::PopStyleVar();

            // Snap the constellation border to wherever ImGui actually placed the panel
            if (!borderSynced) {
                intro.syncBorderToWindow(winPos.x, winPos.y, winSize.x, winSize.y);
                borderSynced = true;
            }
        }

        // ── Full-screen transparent overlay (particles drawn on top of UI) ───
        ImGui::SetNextWindowPos({ 0.f, 0.f });
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::SetNextWindowBgAlpha(0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f });
        if (ImGui::Begin("##intro_overlay", nullptr,
                ImGuiWindowFlags_NoDecoration        |
                ImGuiWindowFlags_NoMove              |
                ImGuiWindowFlags_NoScrollbar         |
                ImGuiWindowFlags_NoSavedSettings     |
                ImGuiWindowFlags_NoBringToFrontOnFocus)) {
            ImGui::PopStyleVar();
            intro.render(ImGui::GetWindowDrawList(),
                         io.DisplaySize.x, io.DisplaySize.y);
        } else {
            ImGui::PopStyleVar();
        }
        ImGui::End();

        m_ui->endFrame(static_cast<VulkanRenderer*>(m_renderer.get()));
        m_renderer->endFrame();
        m_window->swapBuffers();
    }

    m_renderer->params() = savedParams;
    m_lastFrameTime = m_window->getTime();
}

void Application::run() {
    runIntro();
    while (!m_window->shouldClose() && m_running) {
        double currentTime = m_window->getTime();
        float  deltaTime   = static_cast<float>(currentTime - m_lastFrameTime);
        m_lastFrameTime    = currentTime;

        m_window->pollEvents();
        update(deltaTime);
        render();
        m_window->swapBuffers();
    }
}

void Application::loadPlanet(const std::string& name) {
    if (m_planetLoading) return;
    LOG_INFO("Loading planet: {}", name);

    // ── Step 1: Solar system direct lookup ────────────────────────────────────
    // Resolve synchronously (instant) so the renderer updates this frame.
    // Then optionally fire a background AI validation run.
    const auto* solarEntry = SolarSystemDatabase::instance().findByName(name);
    if (solarEntry) {
        LOG_INFO("Solar system match: {}", solarEntry->name);
        std::string cat = ExoplanetMapper::categoryName(
            ExoplanetMapper::classify(solarEntry->physicalData));
        m_currentStatus = solarEntry->name + "  |  " + cat + "  |  Solar System";
        m_ui->setExoplanetStatus(m_currentStatus);

        // Store ExoplanetData for the info panel
        m_loadedExoData = solarEntry->physicalData;
        m_ui->setExoplanetData(&(*m_loadedExoData));

        // Start fade transition instead of direct assignment
        m_targetParams = solarEntry->visualParams;
        m_savedBaseParams = solarEntry->visualParams;
        m_transitioning = true;
        m_transitionShrinking = true;
        m_transitionAlpha = 1.0f;

        // ── Background validation: run AI pipeline on known physical data ──
        // Compares AI output to our hand-tuned visual params → accuracy score.
        if (m_inference->isAvailable() && !m_validationRunning) {
            m_validationRunning = true;
            auto entry = solarEntry;  // pointer to static storage, safe to capture
            m_validationFuture = std::async(std::launch::async,
                [this, entry]() -> std::string {
                    auto data = entry->physicalData;
                    data = m_inference->fillMissingParametersSync(std::move(data));

                    std::set<std::string> physicsFields;
                    ExoplanetMapper::toPlanetParams(data, &physicsFields, nullptr);

                    // Exclude the planet itself from analog matching
                    std::string analogContext;
                    if (data.mass_earth.hasValue() && data.radius_earth.hasValue() &&
                        data.equilibrium_temp_k.hasValue()) {
                        auto analog = SolarSystemDatabase::instance().findClosestAnalog(
                            data.mass_earth.value, data.radius_earth.value,
                            data.equilibrium_temp_k.value, 0.35f,
                            entry->name);  // exclude self
                        if (analog) {
                            analogContext = fmt::format(
                                "{} (similarity {:.0f}%) — {}",
                                analog->entry->name, analog->score * 100.0f,
                                analog->entry->description);
                        }
                    }

                    auto aiJson = m_inference->inferRenderParamsSync(
                        data, analogContext, physicsFields);

                    // Build predicted params from physics base + AI (no analog visual base)
                    PlanetParams predicted = ExoplanetMapper::toPlanetParams(data, nullptr, nullptr);
                    ExoplanetMapper::applyAIRenderOverrides(predicted, aiJson, physicsFields);

                    auto report = ExoplanetMapper::validate(predicted, entry->visualParams);

                    LOG_INFO("[Validation] {} → overall {:.1f}%",
                             entry->name, report.overall_score * 100.0f);
                    for (auto& [field, score] : report.field_scores)
                        LOG_DEBUG("  {:<20s} {:.0f}%", field, score * 100.0f);

                    return fmt::format("AI acc: {:.0f}%", report.overall_score * 100.0f);
                });
        }
        return;
    }

    // ── Steps 2-6: Unknown exoplanet — async pipeline ─────────────────────────
    m_planetLoading = true;
    m_ui->setLoading(true);
    m_ui->setExoplanetStatus("Searching...");
    m_pipelineStage.store(static_cast<int>(PipelineStage::QueryingNasa));

    m_planetFuture = std::async(std::launch::async,
        [this, name]() -> LoadResult {

            // ── Step 2: NASA Exoplanet Archive query ───────────────────────
            m_pipelineStage.store(static_cast<int>(PipelineStage::QueryingNasa));
            auto results = m_nasa->queryByNameSync(name);
            if (results.empty()) {
                m_pipelineStage.store(static_cast<int>(PipelineStage::Failed));
                return {std::nullopt, "Not found: \"" + name + "\"", std::nullopt};
            }

            auto data = results[0];
            data.calculateDerivedValues();

            // ── Step 3: AI fills missing atmosphere / physical fields ───────
            m_pipelineStage.store(static_cast<int>(PipelineStage::RunningAI));
            if (m_inference->isAvailable()) {
                data = m_inference->fillMissingParametersSync(std::move(data));
            }

            // ── Step 4: Find closest solar-system analog for context ────────
            m_pipelineStage.store(static_cast<int>(PipelineStage::MappingParams));
            std::string           analogContext;
            std::set<std::string> physicsFields;
            ExoplanetMapper::toPlanetParams(data, &physicsFields, nullptr);

            if (data.mass_earth.hasValue() && data.radius_earth.hasValue() &&
                data.equilibrium_temp_k.hasValue()) {
                auto analog = SolarSystemDatabase::instance().findClosestAnalog(
                    data.mass_earth.value, data.radius_earth.value,
                    data.equilibrium_temp_k.value, 0.35f);
                if (analog) {
                    analogContext = fmt::format(
                        "{} (similarity {:.0f}%) — {}",
                        analog->entry->name, analog->score * 100.0f,
                        analog->entry->description);
                }
            }

            // ── Step 5: AI fills remaining unknown render fields ────────────
            nlohmann::json aiJson;
            if (m_inference->isAvailable()) {
                aiJson = m_inference->inferRenderParamsSync(data, analogContext, physicsFields);
            }

            // ── Step 6: Physics base + AI fill → final PlanetParams ─────────
            AnalogMatch usedAnalog;
            PlanetParams params = ExoplanetMapper::toRenderParams(data, aiJson, &usedAnalog);

            std::string category = ExoplanetMapper::categoryName(
                ExoplanetMapper::classify(data));
            std::string label = data.name + "  |  " + category;
            if (usedAnalog.entry) {
                label += fmt::format("  |  ~{} ({:.0f}%)",
                                     usedAnalog.entry->name, usedAnalog.score * 100.0f);
            }

            m_pipelineStage.store(static_cast<int>(PipelineStage::Done));
            return {params, label, data};
        });
}

void Application::update(float deltaTime) {
    static bool   dragging = false;
    static double lastX = 0, lastY = 0;

    if (ImGui::GetIO().WantCaptureMouse) {
        dragging = false;
    } else if (m_window->isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
        double x, y;
        m_window->getCursorPos(x, y);
        if (!dragging) {
            dragging = true;
            lastX = x; lastY = y;
        } else {
            float dx = static_cast<float>(x - lastX);
            float dy = static_cast<float>(y - lastY);
            m_camera->rotate(dx * 0.005f, dy * 0.005f);
            lastX = x; lastY = y;
        }
    } else {
        dragging = false;
    }

    m_camera->update(deltaTime);

    // ── Fade transition between planets ──────────────────────────────────
    if (m_transitioning) {
        float speed = 3.0f; // ~0.33s per phase, ~0.67s total
        if (m_transitionShrinking) {
            m_transitionAlpha -= deltaTime * speed;
            if (m_transitionAlpha <= 0.0f) {
                m_transitionAlpha = 0.0f;
                // Swap to new planet params at zero-size
                m_renderer->params() = m_savedBaseParams;
                m_transitionShrinking = false;
            }
        } else {
            m_transitionAlpha += deltaTime * speed;
            if (m_transitionAlpha >= 1.0f) {
                m_transitionAlpha = 1.0f;
                m_transitioning = false;
                // Ensure final params are exact target (no floating point drift)
                m_renderer->params() = m_savedBaseParams;
            }
        }
        // Apply fade multiplier to visual parameters (always from saved base)
        auto& rp = m_renderer->params();
        rp.radius = m_savedBaseParams.radius * m_transitionAlpha;
        rp.atmosphereDensity = m_savedBaseParams.atmosphereDensity * m_transitionAlpha;
        rp.cloudsDensity = m_savedBaseParams.cloudsDensity * m_transitionAlpha;
    }

    // Poll pipeline stage and update status text while loading
    if (m_planetLoading && m_planetFuture.valid()) {
        if (m_planetFuture.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            // Pipeline complete — extract results
            auto [params, status, exoData] = m_planetFuture.get();
            if (params.has_value()) {
                // Start fade transition instead of direct assignment
                m_targetParams = *params;
                m_savedBaseParams = *params;
                m_transitioning = true;
                m_transitionShrinking = true;
                m_transitionAlpha = 1.0f;
            }
            // Store ExoplanetData for info panel
            if (exoData.has_value()) {
                m_loadedExoData = std::move(*exoData);
                m_ui->setExoplanetData(&(*m_loadedExoData));
                // Add the loaded planet name to known names and refresh autocomplete
                m_knownNames.insert(m_loadedExoData->name);
                std::vector<std::string> sortedNames(m_knownNames.begin(), m_knownNames.end());
                std::sort(sortedNames.begin(), sortedNames.end());
                m_ui->setCachedNames(sortedNames);
            }
            m_currentStatus = status;
            m_ui->setExoplanetStatus(m_currentStatus);
            m_ui->setLoading(false);
            m_planetLoading = false;
            m_pipelineStage.store(static_cast<int>(PipelineStage::Idle));
            LOG_INFO("Planet loaded: {}", m_currentStatus);
        } else {
            // Still loading — update status text from pipeline stage
            int stage = m_pipelineStage.load();
            if (stage >= 0 && stage < static_cast<int>(sizeof(kStageMessages) / sizeof(kStageMessages[0]))) {
                m_ui->setExoplanetStatus(kStageMessages[stage]);
            }
        }
    }

    // Append validation accuracy score once background validation completes
    if (m_validationRunning && m_validationFuture.valid()) {
        if (m_validationFuture.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            auto scoreStr = m_validationFuture.get();
            m_currentStatus += "  |  " + scoreStr;
            m_ui->setExoplanetStatus(m_currentStatus);
            m_validationRunning = false;
            LOG_INFO("Validation complete: {}", m_currentStatus);
        }
    }
}

void Application::render() {
    m_renderer->beginFrame();
    m_renderer->render(*m_camera);

    m_ui->beginFrame();
    m_ui->render(m_renderer->params());
    m_ui->endFrame(static_cast<VulkanRenderer*>(m_renderer.get()));

    m_renderer->endFrame();
}

void Application::shutdown() {
    m_ui.reset();
    m_renderer.reset();
    m_camera.reset();
    m_window.reset();
}

}  // namespace astrocore
