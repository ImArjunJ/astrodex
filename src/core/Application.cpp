#include "core/Application.hpp"
#include "core/Logger.hpp"
#include "config/SystemConfig.hpp"
#include "render/Renderer.hpp"
#include "render/Camera.hpp"
#include "render/ExoplanetConverter.hpp"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <chrono>

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

    // Window
    WindowConfig config;
    config.title = "AstroSplat - Space Physics Simulation";
    config.width = 1280;
    config.height = 720;
    config.vsync = true;
    m_window = std::make_unique<Window>(config);

    // Camera — start close to see the focused planet
    m_camera = std::make_unique<Camera>();
    m_camera->setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
    m_camera->setPosition(glm::vec3(0.0f, 0.0f, 15.0f));  // Close enough to see detail
    m_camera->setFreeMode(true);  // Start in free mode for sandbox

    m_window->setScrollCallback([this](double, double yoffset) {
        if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse) return;
        m_camera->zoom(static_cast<float>(-yoffset) * 0.3f);
    });

    m_window->setResizeCallback([this](int width, int height) {
        m_renderer->resize(width, height);
        m_camera->setAspectRatio(static_cast<float>(width) / static_cast<float>(height));
    });

    // Renderer
    m_renderer = std::make_unique<Renderer>();
    m_renderer->init(m_window->getWidth(), m_window->getHeight());

    // UI
    m_ui = std::make_unique<UIManager>();
    m_ui->init(m_window->getHandle());

    // Preset manager - scan for available presets
    m_presetManager.setPresetsDirectory("configs");
    m_presetManager.scanPresets();
    if (m_presetManager.getAvailablePresets().empty()) {
        m_presetManager.generateBuiltInPresets();
    }

    // Exoplanet data aggregator (queries NASA, ExoMAST, Exoplanet.eu)
    AggregatorConfig aggConfig;
    aggConfig.query_nasa = true;
    aggConfig.query_exomast = true;      // Get atmospheric spectroscopy data!
    aggConfig.query_exoplanet_eu = true; // Get molecules, albedo, measured temps!
    m_dataAggregator = std::make_unique<ExoplanetDataAggregator>(aggConfig);

    // Start preloading Exoplanet.eu catalog in background (faster searches later)
    LOG_INFO("Preloading Exoplanet.eu catalog in background...");
    m_dataAggregator->preloadExoplanetEuCatalog();

    // AI inference engine
    m_inferenceEngine = std::make_unique<InferenceEngine>();
    if (m_inferenceEngine->isAvailable()) {
        LOG_INFO("AI inference available - will use Claude to fill missing exoplanet data");
    } else {
        LOG_WARN("AI inference not available - will use heuristics only");
    }

    // Initialize simulation with solar system
    m_simulation.init();
    m_simulation.loadSolarSystem();
    LOG_INFO("Loaded solar system simulation with {} bodies", static_cast<int>(m_simulation.world().bodyCount()));

    // Set up ring renderers for bodies with rings
    for (const auto& body : m_simulation.world().bodies()) {
        if (m_simulation.hasRing(body->id())) {
            auto* ringParams = m_simulation.getRingParams(body->id());
            if (ringParams) {
                m_renderer->setupRing(body->id(), *ringParams,
                                      body->renderRadius(), static_cast<float>(body->mass()));
            }
        }
    }

    // Create transparent cursor for Wayland compatibility
    unsigned char pixels[4] = {0, 0, 0, 0};
    GLFWimage image = {1, 1, pixels};
    m_blankCursor = glfwCreateCursor(&image, 0, 0);

    m_lastFrameTime = m_window->getTime();
    LOG_INFO("Ready - Controls: WASD/QE move, Scroll zoom");
    LOG_INFO("TAB lock mouse (FPS mode), SPACE pause, +/- time scale, 1-5 focus body");
}

void Application::run() {
    while (!m_window->shouldClose() && m_running) {
        double currentTime = m_window->getTime();
        float deltaTime = static_cast<float>(currentTime - m_lastFrameTime);
        m_lastFrameTime = currentTime;

        m_window->pollEvents();
        handleInput();
        update(deltaTime);
        render();
        m_window->swapBuffers();
    }
}

void Application::handleInput() {
    GLFWwindow* window = m_window->getHandle();

    // Tab always works for mouse lock toggle, even when ImGui wants keyboard
    // Other keys skip if ImGui wants keyboard

    // Tab - toggle mouse lock (FPS mode) - always works regardless of ImGui
    static bool tabWasPressed = false;
    bool tabPressed = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;
    if (tabPressed && !tabWasPressed) {
        m_mouseLocked = !m_mouseLocked;
        if (m_mouseLocked) {
            // Disable cursor - this hides it AND provides infinite mouse motion
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            if (glfwRawMouseMotionSupported()) {
                glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
            }
            glfwGetCursorPos(window, &m_lastMouseX, &m_lastMouseY);
            LOG_INFO("Mouse locked - FPS mode (Tab to unlock)");
        } else {
            // Re-enable cursor for UI mode
            glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            LOG_INFO("Mouse unlocked - UI mode (Tab to lock)");
        }
    }
    tabWasPressed = tabPressed;

    // Skip other input if ImGui wants keyboard (but not when mouse is locked for FPS mode)
    if (ImGui::GetIO().WantCaptureKeyboard && !m_mouseLocked) return;

    // Camera speed adjustment with [ and ]
    static bool bracketLeftWasPressed = false;
    bool bracketLeftPressed = glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS;
    if (bracketLeftPressed && !bracketLeftWasPressed) {
        m_cameraSpeed = std::max(1.0f, m_cameraSpeed / 2.0f);
        LOG_INFO("Camera speed: {}", m_cameraSpeed);
    }
    bracketLeftWasPressed = bracketLeftPressed;

    static bool bracketRightWasPressed = false;
    bool bracketRightPressed = glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS;
    if (bracketRightPressed && !bracketRightWasPressed) {
        m_cameraSpeed = std::min(100000.0f, m_cameraSpeed * 2.0f);
        LOG_INFO("Camera speed: {}", m_cameraSpeed);
    }
    bracketRightWasPressed = bracketRightPressed;

    // WASD/QE movement (always active, behavior depends on camera mode)
    float moveSpeed = m_cameraSpeed;  // Use adjustable base speed
    float deltaTime = 0.016f; // Approximate
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        moveSpeed *= 5.0f;  // Fast movement
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
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        m_camera->moveUp(-moveSpeed * deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
        m_camera->moveUp(moveSpeed * deltaTime);
    }

    // Space - toggle pause
    static bool spaceWasPressed = false;
    bool spacePressed = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
    if (spacePressed && !spaceWasPressed) {
        m_simulation.togglePause();
        LOG_INFO("Simulation {}", m_simulation.isPaused() ? "paused" : "resumed");
    }
    spaceWasPressed = spacePressed;

    // +/= - increase time scale
    static bool plusWasPressed = false;
    bool plusPressed = glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS;
    if (plusPressed && !plusWasPressed) {
        m_simulation.setTimeScale(m_simulation.timeScale() * 2.0);
        LOG_INFO("Time scale: {}x real time", m_simulation.timeScale());
    }
    plusWasPressed = plusPressed;

    // - - decrease time scale
    static bool minusWasPressed = false;
    bool minusPressed = glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS;
    if (minusPressed && !minusWasPressed) {
        m_simulation.setTimeScale(m_simulation.timeScale() * 0.5);
        LOG_INFO("Time scale: {}x real time", m_simulation.timeScale());
    }
    minusWasPressed = minusPressed;

    // 1-5 - focus on different bodies
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
                LOG_INFO("Focused on {}", focus->name());
                // Update renderer params for focused body
                if (m_simulation.hasAppearance(focus->id())) {
                    m_renderer->params() = m_simulation.getBodyAppearance(focus->id());
                }
                // Calculate good view distance based on planet size
                float viewDistance = focus->renderRadius() * 4.0f;
                // Smooth camera transition - focus body is always at origin in render space
                m_camera->transitionTo(glm::vec3(0.0f), 1.5f, viewDistance);
            }
        }
    }
    lastKey = key;
}

void Application::update(float deltaTime) {
    // Update simulation
    m_simulation.update(deltaTime);

    // Update ring particle orbits
    m_renderer->updateRings(deltaTime);

    // FPS-style mouse look when locked
    if (m_mouseLocked) {
        double x, y;
        m_window->getCursorPos(x, y);
        float dx = static_cast<float>(x - m_lastMouseX);
        float dy = static_cast<float>(y - m_lastMouseY);
        m_lastMouseX = x;
        m_lastMouseY = y;

        // Apply rotation (negate dx for correct left/right)
        m_camera->rotate(-dx * 0.003f, dy * 0.003f);
    }

    // Right-click to select a body
    static bool rightWasPressed = false;
    bool rightPressed = m_window->isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);
    if (rightPressed && !rightWasPressed && !ImGui::GetIO().WantCaptureMouse) {
        double mx, my;
        m_window->getCursorPos(mx, my);

        // Convert mouse position to NDC
        float ndcX = (2.0f * static_cast<float>(mx) / m_window->getWidth()) - 1.0f;
        float ndcY = 1.0f - (2.0f * static_cast<float>(my) / m_window->getHeight());

        // Create ray from camera
        glm::vec4 rayClip(ndcX, ndcY, -1.0f, 1.0f);
        glm::mat4 invProj = glm::inverse(m_camera->getProjectionMatrix());
        glm::vec4 rayEye = invProj * rayClip;
        rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);

        glm::mat4 invView = glm::inverse(m_camera->getViewMatrix());
        glm::vec3 rayWorld = glm::normalize(glm::vec3(invView * rayEye));

        // Pick body
        uint64_t pickedId = m_simulation.pickBody(m_camera->getPosition(), rayWorld);
        if (pickedId != 0) {
            m_simulation.setSelectedBody(pickedId);
            auto* selected = m_simulation.selectedBody();
            if (selected) {
                LOG_INFO("Selected: {}", selected->name());
            }
        } else {
            m_simulation.clearSelection();
        }
    }
    rightWasPressed = rightPressed;

    m_camera->update(deltaTime);
}

void Application::render() {
    m_renderer->beginFrame();

    // Render starfield background first
    m_renderer->renderStarfield(*m_camera);

    // Render ALL bodies with the detailed raymarched shader
    glm::dvec3 focusPos = m_simulation.focusBody() ?
        m_simulation.focusBody()->position() : glm::dvec3(0.0);

    // Find the Sun position for accurate lighting
    glm::dvec3 sunPhysicsPos(0.0);
    for (const auto& body : m_simulation.world().bodies()) {
        if (body->isEmissive()) {
            sunPhysicsPos = body->position();
            break;
        }
    }

    for (const auto& body : m_simulation.world().bodies()) {
        if (m_simulation.hasAppearance(body->id())) {
            // Set params for this body
            m_renderer->params() = m_simulation.getBodyAppearance(body->id());

            // Set planet position
            glm::dvec3 relativePos = body->position() - focusPos;
            glm::vec3 renderPos = m_simulation.physicsToRender(relativePos);
            m_renderer->setPlanetPosition(renderPos);

            // Calculate sun direction FROM this body TO the sun
            if (!body->isEmissive()) {
                glm::dvec3 toSun = sunPhysicsPos - body->position();
                glm::vec3 sunDir = glm::normalize(glm::vec3(toSun));
                sunDir.z = -sunDir.z;
                m_renderer->params().sunDirection = sunDir;
            }

            // Render this body
            m_renderer->render(*m_camera, body->isEmissive());

            // Render rings if this body has them
            if (m_renderer->hasRing(body->id())) {
                m_renderer->renderRing(body->id(), *m_camera, renderPos);
            }
        }
    }

    // Also render orbit trails
    m_simulation.renderOrbits(*m_renderer, *m_camera);

    m_renderer->endFrame();

    m_ui->beginFrame();

    // Render planet params UI for selected body (if any)
    auto* selected = m_simulation.selectedBody();
    if (selected && m_simulation.hasAppearance(selected->id())) {
        // Get or create ring params for this body
        RingParams* ringParams = m_simulation.getRingParams(selected->id());
        RingParams defaultRing;
        if (!ringParams) {
            m_simulation.setRingParams(selected->id(), defaultRing);
            ringParams = m_simulation.getRingParams(selected->id());
        }

        // Callback to regenerate ring when params change
        auto onRingChanged = [this, selected, ringParams]() {
            m_renderer->setupRing(selected->id(), *ringParams,
                                  selected->renderRadius(),
                                  static_cast<float>(selected->mass()));
        };

        m_ui->renderPlanetEditor(selected->name(),
                                 m_simulation.getBodyAppearance(selected->id()),
                                 ringParams, onRingChanged);
    }

    // Render simulation controls
    ImGui::SetNextWindowPos(ImVec2(10, 400), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(280, 200), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Simulation")) {
        auto stats = m_simulation.getStats();

        // Status
        ImGui::Text("Bodies: %d", stats.bodyCount);
        ImGui::Text("Focus: %s", stats.focusBodyName.c_str());

        // Time display
        double simTime = stats.simulationTime;
        double days = simTime / constants::DAY;
        double years = days / 365.25;
        if (years >= 1.0) {
            ImGui::Text("Sim Time: %.2f years", years);
        } else {
            ImGui::Text("Sim Time: %.1f days", days);
        }

        // Controls
        ImGui::Separator();
        if (ImGui::Button(stats.paused ? "Resume (Space)" : "Pause (Space)")) {
            m_simulation.togglePause();
        }

        // Time scale slider (logarithmic)
        float logScale = std::log10(static_cast<float>(stats.timeScale));
        if (ImGui::SliderFloat("Time Scale", &logScale, 0.0f, 8.0f, "10^%.1f")) {
            m_simulation.setTimeScale(std::pow(10.0, logScale));
        }
        ImGui::Text("%.0fx real time", stats.timeScale);

        // Body list - select & focus
        ImGui::Separator();
        ImGui::Text("Bodies (Right-click in view to select):");
        auto* currentSelected = m_simulation.selectedBody();
        auto* currentFocus = m_simulation.focusBody();
        for (const auto& body : m_simulation.world().bodies()) {
            bool isSelected = (currentSelected == body.get());
            bool isFocused = (currentFocus == body.get());

            std::string label = body->name();
            if (isFocused) label += " [F]";

            if (ImGui::Selectable(label.c_str(), isSelected)) {
                m_simulation.setSelectedBody(body->id());
            }
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                m_simulation.setFocusBody(body->id());
                // Update renderer params
                if (m_simulation.hasAppearance(body->id())) {
                    m_renderer->params() = m_simulation.getBodyAppearance(body->id());
                }
                // Animate camera to good view distance
                float viewDistance = body->renderRadius() * 4.0f;
                m_camera->transitionTo(glm::vec3(0.0f), 1.5f, viewDistance);
            }
        }
        ImGui::Text("(Double-click to focus camera)");

        // Physics stats
        if (ImGui::CollapsingHeader("Physics Stats")) {
            auto physStats = m_simulation.world().computeStats();
            ImGui::Text("Octree Nodes: %d", physStats.octreeNodes);
            ImGui::Text("Max Depth: %d", physStats.maxOctreeDepth);
            ImGui::Text("Total Mass: %.2e kg", physStats.totalMass);
            ImGui::Text("Kinetic E: %.2e J", physStats.totalKineticEnergy);
            ImGui::Text("Potential E: %.2e J", physStats.totalPotentialEnergy);
            ImGui::Text("Total E: %.2e J", physStats.totalEnergy);
        }
    }
    ImGui::End();

    // System presets UI
    int selectedPresetIndex = -1;
    if (m_ui->renderSystemPresets(m_presetManager, selectedPresetIndex)) {
        // A preset was selected - load it
        auto config = m_presetManager.loadPresetByIndex(static_cast<size_t>(selectedPresetIndex));
        if (config.has_value()) {
            m_simulation.loadFromConfig(config.value());
            // Re-setup ring renderers for any bodies with rings
            m_renderer->clearRings();
            for (const auto& body : m_simulation.world().bodies()) {
                if (m_simulation.hasRing(body->id())) {
                    auto* ringParams = m_simulation.getRingParams(body->id());
                    if (ringParams) {
                        m_renderer->setupRing(body->id(), *ringParams,
                                              body->renderRadius(), static_cast<float>(body->mass()));
                    }
                }
            }
            LOG_INFO("Loaded preset: {}", config->name);
        }
    }

    // Check if async exoplanet search completed
    if (m_exoSearching && m_exoSearchFuture.valid()) {
        auto status = m_exoSearchFuture.wait_for(std::chrono::milliseconds(0));
        if (status == std::future_status::ready) {
            m_exoSearchResults = m_exoSearchFuture.get();
            m_exoSearching = false;
            LOG_INFO("Exoplanet search completed: {} results", m_exoSearchResults.size());
        }
    }

    // Check if AI inference completed
    if (m_inferring && m_inferenceFuture.valid()) {
        auto status = m_inferenceFuture.wait_for(std::chrono::milliseconds(0));
        if (status == std::future_status::ready) {
            ExoplanetData enrichedExo = m_inferenceFuture.get();
            m_inferring = false;
            LOG_INFO("AI inference completed for {}", enrichedExo.name);

            // Now load the planet with AI-enriched data
            loadExoplanetIntoSimulation(enrichedExo);
        }
    }

    // AI inference settings UI
    m_ui->renderInferenceSettings(m_inferenceEngine.get());

    // Exoplanet search UI
    auto exoResult = m_ui->renderExoplanetSearch(m_exoSearchResults, m_exoSearching);
    if (exoResult.searchRequested && !m_exoSearching) {
        // Start async search
        m_exoSearchResults.clear();
        m_exoSearching = true;
        m_exoSearchFuture = m_dataAggregator->getNasaClient().queryByName(exoResult.searchQuery);
        LOG_INFO("Starting exoplanet search for: {}", exoResult.searchQuery);
    }
    if (exoResult.viewRequested && exoResult.selectedIndex >= 0 &&
        exoResult.selectedIndex < static_cast<int>(m_exoSearchResults.size()) &&
        !m_inferring) {
        // User wants to view an exoplanet - query ALL data sources
        auto exo = m_exoSearchResults[static_cast<size_t>(exoResult.selectedIndex)];
        LOG_INFO("Fetching data for exoplanet: {} from all sources...", exo.name);

        // Query Exoplanet.eu for molecules, albedo, measured temp (CRITICAL FOR COLOR!)
        LOG_DEBUG("Querying Exoplanet.eu for: {}", exo.name);
        auto euData = m_dataAggregator->getExoplanetEuClient().queryByNameSync(exo.name);
        if (euData.has_value()) {
            LOG_INFO("Exoplanet.eu: Found data for {}", euData->name);
            m_dataAggregator->getExoplanetEuClient().mergeIntoExoplanetData(exo, *euData);
            if (euData->molecules_detected.has_value()) {
                LOG_INFO("Exoplanet.eu: DETECTED MOLECULES: {}", *euData->molecules_detected);
            }
            if (euData->temp_measured_k.has_value()) {
                LOG_INFO("Exoplanet.eu: MEASURED TEMP: {:.0f} K", *euData->temp_measured_k);
            }
            if (euData->geometric_albedo.has_value()) {
                LOG_INFO("Exoplanet.eu: MEASURED ALBEDO: {:.3f}", *euData->geometric_albedo);
            }
        } else {
            LOG_WARN("Exoplanet.eu: No data found for {}", exo.name);
        }

        // Also query ExoMAST for additional atmospheric detections
        auto atmosData = m_dataAggregator->getExoMastClient().queryAtmosphereSync(exo.name);
        if (atmosData.has_value() && !atmosData->detections.empty()) {
            std::string molecules;
            for (const auto& det : atmosData->detections) {
                if (!molecules.empty()) molecules += ", ";
                molecules += det.molecule;
            }
            LOG_INFO("ExoMAST: Detected molecules: {}", molecules);
            m_currentAtmosphericDetections = atmosData->detections;
        } else {
            m_currentAtmosphericDetections.clear();
        }

        LOG_INFO("Starting AI inference for exoplanet: {}", exo.name);

        if (m_inferenceEngine->isAvailable()) {
            // Use AI to fill missing parameters
            m_pendingExoplanet = exo;
            m_inferring = true;
            m_inferenceFuture = m_inferenceEngine->fillMissingParameters(exo);
        } else {
            // No AI available, load directly with heuristics
            LOG_WARN("AI not available, using heuristics only");
            loadExoplanetIntoSimulation(exo);
        }
    }

    // Handle cached planet selection (instant load, no API calls needed)
    if (exoResult.cachedPlanetSelected && !exoResult.cachedPlanetName.empty()) {
        LOG_INFO("Loading cached planet: {}", exoResult.cachedPlanetName);

        auto cachedParams = ExoplanetConverter::loadCachedParams(exoResult.cachedPlanetName);
        if (cachedParams) {
            // Create a simple system with just the planet
            SystemConfig config;
            config.name = exoResult.cachedPlanetName + " (Cached)";
            config.description = "Pre-cached exoplanet render params";
            config.source = "cache";
            config.visualScale = 1.0;

            // Add a star for lighting
            float visualPlanetRadius = cachedParams->radius;
            float visualStarRadius = visualPlanetRadius * 8.0f;
            float starDistance = visualPlanetRadius * 40.0f;

            BodyConfig star;
            star.name = "Host Star";
            star.type = "Star";
            star.mass = constants::SOLAR_MASS;
            star.radius = visualStarRadius;
            star.position = {starDistance, starDistance * 0.3, -starDistance * 0.2};
            star.velocity = {0.0, 0.0, 0.0};
            star.rotationPeriod = 25.0 * constants::DAY;
            config.bodies.push_back(star);

            // Add the planet at origin
            BodyConfig planet;
            planet.name = exoResult.cachedPlanetName;
            planet.type = "Planet";
            planet.mass = constants::EARTH_MASS;
            planet.radius = visualPlanetRadius;
            planet.position = {0.0, 0.0, 0.0};
            planet.velocity = {0.0, 0.0, 0.0};
            planet.rotationPeriod = constants::DAY;
            config.bodies.push_back(planet);

            // Load the config
            m_simulation.loadFromConfig(config);

            // Find and configure the planet
            for (const auto& body : m_simulation.world().bodies()) {
                if (body->type() == BodyType::Planet) {
                    m_simulation.setBodyAppearance(body->id(), *cachedParams);
                    m_simulation.setSelectedBody(body->id());
                    m_simulation.setFocusBody(body->id());
                    m_renderer->params() = *cachedParams;
                    break;
                }
            }

            LOG_INFO("Loaded cached render params for {}", exoResult.cachedPlanetName);
        } else {
            LOG_WARN("Failed to load cached params for {}, cache file may be missing",
                     exoResult.cachedPlanetName);
        }
    }

    m_ui->endFrame();
}

void Application::shutdown() {
    if (m_blankCursor) {
        glfwDestroyCursor(m_blankCursor);
        m_blankCursor = nullptr;
    }
    m_ui.reset();
    m_renderer.reset();
    m_camera.reset();
    m_window.reset();
}

void Application::loadExoplanetIntoSimulation(const ExoplanetData& exo) {
    LOG_INFO("Loading exoplanet {} into simulation", exo.name);

    // Log AI-inferred values if present
    if (exo.biome_classification.isAIInferred()) {
        LOG_INFO("  AI biome: {}", exo.biome_classification.value);
    }
    if (exo.surface_color_hint.isAIInferred()) {
        LOG_INFO("  AI surface hint: {}", exo.surface_color_hint.value);
    }
    if (exo.atmosphere_composition.isAIInferred()) {
        LOG_INFO("  AI atmosphere: {}", exo.atmosphere_composition.value);
    }
    if (exo.ocean_coverage_fraction.isAIInferred()) {
        LOG_INFO("  AI ocean coverage: {:.1f}%", exo.ocean_coverage_fraction.value * 100);
    }

    // Convert exoplanet data to planet params using AI-generated render params
    PlanetParams params = ExoplanetConverter::toPlanetParams(exo, m_inferenceEngine.get());

    // Create a system with host star and planet
    SystemConfig config;
    config.name = exo.name + " System";
    config.description = "Exoplanet system from NASA Exoplanet Archive";
    config.source = "nasa_tap";
    config.visualScale = 1.0;

    // Get star temperature for lighting color
    double starTemp = 5778.0;  // Default solar temperature
    if (exo.host_star.effective_temp_k.hasValue()) {
        starTemp = exo.host_star.effective_temp_k.value;
    }

    // Visual sizes - planet radius from params, star is 8x larger visually
    float visualPlanetRadius = params.radius;
    float visualStarRadius = visualPlanetRadius * 8.0f;  // Star 8x planet size visually
    float starDistance = visualPlanetRadius * 40.0f;     // Star 40 planet-radii away

    // Create the host star with VISUAL radius (not physical)
    BodyConfig star;
    star.name = exo.host_star.name.empty() ? "Host Star" : exo.host_star.name;
    star.type = "Star";
    star.mass = constants::SOLAR_MASS;  // Mass doesn't matter for rendering
    star.radius = visualStarRadius;     // Use VISUAL radius, not physical!
    // Position star for good lighting angle
    star.position = {starDistance, starDistance * 0.3, -starDistance * 0.2};
    star.velocity = {0.0, 0.0, 0.0};
    star.rotationPeriod = 25.0 * constants::DAY;
    config.bodies.push_back(star);

    // Create the exoplanet at origin with VISUAL radius
    BodyConfig planet;
    planet.name = exo.name;
    planet.type = "Planet";
    planet.mass = exo.mass_earth.hasValue() ?
        exo.mass_earth.value * constants::EARTH_MASS : constants::EARTH_MASS;
    planet.radius = visualPlanetRadius;  // Use VISUAL radius
    planet.position = {0.0, 0.0, 0.0};
    planet.velocity = {0.0, 0.0, 0.0};
    planet.rotationPeriod = constants::DAY;
    config.bodies.push_back(planet);

    // Load the config into simulation
    m_simulation.loadFromConfig(config);

    // Find the star and planet bodies
    CelestialBody* starBody = nullptr;
    CelestialBody* planetBody = nullptr;
    for (const auto& body : m_simulation.world().bodies()) {
        if (body->type() == BodyType::Star) {
            starBody = body.get();
        } else if (body->type() == BodyType::Planet) {
            planetBody = body.get();
        }
    }

    if (planetBody) {
        m_simulation.setBodyAppearance(planetBody->id(), params);
        m_simulation.setSelectedBody(planetBody->id());
        m_simulation.setFocusBody(planetBody->id());

        // Update renderer params for the planet
        m_renderer->params() = params;

        // Set sun direction pointing from planet toward star
        if (starBody) {
            glm::vec3 toStar = glm::vec3(starBody->position() - planetBody->position());
            m_renderer->params().sunDirection = glm::normalize(toStar);
        }

        // Adjust sun color based on host star temperature
        if (starTemp < 3500) {
            // Red dwarf - warm reddish light
            m_renderer->params().sunColor = {1.0f, 0.65f, 0.4f};
            m_renderer->params().sunIntensity *= 0.8f;
        } else if (starTemp < 5000) {
            // Orange/K-type - warm light
            m_renderer->params().sunColor = {1.0f, 0.85f, 0.65f};
        } else if (starTemp < 6000) {
            // Sun-like - yellow-white
            m_renderer->params().sunColor = {1.0f, 0.98f, 0.9f};
        } else if (starTemp < 7500) {
            // F-type - white
            m_renderer->params().sunColor = {1.0f, 1.0f, 1.0f};
        } else {
            // Hot star - blue-white, brighter
            m_renderer->params().sunColor = {0.9f, 0.95f, 1.0f};
            m_renderer->params().sunIntensity *= 1.2f;
        }

        LOG_INFO("Host star {} at {}K, visual radius {:.1f}",
                 star.name, starTemp, visualStarRadius);

        // Position camera for good view of the planet
        float viewDistance = params.radius * 4.0f;
        m_camera->setPosition(glm::vec3(0.0f, 0.0f, viewDistance));
        m_camera->setTarget(glm::vec3(0.0f));
    }

    // Pause simulation since we're just viewing
    m_simulation.pause();
    m_renderer->clearRings();

    LOG_INFO("Now viewing exoplanet {} (type: {})",
             exo.name, ExoplanetConverter::inferPlanetType(exo));
}

}  // namespace astrocore
