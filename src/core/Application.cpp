#include "core/Application.hpp"
#include "core/Logger.hpp"
#include "render/Camera.hpp"
#include "intro/IntroAnimation.hpp"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <chrono>
#include <spdlog/fmt/fmt.h>
#include <set>

#ifdef ASTRO_METAL
#  include "render/MetalRenderer.hpp"
#else
#  include "render/Renderer.hpp"
#endif

namespace astrocore {

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

#ifdef ASTRO_METAL
    m_renderer = std::make_unique<MetalRenderer>();
    LOG_INFO("Using Metal rendering backend");
#else
    m_renderer = std::make_unique<Renderer>();
    LOG_INFO("Using OpenGL rendering backend");
#endif

    // Pass the GLFW window handle — Metal needs it to attach CAMetalLayer;
    // the OpenGL renderer ignores it.
    m_renderer->init(m_window->getWidth(), m_window->getHeight(),
                     m_window->getHandle());

    m_ui = std::make_unique<UIManager>();

#ifdef ASTRO_METAL
    m_ui->init(m_window->getHandle(), m_renderer->getMetalDevice());
#else
    m_ui->init(m_window->getHandle());
#endif

    // ML pipeline
    m_nasa      = std::make_unique<NasaApiClient>();
    m_inference = std::make_unique<InferenceEngine>();

    m_ui->setExoplanetCallback([this](const std::string& name) {
        loadPlanet(name);
    });

    m_lastFrameTime = m_window->getTime();
    LOG_INFO("Ready");
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

#ifdef ASTRO_METAL
        MetalFrameContext ctx = m_renderer->getMetalContext();
        // If the CAMetalLayer hasn't produced a drawable yet (common on first
        // few frames), skip the ImGui frame entirely to avoid feeding a nil
        // renderPassDescriptor into ImGui_ImplMetal_NewFrame which causes it
        // to cache a zero-format pipeline state and spam error logs.
        if (!ctx.renderPassDescriptor) {
            m_renderer->endFrame();
            m_window->swapBuffers();
            continue;
        }
        m_ui->beginFrame(ctx.renderPassDescriptor);
#else
        m_ui->beginFrame();
#endif
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

#ifdef ASTRO_METAL
        m_ui->endFrame(ctx.commandBuffer, ctx.commandEncoder);
#else
        m_ui->endFrame();
#endif
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
        m_renderer->params() = solarEntry->visualParams;

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
    m_ui->setExoplanetStatus("Searching...");

    m_planetFuture = std::async(std::launch::async,
        [this, name]() -> LoadResult {

            // ── Step 2: NASA Exoplanet Archive query ───────────────────────
            auto results = m_nasa->queryByNameSync(name);
            if (results.empty()) {
                return {std::nullopt, "Not found: \"" + name + "\""};
            }

            auto data = results[0];
            data.calculateDerivedValues();

            // ── Step 3: AI fills missing atmosphere / physical fields ───────
            if (m_inference->isAvailable()) {
                data = m_inference->fillMissingParametersSync(std::move(data));
            }

            // ── Step 4: Find closest solar-system analog for context ────────
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

            return {params, label};
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

    // Apply planet load result if ready
    if (m_planetLoading && m_planetFuture.valid()) {
        if (m_planetFuture.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            auto [params, status] = m_planetFuture.get();
            if (params.has_value()) {
                m_renderer->params() = *params;
            }
            m_currentStatus = status;
            m_ui->setExoplanetStatus(m_currentStatus);
            m_planetLoading = false;
            LOG_INFO("Planet loaded: {}", m_currentStatus);
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

#ifdef ASTRO_METAL
    // For Metal, ImGui renders into the active command encoder.
    // We hand UIManager the current Metal frame context.
    MetalFrameContext ctx = m_renderer->getMetalContext();
    m_ui->beginFrame(ctx.renderPassDescriptor);
    m_ui->render(m_renderer->params());
    m_ui->endFrame(ctx.commandBuffer, ctx.commandEncoder);
#else
    m_ui->beginFrame();
    m_ui->render(m_renderer->params());
    m_ui->endFrame();
#endif

    m_renderer->endFrame();
}

void Application::shutdown() {
    m_ui.reset();
    m_renderer.reset();
    m_camera.reset();
    m_window.reset();
}

}  // namespace astrocore
