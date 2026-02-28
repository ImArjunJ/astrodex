#include "core/Application.hpp"
#include "core/Logger.hpp"
#include "render/Camera.hpp"
#include <imgui.h>
#include <chrono>

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

void Application::run() {
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
    m_planetLoading = true;

    LOG_INFO("Loading exoplanet: {}", name);
    m_ui->setExoplanetStatus("Querying NASA archive...");

    m_planetFuture = std::async(std::launch::async,
        [this, name]() -> LoadResult {

            // Step 1: NASA query
            auto results = m_nasa->queryByNameSync(name);
            if (results.empty()) {
                return {std::nullopt, "Not found: \"" + name + "\""};
            }

            auto data = results[0];
            data.calculateDerivedValues();

            // Step 2: AI fills missing atmosphere / physical fields
            if (m_inference->isAvailable()) {
                data = m_inference->fillMissingParametersSync(std::move(data));
            }

            // Step 3: AI generates numeric renderer overrides
            nlohmann::json aiJson;
            if (m_inference->isAvailable()) {
                aiJson = m_inference->inferRenderParamsSync(data);
            }

            // Step 4: Physics derivation → PlanetParams, then AI merge
            PlanetParams params = ExoplanetMapper::toRenderParams(data, aiJson);
            std::string category = ExoplanetMapper::categoryName(
                ExoplanetMapper::classify(data));

            return {params, data.name + "  |  " + category};
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
            m_ui->setExoplanetStatus(status);
            m_planetLoading = false;
            LOG_INFO("Planet loaded: {}", status);
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
