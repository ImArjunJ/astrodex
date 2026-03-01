#include "core/Application.hpp"
#include "core/Logger.hpp"
#include "render/Camera.hpp"
#include "render/ThumbnailRenderer.hpp"
#include "intro/IntroAnimation.hpp"
#include "render/VulkanRenderer.hpp"
#include "data/SolarSystemDatabase.hpp"
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <chrono>
#include <filesystem>
#include <spdlog/fmt/fmt.h>
#include <set>
#include <algorithm>
#include <cctype>
#include <cstring>

// stb_image for loading cached PNG thumbnails (IMPLEMENTATION defined in VulkanRenderer.cpp)
#include "../../external/stb_image.h"

namespace astrocore {

// Pipeline stage display messages (indexed by PipelineStage enum)
static constexpr const char* kStageMessages[] = {
    "",                          // Idle
    "Querying catalogs...",      // QueryingSources (NASA + OEC + Gaia + CDS)
    "Inferring visuals...",      // InferringVisuals
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
    m_dataFusion   = std::make_unique<DataFusionEngine>();
    m_inference    = std::make_unique<InferenceEngine>();
    m_cacheManager = std::make_unique<CacheManager>();

    m_ui->setExoplanetCallback([this](const std::string& name) {
        loadPlanet(name);
    });

    // Seed autocomplete name list from solar system database + cache
    buildPlanetNameList();

    // ── Catalogue initialization ─────────────────────────────────────────
    m_catalogue = std::make_unique<CatalogueView>();
    m_catalogue->init();
    m_catalogue->setPlanetCallback([this](const std::string& name) {
        onCataloguePlanetClicked(name);
    });

    // Load cached records immediately for offline-first catalogue display
    {
        auto cachedNames = m_cacheManager->listCached();
        for (const auto& cn : cachedNames) {
            auto data = m_cacheManager->retrieve(cn);
            if (data.has_value() && !data->name.empty()) {
                m_catalogueData.push_back(std::move(*data));
            }
        }
        if (!m_catalogueData.empty()) {
            LOG_INFO("Catalogue: {} cached records loaded instantly", m_catalogueData.size());
        }
    }

    // Launch background prefetch of 500 notable exoplanets
    m_prefetchProgress = std::make_shared<std::atomic<int>>(0);
    m_prefetchFuture = m_dataFusion->prefetchNotable(500);
    m_prefetchComplete = false;
    m_catalogueMode = true;

    // ── Thumbnail renderer initialization ────────────────────────────────
    initThumbnailRenderer();
    loadThumbnailsFromCache();

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

    // ── Multi-source exoplanet pipeline (NASA + OEC + Gaia + CDS + AI) ────────
    m_planetLoading = true;
    m_ui->setLoading(true);
    m_ui->setExoplanetStatus("Searching...");
    m_pipelineStage.store(static_cast<int>(PipelineStage::QueryingSources));

    m_planetFuture = std::async(std::launch::async,
        [this, name]() -> LoadResult {

            // ── Step 1: Multi-source data fusion ────────────────────────────
            // Queries NASA, OEC, resolves host star via CDS/SIMBAD,
            // enriches with Gaia DR3 + VizieR, merges by priority,
            // then AI-fills missing physical fields.
            m_pipelineStage.store(static_cast<int>(PipelineStage::QueryingSources));
            auto data = m_dataFusion->fetchAndFuseSync(name);
            if (data.name.empty()) {
                m_pipelineStage.store(static_cast<int>(PipelineStage::Failed));
                return {std::nullopt, "Not found: \"" + name + "\"", std::nullopt};
            }

            // ── Step 2: Find closest solar-system analog for context ────────
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

            // ── Step 3: AI infers visual render parameters ──────────────────
            m_pipelineStage.store(static_cast<int>(PipelineStage::InferringVisuals));
            nlohmann::json aiJson;
            if (m_inference->isAvailable()) {
                aiJson = m_inference->inferRenderParamsSync(data, analogContext, physicsFields);
            }

            // ── Step 4: Physics base + AI → final PlanetParams ──────────────
            m_pipelineStage.store(static_cast<int>(PipelineStage::MappingParams));
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

void Application::onCataloguePlanetClicked(const std::string& name) {
    LOG_INFO("Catalogue: selected planet '{}'", name);
    loadPlanet(name);
    m_catalogueMode = false;
}

// ── Thumbnail Management ─────────────────────────────────────────────────────

std::string Application::makePlanetSlug(const std::string& name) {
    std::string slug;
    slug.reserve(name.size());
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            slug.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        } else if (c == ' ' || c == '_') {
            if (!slug.empty() && slug.back() != '-') slug.push_back('-');
        } else if (c == '-') {
            slug.push_back('-');
        }
    }
    // Trim trailing hyphens
    while (!slug.empty() && slug.back() == '-') slug.pop_back();
    return slug;
}

void Application::initThumbnailRenderer() {
    auto* vkr = static_cast<VulkanRenderer*>(m_renderer.get());

    VkDevice device       = static_cast<VkDevice>(vkr->getDevice());
    VmaAllocator allocator = static_cast<VmaAllocator>(vkr->getAllocator());
    VkQueue queue         = static_cast<VkQueue>(vkr->getGraphicsQueue());
    VkCommandPool cmdPool = static_cast<VkCommandPool>(vkr->getCommandPool());

    m_thumbnailRenderer = std::make_unique<ThumbnailRenderer>(
        device, allocator, queue, cmdPool, 128);

    // Create a fixed thumbnail camera (frames a unit sphere at distance 3)
    m_thumbnailCamera = std::make_unique<Camera>();
    m_thumbnailCamera->setPosition(glm::vec3(0.f, 0.f, 3.f));
    m_thumbnailCamera->setTarget(glm::vec3(0.f, 0.f, 0.f));

    LOG_INFO("ThumbnailRenderer initialized for catalogue previews");
}

void Application::loadThumbnailsFromCache() {
    const std::string cacheDir = ".cache/thumbnails";

    // Create cache directory if it doesn't exist
    std::filesystem::create_directories(cacheDir);

    if (!std::filesystem::exists(cacheDir)) return;

    int loaded = 0;
    for (const auto& entry : std::filesystem::directory_iterator(cacheDir)) {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        if (ext != ".png") continue;

        // Extract planet name from filename (slug format)
        std::string slug = entry.path().stem().string();
        std::string filepath = entry.path().string();

        ImTextureID texID = loadPNGAsTexture(filepath);
        if (texID != 0) {
            // We need to match the slug back to a planet name in catalogueData
            // For simplicity, store by slug and also try to match exact names
            m_catalogue->setThumbnail(slug, texID);

            // Also try to find the exact planet name and set that too
            for (const auto& planet : m_catalogueData) {
                if (makePlanetSlug(planet.name) == slug) {
                    m_catalogue->setThumbnail(planet.name, texID);
                    break;
                }
            }
            ++loaded;
        }
    }

    if (loaded > 0) {
        LOG_INFO("Loaded {} cached thumbnails from {}", loaded, cacheDir);
    }
}

ImTextureID Application::loadPNGAsTexture(const std::string& filepath) {
    // Load PNG via stb_image (already included via VulkanRenderer.cpp, but we
    // need the header here too). stb_image is included without IMPLEMENTATION
    // since VulkanRenderer.cpp already defines it.
    int w, h, ch;
    unsigned char* pixels = stbi_load(filepath.c_str(), &w, &h, &ch, 4);
    if (!pixels) {
        LOG_WARN("Failed to load thumbnail PNG: {}", filepath);
        return 0;
    }

    auto* vkr = static_cast<VulkanRenderer*>(m_renderer.get());
    VkDevice device       = static_cast<VkDevice>(vkr->getDevice());
    VmaAllocator allocator = static_cast<VmaAllocator>(vkr->getAllocator());
    VkQueue queue         = static_cast<VkQueue>(vkr->getGraphicsQueue());
    VkCommandPool cmdPool = static_cast<VkCommandPool>(vkr->getCommandPool());

    // Create VkImage for the texture
    VkImage image;
    VmaAllocation alloc;
    {
        VkImageCreateInfo imgCI{};
        imgCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgCI.imageType = VK_IMAGE_TYPE_2D;
        imgCI.format = VK_FORMAT_R8G8B8A8_UNORM;
        imgCI.extent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1};
        imgCI.mipLevels = 1;
        imgCI.arrayLayers = 1;
        imgCI.samples = VK_SAMPLE_COUNT_1_BIT;
        imgCI.tiling = VK_IMAGE_TILING_OPTIMAL;
        imgCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imgCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo allocCI{};
        allocCI.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        if (vmaCreateImage(allocator, &imgCI, &allocCI, &image, &alloc, nullptr) != VK_SUCCESS) {
            stbi_image_free(pixels);
            return 0;
        }
    }

    // Create staging buffer and upload pixels
    VkBuffer stagingBuf;
    VmaAllocation stagingAlloc;
    VkDeviceSize imageSize = static_cast<VkDeviceSize>(w) * h * 4;
    {
        VkBufferCreateInfo bufCI{};
        bufCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufCI.size = imageSize;
        bufCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        VmaAllocationCreateInfo allocCI{};
        allocCI.usage = VMA_MEMORY_USAGE_CPU_ONLY;

        if (vmaCreateBuffer(allocator, &bufCI, &allocCI, &stagingBuf, &stagingAlloc, nullptr) != VK_SUCCESS) {
            vmaDestroyImage(allocator, image, alloc);
            stbi_image_free(pixels);
            return 0;
        }

        void* mapped;
        vmaMapMemory(allocator, stagingAlloc, &mapped);
        std::memcpy(mapped, pixels, imageSize);
        vmaUnmapMemory(allocator, stagingAlloc);
    }

    stbi_image_free(pixels);

    // Upload: transition + copy + transition
    {
        VkCommandBuffer cmd;
        VkCommandBufferAllocateInfo cmdAI{};
        cmdAI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAI.commandPool = cmdPool;
        cmdAI.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAI.commandBufferCount = 1;
        vkAllocateCommandBuffers(device, &cmdAI, &cmd);

        VkCommandBufferBeginInfo beginI{};
        beginI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginI.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &beginI);

        // Transition: UNDEFINED -> TRANSFER_DST
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                             0, nullptr, 0, nullptr, 1, &barrier);

        // Copy buffer to image
        VkBufferImageCopy region{};
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageExtent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1};
        vkCmdCopyBufferToImage(cmd, stagingBuf, image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        // Transition: TRANSFER_DST -> SHADER_READ_ONLY
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                             0, nullptr, 0, nullptr, 1, &barrier);

        vkEndCommandBuffer(cmd);

        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;
        vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(queue);

        vkFreeCommandBuffers(device, cmdPool, 1, &cmd);
    }

    // Cleanup staging buffer
    vmaDestroyBuffer(allocator, stagingBuf, stagingAlloc);

    // Create image view
    VkImageView view;
    {
        VkImageViewCreateInfo viewCI{};
        viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewCI.image = image;
        viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewCI.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewCI.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCreateImageView(device, &viewCI, nullptr, &view);
    }

    // Create sampler
    VkSampler sampler;
    {
        VkSamplerCreateInfo sampCI{};
        sampCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampCI.magFilter = VK_FILTER_LINEAR;
        sampCI.minFilter = VK_FILTER_LINEAR;
        sampCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        vkCreateSampler(device, &sampCI, nullptr, &sampler);
    }

    // Register with ImGui
    VkDescriptorSet descSet = ImGui_ImplVulkan_AddTexture(
        sampler, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    ImTextureID texID = reinterpret_cast<ImTextureID>(descSet);

    return texID;
}

void Application::updateThumbnailGeneration(float deltaTime) {
    if (!m_thumbnailRenderer) return;
    if (!m_catalogueMode) return;  // Only generate when catalogue is visible

    // Initialize queue once catalogue data is populated
    if (!m_thumbnailQueueInitialized && !m_catalogueData.empty()) {
        m_thumbnailQueueInitialized = true;
        m_thumbnailQueue.clear();

        // Queue all planets for thumbnail generation
        for (int i = 0; i < static_cast<int>(m_catalogueData.size()); ++i) {
            // Skip if already cached on disk
            std::string slug = makePlanetSlug(m_catalogueData[i].name);
            std::string cachePath = ".cache/thumbnails/" + slug + ".png";
            if (!std::filesystem::exists(cachePath)) {
                m_thumbnailQueue.push_back(i);
            }
        }

        LOG_INFO("Thumbnail queue: {} planets to render", m_thumbnailQueue.size());
    }

    // Generate one thumbnail per frame (synchronous, very fast for placeholder renders)
    if (!m_thumbnailQueue.empty()) {
        int idx = m_thumbnailQueue.front();
        m_thumbnailQueue.pop_front();

        if (idx < 0 || idx >= static_cast<int>(m_catalogueData.size())) return;

        const auto& planet = m_catalogueData[idx];
        std::string slug = makePlanetSlug(planet.name);
        std::string cachePath = ".cache/thumbnails/" + slug + ".png";

        // Skip if already exists (may have been cached during this session)
        if (std::filesystem::exists(cachePath)) return;

        // Convert ExoplanetData to PlanetParams for rendering
        PlanetParams params;
        if (planet.mass_earth.hasValue() && planet.radius_earth.hasValue()) {
            params = ExoplanetMapper::toPlanetParams(planet);
        } else {
            // Fallback: find closest solar system analog
            if (planet.mass_earth.hasValue() && planet.radius_earth.hasValue() &&
                planet.equilibrium_temp_k.hasValue()) {
                auto analog = SolarSystemDatabase::instance().findClosestAnalog(
                    planet.mass_earth.value, planet.radius_earth.value,
                    planet.equilibrium_temp_k.value, 0.15f);
                if (analog && analog->entry) {
                    params = analog->entry->visualParams;
                } else {
                    params = ExoplanetMapper::toPlanetParams(planet);
                }
            } else {
                params = ExoplanetMapper::toPlanetParams(planet);
            }
        }

        // Render thumbnail
        ImTextureID texID = m_thumbnailRenderer->renderThumbnail(params, *m_thumbnailCamera);

        // Save to PNG cache
        m_thumbnailRenderer->saveToPNG(cachePath);

        // Set the thumbnail in CatalogueView
        m_catalogue->setThumbnail(planet.name, texID);
    }

    // ── Hover animation (slow rotation of hovered planet) ────────────────
    int hoveredIdx = m_catalogue->getHoveredCardIndex();
    std::string hoveredName = m_catalogue->getHoveredPlanetName();
    if (hoveredIdx >= 0 && hoveredIdx < static_cast<int>(m_catalogueData.size()) &&
        !hoveredName.empty()) {
        const auto& planet = m_catalogueData[hoveredIdx];

        // Get params for this planet
        PlanetParams params = ExoplanetMapper::toPlanetParams(planet);

        // Apply slow rotation based on accumulated time
        static float hoverRotation = 0.f;
        hoverRotation += deltaTime * 0.1f;  // ~10 seconds per full rotation
        params.rotationOffset = hoverRotation;

        // Re-render thumbnail with updated rotation (synchronous, fast)
        ImTextureID texID = m_thumbnailRenderer->renderThumbnail(params, *m_thumbnailCamera);

        // Update texture in catalogue (do NOT save animated frames to disk)
        m_catalogue->setThumbnail(planet.name, texID);
    }
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

    // ── Poll prefetch future for catalogue data ─────────────────────────
    if (!m_prefetchComplete && m_prefetchFuture.valid()) {
        if (m_prefetchFuture.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            auto results = m_prefetchFuture.get();
            // Merge prefetch results with existing cached data (avoid duplicates)
            std::set<std::string> existingNames;
            for (const auto& d : m_catalogueData) existingNames.insert(d.name);
            for (auto& r : results) {
                if (existingNames.find(r.name) == existingNames.end()) {
                    m_catalogueData.push_back(std::move(r));
                }
            }
            m_prefetchComplete = true;
            m_catalogue->setLoadingProgress(static_cast<int>(m_catalogueData.size()),
                                             static_cast<int>(m_catalogueData.size()));
            LOG_INFO("Catalogue: prefetch complete, {} total planets", m_catalogueData.size());
        } else {
            // Update loading progress estimate (based on time or counter)
            m_catalogue->setLoadingProgress(
                static_cast<int>(m_catalogueData.size()), 500);
        }
    }

    // ── Back button returns to catalogue ─────────────────────────────────
    if (!m_catalogueMode && m_ui->wasBackPressed()) {
        m_catalogueMode = true;
    }

    // ── Progressive thumbnail generation ─────────────────────────────────
    updateThumbnailGeneration(deltaTime);

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

    if (m_catalogueMode) {
        // Render catalogue UI (full-screen card grid)
        float W = static_cast<float>(m_window->getWidth());
        float H = static_cast<float>(m_window->getHeight());
        float dt = static_cast<float>(m_window->getTime() - m_lastFrameTime);
        m_catalogue->render(m_catalogueData, W, H, dt);
    } else {
        // Render planet detail UI (existing editor panel)
        m_ui->render(m_renderer->params());
    }

    // Theme toggle always visible
    m_ui->renderThemeToggle();

    m_ui->endFrame(static_cast<VulkanRenderer*>(m_renderer.get()));

    m_renderer->endFrame();
}

void Application::shutdown() {
    m_thumbnailRenderer.reset();
    m_thumbnailCamera.reset();
    m_catalogue.reset();
    m_ui.reset();
    m_renderer.reset();
    m_camera.reset();
    m_window.reset();
}

}  // namespace astrocore
