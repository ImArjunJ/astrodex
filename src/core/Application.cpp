#include "core/Application.hpp"
#include "core/Logger.hpp"
#include "config/SystemConfig.hpp"
#include "render/Renderer.hpp"
#include "render/Camera.hpp"
#include "render/ExoplanetConverter.hpp"
#include "data/ExoplanetPredictor.hpp"
#include "intro/IntroAnimation.hpp"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <chrono>
#include <thread>

namespace astrocore {

Application::Application() {
    init();
}

Application::~Application() {
    shutdown();
}

void Application::init() {
    Logger::init();
    LOG_INFO("AstroSplat starting...");

    WindowConfig config;
    config.title = "AstroSplat - Procedural Planet Generator";
    config.width = 1280;
    config.height = 720;
    config.vsync = true;
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

    m_renderer = std::make_unique<Renderer>();
    m_renderer->init(m_window->getWidth(), m_window->getHeight());

    m_ui = std::make_unique<UIManager>();
    m_ui->init(m_window->getHandle());

    // Preset manager
    m_presetManager.setPresetsDirectory("configs");
    m_presetManager.scanPresets();
    if (m_presetManager.getAvailablePresets().empty()) {
        m_presetManager.generateBuiltInPresets();
    }

    // Exoplanet data aggregator
    AggregatorConfig aggConfig;
    aggConfig.query_nasa = true;
    aggConfig.query_exomast = true;
    aggConfig.query_exoplanet_eu = true;
    m_dataAggregator = std::make_unique<ExoplanetDataAggregator>(aggConfig);
    m_dataAggregator->preloadExoplanetEuCatalog();

    // AI inference engine
    m_inferenceEngine = std::make_unique<InferenceEngine>();
    if (m_inferenceEngine->isAvailable()) {
        LOG_INFO("AI inference available");
    }

    // Galaxy view setup
    m_galaxy = std::make_unique<GalaxyView>();
    m_galaxy->setExoplanetCallback([this](const std::string& name) {
        loadPlanet(name);
    });
    m_galaxy->setFetchMetadataCallback([this](const std::string& name) {
        // Fetch metadata from NASA in background, then predict missing values
        m_galaxy->setFetchingMetadata(true);
        std::thread([this, name]() {
            try {
                auto result = m_dataAggregator->queryPlanetSync(name);
                ExoplanetData d = result.data;

                // Use predictor to fill ALL missing fields
                ExoplanetPredictor predictor;
                predictor.setInferenceEngine(m_inferenceEngine.get());
                d = predictor.fillAllMissing(std::move(d));

                // Calculate distance in light years from parsecs
                float distLY = 0.f;
                if (d.distance_ly.hasValue()) {
                    distLY = static_cast<float>(d.distance_ly.value);
                } else if (d.host_star.distance_pc.hasValue()) {
                    distLY = static_cast<float>(d.host_star.distance_pc.value * 3.26156);
                }

                // All values are now guaranteed to be valid (no NaN)
                m_galaxy->updatePlanetMetadata(
                    name,
                    d.host_star.name,
                    distLY,
                    static_cast<float>(d.radius_earth.value),
                    static_cast<float>(d.mass_earth.value),
                    static_cast<float>(d.equilibrium_temp_k.value),
                    d.host_star.gaia_dr3_id
                );
                LOG_INFO("Fetched and predicted metadata for {}: host={}, dist={:.1f}ly, "
                         "radius={:.2f}R_E, mass={:.2f}M_E, temp={:.0f}K [sources: R={}, M={}, T={}]",
                         name, d.host_star.name, distLY,
                         d.radius_earth.value, d.mass_earth.value, d.equilibrium_temp_k.value,
                         dataSourceToString(d.radius_earth.source),
                         dataSourceToString(d.mass_earth.source),
                         dataSourceToString(d.equilibrium_temp_k.source));
            } catch (const std::exception& e) {
                LOG_WARN("Failed to fetch metadata for {}: {}", name, e.what());
            }
            m_galaxy->setFetchingMetadata(false);
        }).detach();
    });

    // Load cached planets into galaxy view
    auto cachedPlanets = ExoplanetConverter::listCachedPlanets();
    for (const auto& planet : cachedPlanets) {
        m_galaxy->addExoplanet(planet.name, planet.type, 0.0f);
    }
    LOG_INFO("Added {} cached planets to galaxy view", cachedPlanets.size());

    // Initialize simulation and load solar system
    m_simulation.init();
    m_simulation.loadSolarSystem();
    m_simulation.setFocusBody("Earth");

    // Create blank cursor for mouse lock
    unsigned char pixels[4] = {0, 0, 0, 0};
    GLFWimage image = {1, 1, pixels};
    m_blankCursor = glfwCreateCursor(&image, 0, 0);

    m_lastFrameTime = m_window->getTime();
    LOG_INFO("Ready");
}

void Application::runIntro() {
    // Hide the planet during intro
    auto savedParams = m_renderer->params();
    m_renderer->params().radius = 0.001f;
    m_renderer->params().atmosphereDensity = 0.0f;
    m_renderer->params().cloudsDensity = 0.0f;

    IntroAnimation intro;
    bool borderSynced = false;
    m_lastFrameTime = m_window->getTime();

    while (!m_window->shouldClose() && !intro.isDone()) {
        double currentTime = m_window->getTime();
        float dt = static_cast<float>(currentTime - m_lastFrameTime);
        m_lastFrameTime = currentTime;
        dt = std::min(dt, 0.05f);

        m_window->pollEvents();

        // Skip on Escape or Space
        GLFWwindow* w = m_window->getHandle();
        if (glfwGetKey(w, GLFW_KEY_ESCAPE) == GLFW_PRESS ||
            glfwGetKey(w, GLFW_KEY_SPACE) == GLFW_PRESS)
            break;

        intro.update(dt);

        m_renderer->beginFrame();
        m_renderer->render(*m_camera);

        m_ui->beginFrame();
        ImGuiIO& io = ImGui::GetIO();

        // Fade in planet with UI
        float uiAlpha = intro.getUIAlpha();
        {
            auto& rp = m_renderer->params();
            rp.radius = savedParams.radius * uiAlpha;
            rp.atmosphereDensity = savedParams.atmosphereDensity * uiAlpha;
            rp.cloudsDensity = savedParams.cloudsDensity * uiAlpha;
        }
        if (uiAlpha > 0.f) {
            ImVec2 winPos, winSize;
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, uiAlpha);
            m_ui->render(m_renderer->params(), &winPos, &winSize);
            ImGui::PopStyleVar();

            if (!borderSynced) {
                intro.syncBorderToWindow(winPos.x, winPos.y, winSize.x, winSize.y);
                borderSynced = true;
            }
        }

        // Full-screen overlay for intro particles
        ImGui::SetNextWindowPos({0.f, 0.f});
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::SetNextWindowBgAlpha(0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.f, 0.f});
        if (ImGui::Begin("##intro_overlay", nullptr,
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoMouseInputs |
                ImGuiWindowFlags_NoBringToFrontOnFocus)) {
            ImGui::PopStyleVar();
            intro.render(ImGui::GetWindowDrawList(), io.DisplaySize.x, io.DisplaySize.y);
        } else {
            ImGui::PopStyleVar();
        }
        ImGui::End();

        m_ui->endFrame();
        m_renderer->endFrame();
        m_window->swapBuffers();
    }

    m_renderer->params() = savedParams;
    m_lastFrameTime = m_window->getTime();
}

void Application::run() {
    // Run intro animation
    runIntro();

    // Start in galaxy mode with planet hidden
    m_savedParams = m_renderer->params();
    m_renderer->params().radius = 0.001f;
    m_renderer->params().atmosphereDensity = 0.0f;
    m_renderer->params().cloudsDensity = 0.0f;
    m_screen = AppScreen::Galaxy;
    m_galaxyFadeTimer = 0.f;

    while (!m_window->shouldClose() && m_running) {
        double currentTime = m_window->getTime();
        float deltaTime = std::min(static_cast<float>(currentTime - m_lastFrameTime), 0.05f);
        m_lastFrameTime = currentTime;

        m_window->pollEvents();

        if (m_screen == AppScreen::Galaxy) {
            renderGalaxy(deltaTime);
        } else if (m_screen == AppScreen::SolarSystem) {
            handleSolarSystemInput();
            m_simulation.update(deltaTime);
            renderSolarSystem(deltaTime);
        } else {
            handleInput();
            update(deltaTime);
            render(deltaTime);
        }

        m_window->swapBuffers();
    }
}

void Application::renderGalaxy(float dt) {
    // Note: Solar system can be accessed via the "Solar System Simulation" button in the sidebar

    ImGuiIO& io = ImGui::GetIO();
    if (!m_galaxy->isInitialized())
        m_galaxy->init(io.DisplaySize.x, io.DisplaySize.y);

    m_galaxy->update(dt, io.DisplaySize.x, io.DisplaySize.y);

    // Check transition states
    bool isZoomingIn = m_galaxy->isZoomingToPlanet();
    bool isViewingPlanet = m_galaxy->isViewingPlanet();
    bool isZoomingOut = m_galaxy->isZoomingOut();
    float transProgress = m_galaxy->getTransitionProgress();

    m_renderer->beginFrame();

    // If zooming in or out, blend between galaxy and planet render
    if (isZoomingIn || isZoomingOut) {
        float rawProgress = isZoomingIn ? transProgress : (1.0f - transProgress);

        // For most of the transition, just show galaxy
        if (rawProgress < 0.92f) {
            // Pure galaxy view while zooming
            m_galaxy->renderBackground(nullptr, io.DisplaySize.x, io.DisplaySize.y);
        } else {
            // Final stretch (92-100%): crossfade from galaxy to planet
            float crossfade = (rawProgress - 0.92f) / 0.08f;  // 0 to 1 over final 8%

            if (crossfade < 0.5f) {
                // First half: galaxy fading out
                m_galaxy->renderBackground(nullptr, io.DisplaySize.x, io.DisplaySize.y);
            } else {
                // Second half: planet fading in
                m_renderer->render(*m_camera, false);
            }
        }
    } else if (isViewingPlanet) {
        // Just render the planet when fully arrived
        m_renderer->render(*m_camera, false);
    } else {
        // Normal galaxy view - render 3D galaxy
        m_galaxy->renderBackground(nullptr, io.DisplaySize.x, io.DisplaySize.y);
    }

    m_ui->beginFrame();

    // Fade-in timer
    m_galaxyFadeTimer += dt;
    const float kFadeDur = 1.5f;
    const float fadeAlpha = std::min(m_galaxyFadeTimer / kFadeDur, 1.f);

    bool switching = m_galaxy->isExplosionDone();

    // Show UI only when not in full planet view
    if (!switching && !isViewingPlanet) {
        m_galaxy->renderUI(io.DisplaySize.x, io.DisplaySize.y);
    }

    // When viewing planet, show "Return to Galaxy" button
    if (isViewingPlanet) {
        ImGui::SetNextWindowPos(ImVec2(20.f, 20.f), ImGuiCond_Always);
        if (ImGui::Begin("##return_btn", nullptr,
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_AlwaysAutoResize |
                ImGuiWindowFlags_NoSavedSettings)) {
            if (ImGui::Button("  < Return to Galaxy  ", ImVec2(180.f, 40.f))) {
                m_galaxy->startZoomToGalaxy();
            }
        }
        ImGui::End();
    }

    // Theme toggle always visible
    if (!switching && !isViewingPlanet)
        m_ui->renderThemeToggle();

    // Transition flash overlay (smooth the galaxy->planet switch)
    if (isZoomingIn || isZoomingOut) {
        float rawProgress = isZoomingIn ? transProgress : (1.0f - transProgress);
        // Create a brief white flash at the crossfade point (around 94-98%)
        if (rawProgress > 0.90f && rawProgress < 1.0f) {
            // Peak at 96%, fade in from 90%, fade out to 100%
            float flashIntensity;
            if (rawProgress < 0.96f) {
                flashIntensity = (rawProgress - 0.90f) / 0.06f;  // Fade in
            } else {
                flashIntensity = 1.0f - (rawProgress - 0.96f) / 0.04f;  // Fade out
            }
            flashIntensity = std::clamp(flashIntensity, 0.0f, 1.0f);
            int flashAlpha = static_cast<int>(flashIntensity * 200);  // Max 200 alpha (not full white)
            ImGui::GetForegroundDrawList()->AddRectFilled(
                {0.f, 0.f}, io.DisplaySize,
                IM_COL32(255, 255, 255, flashAlpha));
        }
    }

    // Fade-in overlay
    if (fadeAlpha < 1.f) {
        int blackAlpha = static_cast<int>((1.f - fadeAlpha) * 255);
        ImGui::GetForegroundDrawList()->AddRectFilled(
            {0.f, 0.f}, io.DisplaySize,
            IM_COL32(0, 0, 0, blackAlpha));
    }

    m_ui->endFrame();
    m_renderer->endFrame();

    // Check for solar system button click
    if (m_galaxy->wasSolarSystemRequested()) {
        m_simulation.loadSolarSystem();
        m_simulation.resume();
        m_camera->setPosition(glm::vec3(0.0f, 50.0f, 100.0f));
        m_camera->setTarget(glm::vec3(0.0f));
        m_screen = AppScreen::SolarSystem;
        LOG_INFO("Switching to Solar System simulation");
        return;
    }

    // Handle zoom transitions - load planet when zoom starts
    static bool wasZooming = false;
    if (isZoomingIn && !wasZooming) {
        // Just started zooming - load the planet so it's ready when we arrive
        const std::string name = m_galaxy->selectedName();
        const int preset = m_galaxy->selectedPreset();

        if (preset >= 0) {
            m_renderer->params() = UIManager::getPreset(preset);
            m_currentStatus = name + "  |  Preset";
        } else {
            loadPlanet(name);
        }

        // Set up camera for planet viewing
        m_camera->setPosition(glm::vec3(0.0f, 0.0f, 15.0f));
        m_camera->setTarget(glm::vec3(0.0f));
        m_renderer->setPlanetPosition(glm::vec3(0.0f, 0.0f, -10.0f));

        // Clear simulation state
        m_simulation.clear();
        m_renderer->clearRings();

        LOG_INFO("Loading planet for zoom transition: {}", name);
    }
    wasZooming = isZoomingIn;

    // When zoom out completes, we're back in galaxy browsing mode
    // (nothing special needed - just continue rendering galaxy)

    if (switching) {
        const std::string name = m_galaxy->selectedName();
        const int preset = m_galaxy->selectedPreset();

        if (preset >= 0) {
            m_renderer->params() = UIManager::getPreset(preset);
            m_currentStatus = name + "  |  Preset";
            m_ui->setExoplanetStatus(m_currentStatus);
            LOG_INFO("Galaxy expand (preset): {}", name);
        } else {
            m_renderer->params() = m_savedParams;
            loadPlanet(name);
        }

        m_borderFadeTimer = 0.f;
        m_borderReleased = false;
        m_planetDetailFadeIn = 0.f;

        // Reset camera to default planet viewing position
        m_camera->setPosition(glm::vec3(0.0f, 0.0f, 15.0f));
        m_camera->setTarget(glm::vec3(0.0f));
        m_renderer->setPlanetPosition(glm::vec3(0.0f, 0.0f, -10.0f));
        m_mouseLocked = false;
        glfwSetInputMode(m_window->getHandle(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);

        // Clear simulation state from solar system mode
        m_simulation.clear();
        m_renderer->clearRings();

        m_screen = AppScreen::PlanetDetail;
        LOG_INFO("Switching to PlanetDetail for: {}", name);
    }
}

void Application::handleSolarSystemInput() {
    GLFWwindow* window = m_window->getHandle();

    // Tab - toggle mouse lock
    static bool tabWasPressed = false;
    bool tabPressed = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;
    if (tabPressed && !tabWasPressed) {
        m_mouseLocked = !m_mouseLocked;
        if (m_mouseLocked) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            if (glfwRawMouseMotionSupported()) {
                glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
            }
            glfwGetCursorPos(window, &m_lastMouseX, &m_lastMouseY);
        } else {
            glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
    tabWasPressed = tabPressed;

    // FPS mouse look
    if (m_mouseLocked) {
        double x, y;
        glfwGetCursorPos(window, &x, &y);
        float dx = static_cast<float>(x - m_lastMouseX);
        float dy = static_cast<float>(y - m_lastMouseY);
        m_lastMouseX = x;
        m_lastMouseY = y;
        m_camera->rotate(-dx * 0.003f, dy * 0.003f);
    }

    // Skip other input if ImGui wants keyboard
    if (ImGui::GetIO().WantCaptureKeyboard && !m_mouseLocked) return;

    // WASD/QE movement
    float moveSpeed = m_cameraSpeed * 0.016f;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) moveSpeed *= 5.0f;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) m_camera->moveForward(moveSpeed);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) m_camera->moveForward(-moveSpeed);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) m_camera->moveRight(-moveSpeed);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) m_camera->moveRight(moveSpeed);
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) m_camera->moveUp(-moveSpeed);
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) m_camera->moveUp(moveSpeed);

    // Space - toggle pause
    static bool spaceWasPressed = false;
    bool spacePressed = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
    if (spacePressed && !spaceWasPressed) {
        m_simulation.togglePause();
    }
    spaceWasPressed = spacePressed;

    // +/= increase time scale
    static bool plusWasPressed = false;
    bool plusPressed = glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS;
    if (plusPressed && !plusWasPressed) {
        m_simulation.setTimeScale(m_simulation.timeScale() * 2.0);
    }
    plusWasPressed = plusPressed;

    // - decrease time scale
    static bool minusWasPressed = false;
    bool minusPressed = glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS;
    if (minusPressed && !minusWasPressed) {
        m_simulation.setTimeScale(m_simulation.timeScale() * 0.5);
    }
    minusWasPressed = minusPressed;

    // 1-5 focus on bodies
    static int lastKey = 0;
    int key = 0;
    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) key = 1;
    else if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) key = 2;
    else if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) key = 3;
    else if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS) key = 4;
    else if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS) key = 5;

    if (key != 0 && key != lastKey) {
        const char* names[] = {"", "Sun", "Earth", "Mars", "Jupiter", "Moon"};
        if (key <= 5) {
            m_simulation.setFocusBody(names[key]);
            auto* focus = m_simulation.focusBody();
            if (focus) {
                if (m_simulation.hasAppearance(focus->id())) {
                    m_renderer->params() = m_simulation.getBodyAppearance(focus->id());
                }
                float viewDistance = focus->renderRadius() * 4.0f;
                m_camera->transitionTo(glm::vec3(0.0f), 1.5f, viewDistance);
            }
        }
    }
    lastKey = key;

    // Escape - back to galaxy
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        m_screen = AppScreen::Galaxy;
        m_galaxyFadeTimer = 0.f;
    }
}

void Application::renderSolarSystem(float dt) {
    m_camera->update(dt);

    m_renderer->beginFrame();

    // Render starfield background
    m_renderer->renderStarfield(*m_camera);

    // Get focus position for coordinate conversion
    glm::dvec3 focusPos = m_simulation.focusBody() ?
        m_simulation.focusBody()->position() : glm::dvec3(0.0);

    // Render all bodies
    for (const auto& body : m_simulation.world().bodies()) {
        // Convert physics position to render position (relative to focus)
        glm::dvec3 relPos = body->position() - focusPos;
        glm::vec3 renderPos = m_simulation.physicsToRender(relPos);

        // Set planet position for rendering
        m_renderer->setPlanetPosition(renderPos);

        // Get appearance params for this body (always set to avoid stale params)
        m_renderer->params() = m_simulation.getBodyAppearance(body->id());

        // Render the body
        m_renderer->render(*m_camera, body->isEmissive());

        // Render ring if present
        if (m_renderer->hasRing(body->id())) {
            m_renderer->renderRing(body->id(), *m_camera, renderPos);
        }
    }

    // Render orbit trails
    m_simulation.renderOrbits(*m_renderer, *m_camera);

    m_renderer->updateRings(dt);

    // UI
    m_ui->beginFrame();

    // Left sidebar with body list
    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::SetNextWindowSize(ImVec2(200, 400));
    ImGui::SetNextWindowBgAlpha(0.85f);
    if (ImGui::Begin("##solar_sidebar", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::Text("Solar System");
        ImGui::Separator();

        auto stats = m_simulation.getStats();
        ImGui::TextDisabled("Time: %.1f days", stats.simulationTime / 86400.0);
        ImGui::TextDisabled("Speed: %.0fx", stats.timeScale);
        ImGui::Text(stats.paused ? "PAUSED" : "Running");
        ImGui::Separator();

        // Clickable body list
        ImGui::TextDisabled("Bodies");
        if (ImGui::BeginChild("##bodylist", ImVec2(-1, 200), true)) {
            for (const auto& body : m_simulation.world().bodies()) {
                bool isFocused = (m_simulation.focusBody() == body.get());
                if (ImGui::Selectable(body->name().c_str(), isFocused)) {
                    m_simulation.setFocusBody(body->id());
                    if (m_simulation.hasAppearance(body->id())) {
                        m_renderer->params() = m_simulation.getBodyAppearance(body->id());
                    }
                    float viewDist = body->renderRadius() * 4.0f;
                    m_camera->transitionTo(glm::vec3(0.0f), 1.0f, viewDist);
                }
            }
        }
        ImGui::EndChild();

        ImGui::Separator();
        ImGui::TextDisabled("Controls");
        ImGui::TextDisabled("WASD/QE: Move");
        ImGui::TextDisabled("Tab: Mouse lock");
        ImGui::TextDisabled("Space: Pause");
        ImGui::TextDisabled("+/-: Speed");

        ImGui::Spacing();
        if (ImGui::Button("Back to Galaxy", ImVec2(-1, 0))) {
            m_screen = AppScreen::Galaxy;
            m_galaxyFadeTimer = 0.f;
        }
    }
    ImGui::End();

    m_ui->renderThemeToggle();
    m_ui->endFrame();

    m_renderer->endFrame();
}

void Application::loadPlanet(const std::string& name) {
    if (m_planetLoading) return;
    LOG_INFO("Loading planet: {}", name);

    // First check if it's cached
    auto cachedParams = ExoplanetConverter::loadCachedParams(name);
    if (cachedParams) {
        m_renderer->params() = *cachedParams;
        m_currentStatus = name;
        m_ui->setExoplanetStatus(m_currentStatus);
        LOG_INFO("Loaded cached params for {}", name);

        // Also fetch metadata in background for DATA tab
        std::thread([this, name]() {
            try {
                auto result = m_dataAggregator->queryPlanetSync(name);
                // Fill ALL missing fields with predictions
                ExoplanetPredictor predictor;
                predictor.setInferenceEngine(m_inferenceEngine.get());
                auto filledData = predictor.fillAllMissing(std::move(result.data));
                m_ui->setCurrentExoplanetData(filledData);
            } catch (...) {
                // Ignore - metadata is optional
            }
        }).detach();
        return;
    }

    // Otherwise, start async load
    m_planetLoading = true;
    m_ui->setExoplanetStatus("Searching...");
    m_ui->clearCurrentExoplanetData();

    m_planetFuture = std::async(std::launch::async,
        [this, name]() -> LoadResult {
            LoadResult result;
            result.hasExoData = false;

            // Query NASA
            auto results = m_dataAggregator->getNasaClient().queryByNameSync(name);
            if (results.empty()) {
                result.status = "Not found: \"" + name + "\"";
                return result;
            }

            auto data = results[0];

            // Fill ALL missing fields with scientific predictions + AI
            ExoplanetPredictor predictor;
            predictor.setInferenceEngine(m_inferenceEngine.get());
            data = predictor.fillAllMissing(std::move(data));

            // Convert to render params
            PlanetParams params = ExoplanetConverter::toPlanetParams(data, m_inferenceEngine.get());

            result.params = params;
            result.status = data.name;
            result.exoData = data;
            result.hasExoData = true;
            return result;
        });
}

void Application::handleInput() {
    GLFWwindow* window = m_window->getHandle();

    // Tab toggle mouse lock
    static bool tabWasPressed = false;
    bool tabPressed = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;
    if (tabPressed && !tabWasPressed) {
        m_mouseLocked = !m_mouseLocked;
        if (m_mouseLocked) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            if (glfwRawMouseMotionSupported()) {
                glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
            }
            glfwGetCursorPos(window, &m_lastMouseX, &m_lastMouseY);
        } else {
            glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
    tabWasPressed = tabPressed;

    if (ImGui::GetIO().WantCaptureKeyboard && !m_mouseLocked) return;

    // Camera speed
    static bool bracketLeftWasPressed = false;
    bool bracketLeftPressed = glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS;
    if (bracketLeftPressed && !bracketLeftWasPressed) {
        m_cameraSpeed = std::max(1.0f, m_cameraSpeed / 2.0f);
    }
    bracketLeftWasPressed = bracketLeftPressed;

    static bool bracketRightWasPressed = false;
    bool bracketRightPressed = glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS;
    if (bracketRightPressed && !bracketRightWasPressed) {
        m_cameraSpeed = std::min(100000.0f, m_cameraSpeed * 2.0f);
    }
    bracketRightWasPressed = bracketRightPressed;

    // WASD movement
    float moveSpeed = m_cameraSpeed;
    float deltaTime = 0.016f;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        moveSpeed *= 5.0f;
    }
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        m_camera->moveForward(moveSpeed * deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        m_camera->moveForward(-moveSpeed * deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        m_camera->moveRight(-moveSpeed * deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        m_camera->moveRight(moveSpeed * deltaTime);
    }
}

void Application::update(float deltaTime) {
    // Update simulation physics
    m_simulation.update(deltaTime);

    // FPS mouse look
    if (m_mouseLocked) {
        double x, y;
        m_window->getCursorPos(x, y);
        float dx = static_cast<float>(x - m_lastMouseX);
        float dy = static_cast<float>(y - m_lastMouseY);
        m_lastMouseX = x;
        m_lastMouseY = y;
        m_camera->rotate(-dx * 0.003f, dy * 0.003f);
    }

    m_camera->update(deltaTime);

    // Check async planet load
    if (m_planetLoading && m_planetFuture.valid()) {
        if (m_planetFuture.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            auto result = m_planetFuture.get();
            if (result.params.has_value()) {
                m_renderer->params() = *result.params;
            }
            m_currentStatus = result.status;
            m_ui->setExoplanetStatus(m_currentStatus);
            if (result.hasExoData) {
                m_ui->setCurrentExoplanetData(result.exoData);
            }
            m_planetLoading = false;
            LOG_INFO("Planet loaded: {}", m_currentStatus);
        }
    }
}

void Application::render(float dt) {
    m_renderer->beginFrame();
    m_renderer->render(*m_camera);

    // Note: Don't render orbit trails in single planet view - that's only for solar system mode

    m_ui->beginFrame();

    ImGuiIO& io = ImGui::GetIO();

    // Border overlay during transition
    if (m_borderFadeTimer >= 0.f) {
        m_borderFadeTimer += dt;
        m_galaxy->update(dt, io.DisplaySize.x, io.DisplaySize.y);

        ImGui::SetNextWindowPos({0.f, 0.f});
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::SetNextWindowBgAlpha(0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.f, 0.f});
        if (ImGui::Begin("##border_overlay", nullptr,
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoMouseInputs |
                ImGuiWindowFlags_NoBringToFrontOnFocus)) {
            ImGui::PopStyleVar();
            m_galaxy->renderBackground(ImGui::GetWindowDrawList(),
                                       io.DisplaySize.x, io.DisplaySize.y);
        } else {
            ImGui::PopStyleVar();
        }
        ImGui::End();

        if (m_borderFadeTimer > 0.50f && !m_borderReleased) {
            m_galaxy->releaseBorder();
            m_borderReleased = true;
        }
        if (m_borderFadeTimer > 0.85f) {
            m_galaxy->reset();
            m_borderFadeTimer = -1.f;
            m_borderReleased = false;
        }
    }

    // Planet detail UI with fade-in
    if (m_planetDetailFadeIn < 1.f)
        m_planetDetailFadeIn = std::min(m_planetDetailFadeIn + dt / 0.5f, 1.f);

    if (m_planetDetailFadeIn < 1.f) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, m_planetDetailFadeIn);
        m_ui->render(m_renderer->params());
        ImGui::PopStyleVar();
    } else {
        m_ui->render(m_renderer->params());
    }

    m_ui->renderThemeToggle();

    // Simulation controls (controls planet rotation animation)
    auto simResult = m_ui->renderSimulationControls(
        m_renderer->isPaused(),
        static_cast<double>(m_renderer->timeScale()),
        false,  // No lock feature in single planet view
        "");

    if (simResult.pauseToggled) {
        m_renderer->setPaused(!m_renderer->isPaused());
    }
    if (simResult.timeScaleChanged) {
        m_renderer->setTimeScale(static_cast<float>(simResult.newTimeScale));
    }

    m_ui->endFrame();
    m_renderer->endFrame();

    // Back button returns to galaxy
    if (m_ui->wasBackPressed()) {
        m_savedParams = m_renderer->params();
        m_renderer->params().radius = 0.001f;
        m_renderer->params().atmosphereDensity = 0.0f;
        m_renderer->params().cloudsDensity = 0.0f;
        m_galaxy->reset();
        m_borderFadeTimer = -1.f;
        m_borderReleased = false;
        m_planetDetailFadeIn = 1.f;
        m_screen = AppScreen::Galaxy;
        LOG_INFO("Back to Galaxy screen");
    }
}

void Application::shutdown() {
    if (m_blankCursor) {
        glfwDestroyCursor(m_blankCursor);
        m_blankCursor = nullptr;
    }
    m_galaxy.reset();
    m_ui.reset();
    m_renderer.reset();
    m_camera.reset();
    m_window.reset();
}

void Application::loadExoplanetIntoSimulation(const ExoplanetData& exo) {
    LOG_INFO("Loading exoplanet {} into simulation", exo.name);

    PlanetParams params = ExoplanetConverter::toPlanetParams(exo, m_inferenceEngine.get());

    SystemConfig config;
    config.name = exo.name + " System";
    config.description = "Exoplanet from NASA Archive";
    config.source = "nasa_tap";
    config.visualScale = 1.0;

    float visualPlanetRadius = params.radius;
    float visualStarRadius = visualPlanetRadius * 8.0f;
    float starDistance = visualPlanetRadius * 40.0f;

    BodyConfig star;
    star.name = exo.host_star.name.empty() ? "Host Star" : exo.host_star.name;
    star.type = "Star";
    star.mass = constants::SOLAR_MASS;
    star.radius = visualStarRadius;
    star.position = {starDistance, starDistance * 0.3, -starDistance * 0.2};
    star.velocity = {0.0, 0.0, 0.0};
    star.rotationPeriod = 25.0 * constants::DAY;
    config.bodies.push_back(star);

    BodyConfig planet;
    planet.name = exo.name;
    planet.type = "Planet";
    planet.mass = exo.mass_earth.hasValue() ?
        exo.mass_earth.value * constants::EARTH_MASS : constants::EARTH_MASS;
    planet.radius = visualPlanetRadius;
    planet.position = {0.0, 0.0, 0.0};
    planet.velocity = {0.0, 0.0, 0.0};
    planet.rotationPeriod = constants::DAY;
    config.bodies.push_back(planet);

    m_simulation.loadFromConfig(config);

    for (const auto& body : m_simulation.world().bodies()) {
        if (body->type() == BodyType::Planet) {
            m_simulation.setBodyAppearance(body->id(), params);
            m_simulation.setSelectedBody(body->id());
            m_simulation.setFocusBody(body->id());
            m_renderer->params() = params;
            break;
        }
    }

    m_simulation.pause();
    m_renderer->clearRings();
}

}  // namespace astrocore
