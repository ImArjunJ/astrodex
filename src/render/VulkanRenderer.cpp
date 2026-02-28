#include "render/VulkanRenderer.hpp"
#include "render/VulkanTypes.hpp"
#include "render/Camera.hpp"
#include "core/Logger.hpp"

#include <vulkan/vulkan.h>
#include <VkBootstrap.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "../../external/stb_image.h"

#include <fstream>
#include <vector>
#include <array>
#include <cstring>
#include <cstdint>
#include <stdexcept>

namespace astrocore {

// ── Helpers ──────────────────────────────────────────────────────────────────

#define VK_CHECK(x)                                                         \
    do {                                                                    \
        VkResult _r = (x);                                                  \
        if (_r != VK_SUCCESS) {                                             \
            LOG_ERROR("Vulkan error {} at {}:{}", int(_r), __FILE__, __LINE__); \
            throw std::runtime_error("Vulkan call failed");                 \
        }                                                                   \
    } while (0)

// Resolve path relative to executable, falling back to CWD
static std::string resolveAssetPath(const std::string& relative) {
#ifdef __APPLE__
    // Get executable directory via _NSGetExecutablePath
    char exePath[4096];
    uint32_t size = sizeof(exePath);
    if (_NSGetExecutablePath(exePath, &size) == 0) {
        std::string dir(exePath);
        auto pos = dir.find_last_of('/');
        if (pos != std::string::npos) {
            std::string candidate = dir.substr(0, pos + 1) + relative;
            if (std::ifstream(candidate).good()) return candidate;
        }
    }
#elif defined(__linux__)
    char exePath[4096];
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len > 0) {
        exePath[len] = '\0';
        std::string dir(exePath);
        auto pos = dir.find_last_of('/');
        if (pos != std::string::npos) {
            std::string candidate = dir.substr(0, pos + 1) + relative;
            if (std::ifstream(candidate).good()) return candidate;
        }
    }
#endif
    return relative; // fallback to CWD-relative
}

static std::vector<char> readFile(const std::string& path) {
    std::string resolved = resolveAssetPath(path);
    std::ifstream f(resolved, std::ios::ate | std::ios::binary);
    if (!f.is_open()) throw std::runtime_error("Failed to open file: " + path);
    size_t sz = f.tellg();
    std::vector<char> buf(sz);
    f.seekg(0);
    f.read(buf.data(), sz);
    return buf;
}

static VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size();
    ci.pCode    = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule mod;
    VK_CHECK(vkCreateShaderModule(device, &ci, nullptr, &mod));
    return mod;
}

// ── Pimpl ────────────────────────────────────────────────────────────────────

static constexpr uint32_t FRAMES_IN_FLIGHT = 2;

struct VulkanRenderer::Impl {
    // vk-bootstrap objects
    vkb::Instance    vkbInstance;
    vkb::Device      vkbDevice;
    vkb::Swapchain   vkbSwapchain;

    // Core handles
    VkInstance       instance       = VK_NULL_HANDLE;
    VkSurfaceKHR     surface        = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice         device         = VK_NULL_HANDLE;
    VkQueue          graphicsQueue  = VK_NULL_HANDLE;
    uint32_t         graphicsQueueFamily = 0;
    VkQueue          presentQueue   = VK_NULL_HANDLE;

    VmaAllocator     allocator      = VK_NULL_HANDLE;

    // Swapchain
    VkSwapchainKHR   swapchain      = VK_NULL_HANDLE;
    VkFormat         swapchainFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkExtent2D       swapchainExtent{};
    std::vector<VkImage>       swapchainImages;
    std::vector<VkImageView>   swapchainImageViews;
    std::vector<VkFramebuffer> framebuffers;

    // Render pass & pipeline
    VkRenderPass     renderPass     = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline       pipeline       = VK_NULL_HANDLE;

    // Descriptors
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool      descriptorPool      = VK_NULL_HANDLE;
    VkDescriptorPool      imguiDescriptorPool = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, FRAMES_IN_FLIGHT> descriptorSets{};

    // Per-frame sync
    uint32_t currentFrame = 0;
    std::array<VkCommandPool,   FRAMES_IN_FLIGHT> commandPools{};
    std::array<VkCommandBuffer, FRAMES_IN_FLIGHT> commandBuffers{};
    std::array<VkSemaphore,     FRAMES_IN_FLIGHT> imageAvailableSems{};
    std::array<VkSemaphore,     FRAMES_IN_FLIGHT> renderFinishedSems{};
    std::array<VkFence,         FRAMES_IN_FLIGHT> inFlightFences{};

    // Resources
    VkBuffer       vertexBuffer     = VK_NULL_HANDLE;
    VmaAllocation  vertexAlloc      = VK_NULL_HANDLE;

    std::array<VkBuffer,      FRAMES_IN_FLIGHT> uniformBuffers{};
    std::array<VmaAllocation, FRAMES_IN_FLIGHT> uniformAllocs{};
    std::array<void*,         FRAMES_IN_FLIGHT> uniformMapped{};

    VkImage       noiseImage     = VK_NULL_HANDLE;
    VmaAllocation noiseAlloc     = VK_NULL_HANDLE;
    VkImageView   noiseImageView = VK_NULL_HANDLE;
    VkSampler     noiseSampler   = VK_NULL_HANDLE;

    VkImage       starmapImage     = VK_NULL_HANDLE;
    VmaAllocation starmapAlloc     = VK_NULL_HANDLE;
    VkImageView   starmapImageView = VK_NULL_HANDLE;
    VkSampler     starmapSampler   = VK_NULL_HANDLE;

    // State
    PlanetParams params;
    float        time   = 0.0f;
    int          width  = 0;
    int          height = 0;
    uint32_t     currentImageIndex = 0;
    bool         framebufferResized = false;
    GLFWwindow*  window = nullptr;

    // Methods
    void createSwapchain(int w, int h);
    void createFramebuffers();
    void cleanupSwapchain();
    void recreateSwapchain();
};

// ── Swapchain management ─────────────────────────────────────────────────────

void VulkanRenderer::Impl::createSwapchain(int w, int h) {
    vkb::SwapchainBuilder builder{vkbDevice};
    auto result = builder
        .set_desired_format({VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(w, h)
        .set_old_swapchain(swapchain)
        .build();

    if (!result) throw std::runtime_error("Failed to create swapchain: " + result.error().message());

    if (swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, swapchain, nullptr);
    }

    vkbSwapchain    = result.value();
    swapchain       = vkbSwapchain.swapchain;
    swapchainFormat = vkbSwapchain.image_format;
    swapchainExtent = vkbSwapchain.extent;
    swapchainImages = vkbSwapchain.get_images().value();
    swapchainImageViews = vkbSwapchain.get_image_views().value();
}

void VulkanRenderer::Impl::createFramebuffers() {
    framebuffers.resize(swapchainImageViews.size());
    for (size_t i = 0; i < swapchainImageViews.size(); i++) {
        VkFramebufferCreateInfo ci{};
        ci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        ci.renderPass      = renderPass;
        ci.attachmentCount = 1;
        ci.pAttachments    = &swapchainImageViews[i];
        ci.width           = swapchainExtent.width;
        ci.height          = swapchainExtent.height;
        ci.layers          = 1;
        VK_CHECK(vkCreateFramebuffer(device, &ci, nullptr, &framebuffers[i]));
    }
}

void VulkanRenderer::Impl::cleanupSwapchain() {
    for (auto fb : framebuffers) vkDestroyFramebuffer(device, fb, nullptr);
    framebuffers.clear();
    for (auto iv : swapchainImageViews) vkDestroyImageView(device, iv, nullptr);
    swapchainImageViews.clear();
}

void VulkanRenderer::Impl::recreateSwapchain() {
    int w = 0, h = 0;
    glfwGetFramebufferSize(window, &w, &h);
    while (w == 0 || h == 0) {
        glfwGetFramebufferSize(window, &w, &h);
        glfwWaitEvents();
    }
    vkDeviceWaitIdle(device);
    cleanupSwapchain();
    createSwapchain(w, h);
    createFramebuffers();
    width  = w;
    height = h;
    framebufferResized = false;
}

// ── Constructor / Destructor ─────────────────────────────────────────────────

VulkanRenderer::VulkanRenderer()  : m_impl(std::make_unique<Impl>()) {}
VulkanRenderer::~VulkanRenderer() {
    if (!m_impl || m_impl->device == VK_NULL_HANDLE) return;
    vkDeviceWaitIdle(m_impl->device);

    // Sync objects
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        vkDestroyFence(m_impl->device,     m_impl->inFlightFences[i],    nullptr);
        vkDestroySemaphore(m_impl->device,  m_impl->renderFinishedSems[i], nullptr);
        vkDestroySemaphore(m_impl->device,  m_impl->imageAvailableSems[i], nullptr);
        vkDestroyCommandPool(m_impl->device, m_impl->commandPools[i],     nullptr);
    }

    // Descriptor pools
    vkDestroyDescriptorPool(m_impl->device, m_impl->descriptorPool,      nullptr);
    vkDestroyDescriptorPool(m_impl->device, m_impl->imguiDescriptorPool, nullptr);

    // Pipeline
    vkDestroyPipeline(m_impl->device,       m_impl->pipeline,       nullptr);
    vkDestroyPipelineLayout(m_impl->device,  m_impl->pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_impl->device, m_impl->descriptorSetLayout, nullptr);
    vkDestroyRenderPass(m_impl->device,      m_impl->renderPass,    nullptr);

    // Resources
    vkDestroySampler(m_impl->device, m_impl->starmapSampler,   nullptr);
    vkDestroyImageView(m_impl->device, m_impl->starmapImageView, nullptr);
    vmaDestroyImage(m_impl->allocator, m_impl->starmapImage, m_impl->starmapAlloc);

    vkDestroySampler(m_impl->device, m_impl->noiseSampler,   nullptr);
    vkDestroyImageView(m_impl->device, m_impl->noiseImageView, nullptr);
    vmaDestroyImage(m_impl->allocator, m_impl->noiseImage, m_impl->noiseAlloc);

    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        vmaDestroyBuffer(m_impl->allocator, m_impl->uniformBuffers[i], m_impl->uniformAllocs[i]);
    }
    vmaDestroyBuffer(m_impl->allocator, m_impl->vertexBuffer, m_impl->vertexAlloc);

    // Swapchain
    m_impl->cleanupSwapchain();
    vkDestroySwapchainKHR(m_impl->device, m_impl->swapchain, nullptr);

    // Core
    vmaDestroyAllocator(m_impl->allocator);
    vkDestroySurfaceKHR(m_impl->instance, m_impl->surface, nullptr);
    vkb::destroy_device(m_impl->vkbDevice);
    vkb::destroy_instance(m_impl->vkbInstance);
}

// ── Initialization ───────────────────────────────────────────────────────────

void VulkanRenderer::init(int width, int height, void* glfwWindow) {
    m_impl->window = static_cast<GLFWwindow*>(glfwWindow);
    m_impl->width  = width;
    m_impl->height = height;

    // 0. On macOS, ensure the Vulkan loader can find MoltenVK's ICD
#ifdef __APPLE__
    {
        const char* icdPaths[] = {
            "/opt/homebrew/etc/vulkan/icd.d/MoltenVK_icd.json",
            "/usr/local/etc/vulkan/icd.d/MoltenVK_icd.json",
        };
        if (!getenv("VK_ICD_FILENAMES") && !getenv("VK_DRIVER_FILES")) {
            for (const char* path : icdPaths) {
                std::ifstream test(path);
                if (test.good()) {
                    setenv("VK_ICD_FILENAMES", path, 0);
                    LOG_INFO("Auto-detected MoltenVK ICD: {}", path);
                    break;
                }
            }
        }
    }
#endif

    // 1. Instance
    // Pass the statically-linked vkGetInstanceProcAddr so vk-bootstrap
    // doesn't try dlopen("libvulkan.dylib") which may fail on Homebrew paths.
    vkb::InstanceBuilder instBuilder(vkGetInstanceProcAddr);
    instBuilder.set_app_name("AstroSplat")
               .require_api_version(1, 2, 0)
               .set_engine_name("AstroCore");
#ifndef NDEBUG
    instBuilder.request_validation_layers()
               .use_default_debug_messenger();
#endif
    // MoltenVK portability
    instBuilder.enable_extension(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);

    auto instResult = instBuilder.build();
    if (!instResult) throw std::runtime_error("Failed to create Vulkan instance: " + instResult.error().message());
    m_impl->vkbInstance = instResult.value();
    m_impl->instance    = m_impl->vkbInstance.instance;
    LOG_INFO("Vulkan instance created");

    // 2. Surface
    VK_CHECK(glfwCreateWindowSurface(m_impl->instance, m_impl->window, nullptr, &m_impl->surface));

    // 3. Physical + logical device
    vkb::PhysicalDeviceSelector selector{m_impl->vkbInstance};
    selector.set_surface(m_impl->surface)
            .set_minimum_version(1, 2)
            .add_required_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    // MoltenVK portability subset
#ifdef __APPLE__
    selector.add_required_extension("VK_KHR_portability_subset");
#endif

    auto physResult = selector.select();
    if (!physResult) throw std::runtime_error("Failed to select physical device: " + physResult.error().message());

    vkb::DeviceBuilder devBuilder{physResult.value()};
    auto devResult = devBuilder.build();
    if (!devResult) throw std::runtime_error("Failed to create logical device: " + devResult.error().message());

    m_impl->vkbDevice      = devResult.value();
    m_impl->device         = m_impl->vkbDevice.device;
    m_impl->physicalDevice = physResult.value().physical_device;

    auto gq = m_impl->vkbDevice.get_queue(vkb::QueueType::graphics);
    auto gqi = m_impl->vkbDevice.get_queue_index(vkb::QueueType::graphics);
    m_impl->graphicsQueue       = gq.value();
    m_impl->graphicsQueueFamily = gqi.value();
    m_impl->presentQueue        = m_impl->vkbDevice.get_queue(vkb::QueueType::present).value();

    LOG_INFO("Vulkan device: {}", physResult.value().properties.deviceName);

    // 4. VMA
    VmaAllocatorCreateInfo allocCI{};
    allocCI.physicalDevice   = m_impl->physicalDevice;
    allocCI.device           = m_impl->device;
    allocCI.instance         = m_impl->instance;
    allocCI.vulkanApiVersion = VK_API_VERSION_1_2;
    VK_CHECK(vmaCreateAllocator(&allocCI, &m_impl->allocator));

    // 5. Swapchain
    m_impl->createSwapchain(width, height);

    // 6. Render pass
    {
        VkAttachmentDescription colorAtt{};
        colorAtt.format         = m_impl->swapchainFormat;
        colorAtt.samples        = VK_SAMPLE_COUNT_1_BIT;
        colorAtt.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAtt.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        colorAtt.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAtt.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAtt.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorRef{};
        colorRef.attachment = 0;
        colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments    = &colorRef;

        VkSubpassDependency dep{};
        dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass    = 0;
        dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.srcAccessMask = 0;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo rpCI{};
        rpCI.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpCI.attachmentCount = 1;
        rpCI.pAttachments    = &colorAtt;
        rpCI.subpassCount    = 1;
        rpCI.pSubpasses      = &subpass;
        rpCI.dependencyCount = 1;
        rpCI.pDependencies   = &dep;
        VK_CHECK(vkCreateRenderPass(m_impl->device, &rpCI, nullptr, &m_impl->renderPass));
    }

    // 7. Framebuffers
    m_impl->createFramebuffers();

    // 8. Descriptor set layout
    {
        VkDescriptorSetLayoutBinding bindings[3]{};
        bindings[0].binding         = 0;
        bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

        bindings[1].binding         = 1;
        bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

        bindings[2].binding         = 2;
        bindings[2].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[2].descriptorCount = 1;
        bindings[2].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo ci{};
        ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        ci.bindingCount = 3;
        ci.pBindings    = bindings;
        VK_CHECK(vkCreateDescriptorSetLayout(m_impl->device, &ci, nullptr, &m_impl->descriptorSetLayout));
    }

    // 9. Pipeline layout
    {
        VkPipelineLayoutCreateInfo ci{};
        ci.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        ci.setLayoutCount = 1;
        ci.pSetLayouts    = &m_impl->descriptorSetLayout;
        VK_CHECK(vkCreatePipelineLayout(m_impl->device, &ci, nullptr, &m_impl->pipelineLayout));
    }

    // 10. Graphics pipeline
    {
        auto vertCode = readFile("shaders/planet_vk.vert.spv");
        auto fragCode = readFile("shaders/planet_vk.frag.spv");
        VkShaderModule vertMod = createShaderModule(m_impl->device, vertCode);
        VkShaderModule fragMod = createShaderModule(m_impl->device, fragCode);

        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertMod;
        stages[0].pName  = "main";
        stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragMod;
        stages[1].pName  = "main";

        VkVertexInputBindingDescription vertBind{};
        vertBind.binding   = 0;
        vertBind.stride    = 16; // 4 floats
        vertBind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription vertAttrs[2]{};
        vertAttrs[0].location = 0;
        vertAttrs[0].binding  = 0;
        vertAttrs[0].format   = VK_FORMAT_R32G32_SFLOAT;
        vertAttrs[0].offset   = 0;
        vertAttrs[1].location = 1;
        vertAttrs[1].binding  = 0;
        vertAttrs[1].format   = VK_FORMAT_R32G32_SFLOAT;
        vertAttrs[1].offset   = 8;

        VkPipelineVertexInputStateCreateInfo vertInput{};
        vertInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertInput.vertexBindingDescriptionCount   = 1;
        vertInput.pVertexBindingDescriptions      = &vertBind;
        vertInput.vertexAttributeDescriptionCount = 2;
        vertInput.pVertexAttributeDescriptions    = vertAttrs;

        VkPipelineInputAssemblyStateCreateInfo inputAsm{};
        inputAsm.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAsm.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo vpState{};
        vpState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vpState.viewportCount = 1;
        vpState.scissorCount  = 1;

        VkPipelineRasterizationStateCreateInfo raster{};
        raster.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode    = VK_CULL_MODE_NONE;
        raster.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        raster.lineWidth   = 1.0f;

        VkPipelineMultisampleStateCreateInfo msaa{};
        msaa.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState blend{};
        blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo blendState{};
        blendState.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blendState.attachmentCount = 1;
        blendState.pAttachments    = &blend;

        VkDynamicState dynStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynState{};
        dynState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynState.dynamicStateCount = 2;
        dynState.pDynamicStates    = dynStates;

        VkGraphicsPipelineCreateInfo pipeCI{};
        pipeCI.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeCI.stageCount          = 2;
        pipeCI.pStages             = stages;
        pipeCI.pVertexInputState   = &vertInput;
        pipeCI.pInputAssemblyState = &inputAsm;
        pipeCI.pViewportState      = &vpState;
        pipeCI.pRasterizationState = &raster;
        pipeCI.pMultisampleState   = &msaa;
        pipeCI.pColorBlendState    = &blendState;
        pipeCI.pDynamicState       = &dynState;
        pipeCI.layout              = m_impl->pipelineLayout;
        pipeCI.renderPass          = m_impl->renderPass;
        pipeCI.subpass             = 0;

        VK_CHECK(vkCreateGraphicsPipelines(m_impl->device, VK_NULL_HANDLE, 1, &pipeCI, nullptr, &m_impl->pipeline));

        vkDestroyShaderModule(m_impl->device, fragMod, nullptr);
        vkDestroyShaderModule(m_impl->device, vertMod, nullptr);
    }

    // 11. Vertex buffer (fullscreen quad)
    {
        float verts[] = {
            -1.f, -1.f, 0.f, 0.f,
             1.f, -1.f, 1.f, 0.f,
             1.f,  1.f, 1.f, 1.f,
            -1.f, -1.f, 0.f, 0.f,
             1.f,  1.f, 1.f, 1.f,
            -1.f,  1.f, 0.f, 1.f,
        };

        VkBufferCreateInfo bufCI{};
        bufCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufCI.size  = sizeof(verts);
        bufCI.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VmaAllocationCreateInfo allocCI{};
        allocCI.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VK_CHECK(vmaCreateBuffer(m_impl->allocator, &bufCI, &allocCI,
                                 &m_impl->vertexBuffer, &m_impl->vertexAlloc, nullptr));

        // Staging
        VkBuffer staging; VmaAllocation stagingAlloc;
        VkBufferCreateInfo stageBufCI{};
        stageBufCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        stageBufCI.size  = sizeof(verts);
        stageBufCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        VmaAllocationCreateInfo stageAllocCI{};
        stageAllocCI.usage = VMA_MEMORY_USAGE_CPU_ONLY;
        VK_CHECK(vmaCreateBuffer(m_impl->allocator, &stageBufCI, &stageAllocCI, &staging, &stagingAlloc, nullptr));

        void* mapped;
        vmaMapMemory(m_impl->allocator, stagingAlloc, &mapped);
        std::memcpy(mapped, verts, sizeof(verts));
        vmaUnmapMemory(m_impl->allocator, stagingAlloc);

        // One-shot command buffer for transfer
        VkCommandPool tmpPool;
        VkCommandPoolCreateInfo poolCI{};
        poolCI.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolCI.queueFamilyIndex = m_impl->graphicsQueueFamily;
        poolCI.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        VK_CHECK(vkCreateCommandPool(m_impl->device, &poolCI, nullptr, &tmpPool));

        VkCommandBuffer cmd;
        VkCommandBufferAllocateInfo cmdAI{};
        cmdAI.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAI.commandPool        = tmpPool;
        cmdAI.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAI.commandBufferCount = 1;
        VK_CHECK(vkAllocateCommandBuffers(m_impl->device, &cmdAI, &cmd));

        VkCommandBufferBeginInfo beginI{};
        beginI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginI.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &beginI);
        VkBufferCopy region{0, 0, sizeof(verts)};
        vkCmdCopyBuffer(cmd, staging, m_impl->vertexBuffer, 1, &region);
        vkEndCommandBuffer(cmd);

        VkSubmitInfo submit{};
        submit.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers    = &cmd;
        vkQueueSubmit(m_impl->graphicsQueue, 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(m_impl->graphicsQueue);

        vkDestroyCommandPool(m_impl->device, tmpPool, nullptr);
        vmaDestroyBuffer(m_impl->allocator, staging, stagingAlloc);
    }

    // 12. Uniform buffers (per frame, host-visible, persistently mapped)
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        VkBufferCreateInfo bufCI{};
        bufCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufCI.size  = sizeof(PlanetUniformsVk);
        bufCI.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

        VmaAllocationCreateInfo allocCI{};
        allocCI.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        allocCI.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo allocInfo{};
        VK_CHECK(vmaCreateBuffer(m_impl->allocator, &bufCI, &allocCI,
                                 &m_impl->uniformBuffers[i], &m_impl->uniformAllocs[i], &allocInfo));
        m_impl->uniformMapped[i] = allocInfo.pMappedData;
    }

    // 13. 3D noise texture (512^3 R8)
    {
        constexpr int sz = 512;
        size_t totalSize = size_t(sz) * sz * sz;
        std::vector<uint8_t> data(totalSize);
        uint32_t seed = 0x12345678;
        for (size_t i = 0; i < totalSize; ++i) {
            seed = seed * 1664525u + 1013904223u;
            data[i] = static_cast<uint8_t>(seed >> 24);
        }

        // Create image
        VkImageCreateInfo imgCI{};
        imgCI.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgCI.imageType     = VK_IMAGE_TYPE_3D;
        imgCI.format        = VK_FORMAT_R8_UNORM;
        imgCI.extent        = {uint32_t(sz), uint32_t(sz), uint32_t(sz)};
        imgCI.mipLevels     = 1;
        imgCI.arrayLayers   = 1;
        imgCI.samples       = VK_SAMPLE_COUNT_1_BIT;
        imgCI.tiling        = VK_IMAGE_TILING_OPTIMAL;
        imgCI.usage         = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imgCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo imgAllocCI{};
        imgAllocCI.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        VK_CHECK(vmaCreateImage(m_impl->allocator, &imgCI, &imgAllocCI,
                                &m_impl->noiseImage, &m_impl->noiseAlloc, nullptr));

        // Staging buffer
        VkBuffer staging; VmaAllocation stagingAlloc;
        VkBufferCreateInfo stageBufCI{};
        stageBufCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        stageBufCI.size  = totalSize;
        stageBufCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        VmaAllocationCreateInfo stageAllocCI{};
        stageAllocCI.usage = VMA_MEMORY_USAGE_CPU_ONLY;
        VK_CHECK(vmaCreateBuffer(m_impl->allocator, &stageBufCI, &stageAllocCI, &staging, &stagingAlloc, nullptr));
        void* mapped;
        vmaMapMemory(m_impl->allocator, stagingAlloc, &mapped);
        std::memcpy(mapped, data.data(), totalSize);
        vmaUnmapMemory(m_impl->allocator, stagingAlloc);

        // Upload via one-shot command buffer
        VkCommandPool tmpPool;
        VkCommandPoolCreateInfo poolCI{};
        poolCI.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolCI.queueFamilyIndex = m_impl->graphicsQueueFamily;
        poolCI.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        VK_CHECK(vkCreateCommandPool(m_impl->device, &poolCI, nullptr, &tmpPool));

        VkCommandBuffer cmd;
        VkCommandBufferAllocateInfo cmdAI{};
        cmdAI.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAI.commandPool        = tmpPool;
        cmdAI.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAI.commandBufferCount = 1;
        VK_CHECK(vkAllocateCommandBuffers(m_impl->device, &cmdAI, &cmd));

        VkCommandBufferBeginInfo beginI{};
        beginI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginI.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &beginI);

        // Transition UNDEFINED → TRANSFER_DST
        VkImageMemoryBarrier barrier{};
        barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcAccessMask       = 0;
        barrier.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.image               = m_impl->noiseImage;
        barrier.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);

        VkBufferImageCopy copyRegion{};
        copyRegion.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copyRegion.imageExtent      = {uint32_t(sz), uint32_t(sz), uint32_t(sz)};
        vkCmdCopyBufferToImage(cmd, staging, m_impl->noiseImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        // Transition TRANSFER_DST → SHADER_READ
        barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);

        vkEndCommandBuffer(cmd);
        VkSubmitInfo submit{};
        submit.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers    = &cmd;
        vkQueueSubmit(m_impl->graphicsQueue, 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(m_impl->graphicsQueue);

        vkDestroyCommandPool(m_impl->device, tmpPool, nullptr);
        vmaDestroyBuffer(m_impl->allocator, staging, stagingAlloc);

        // Image view
        VkImageViewCreateInfo viewCI{};
        viewCI.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewCI.image            = m_impl->noiseImage;
        viewCI.viewType         = VK_IMAGE_VIEW_TYPE_3D;
        viewCI.format           = VK_FORMAT_R8_UNORM;
        viewCI.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VK_CHECK(vkCreateImageView(m_impl->device, &viewCI, nullptr, &m_impl->noiseImageView));

        // Sampler
        VkSamplerCreateInfo sampCI{};
        sampCI.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampCI.magFilter    = VK_FILTER_LINEAR;
        sampCI.minFilter    = VK_FILTER_LINEAR;
        sampCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        VK_CHECK(vkCreateSampler(m_impl->device, &sampCI, nullptr, &m_impl->noiseSampler));
    }

    // 13b. Star cubemap texture (6 faces loaded from PNG)
    {
        const char* faceFiles[] = {
            "assets/starmap/starmap_posX.png",
            "assets/starmap/starmap_negX.png",
            "assets/starmap/starmap_posY.png",
            "assets/starmap/starmap_negY.png",
            "assets/starmap/starmap_posZ.png",
            "assets/starmap/starmap_negZ.png",
        };

        int faceW = 0, faceH = 0, faceChannels = 0;
        stbi_uc* facePixels[6]{};

        for (int f = 0; f < 6; f++) {
            std::string path = resolveAssetPath(faceFiles[f]);
            facePixels[f] = stbi_load(path.c_str(), &faceW, &faceH, &faceChannels, 4);
            if (!facePixels[f]) {
                LOG_WARN("Failed to load starmap face: {} — using black", faceFiles[f]);
                // Create a 1x1 black fallback
                if (f == 0) { faceW = 1; faceH = 1; }
                facePixels[f] = static_cast<stbi_uc*>(calloc(faceW * faceH * 4, 1));
            }
        }

        uint32_t fw = uint32_t(faceW), fh = uint32_t(faceH);
        size_t faceBytes = size_t(faceW) * faceH * 4;

        // Create cubemap image
        VkImageCreateInfo imgCI{};
        imgCI.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgCI.flags         = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        imgCI.imageType     = VK_IMAGE_TYPE_2D;
        imgCI.format        = VK_FORMAT_R8G8B8A8_UNORM;
        imgCI.extent        = {fw, fh, 1};
        imgCI.mipLevels     = 1;
        imgCI.arrayLayers   = 6;
        imgCI.samples       = VK_SAMPLE_COUNT_1_BIT;
        imgCI.tiling        = VK_IMAGE_TILING_OPTIMAL;
        imgCI.usage         = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imgCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo imgAllocCI{};
        imgAllocCI.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        VK_CHECK(vmaCreateImage(m_impl->allocator, &imgCI, &imgAllocCI,
                                &m_impl->starmapImage, &m_impl->starmapAlloc, nullptr));

        // Staging buffer for all 6 faces
        VkBuffer staging; VmaAllocation stagingAlloc;
        VkBufferCreateInfo stageBufCI{};
        stageBufCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        stageBufCI.size  = faceBytes * 6;
        stageBufCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        VmaAllocationCreateInfo stageAllocCI{};
        stageAllocCI.usage = VMA_MEMORY_USAGE_CPU_ONLY;
        VK_CHECK(vmaCreateBuffer(m_impl->allocator, &stageBufCI, &stageAllocCI, &staging, &stagingAlloc, nullptr));

        void* mapped;
        vmaMapMemory(m_impl->allocator, stagingAlloc, &mapped);
        for (int f = 0; f < 6; f++) {
            memcpy(static_cast<char*>(mapped) + f * faceBytes, facePixels[f], faceBytes);
            stbi_image_free(facePixels[f]);
        }
        vmaUnmapMemory(m_impl->allocator, stagingAlloc);

        // Upload via one-shot command buffer
        VkCommandPool tmpPool;
        VkCommandPoolCreateInfo poolCI{};
        poolCI.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolCI.queueFamilyIndex = m_impl->graphicsQueueFamily;
        poolCI.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        VK_CHECK(vkCreateCommandPool(m_impl->device, &poolCI, nullptr, &tmpPool));

        VkCommandBuffer cmd;
        VkCommandBufferAllocateInfo cmdAI{};
        cmdAI.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAI.commandPool        = tmpPool;
        cmdAI.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAI.commandBufferCount = 1;
        VK_CHECK(vkAllocateCommandBuffers(m_impl->device, &cmdAI, &cmd));

        VkCommandBufferBeginInfo beginI{};
        beginI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginI.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &beginI);

        // Transition all 6 layers UNDEFINED → TRANSFER_DST
        VkImageMemoryBarrier barrier{};
        barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcAccessMask       = 0;
        barrier.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.image               = m_impl->starmapImage;
        barrier.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6};
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);

        // Copy each face from staging buffer
        std::array<VkBufferImageCopy, 6> regions{};
        for (uint32_t f = 0; f < 6; f++) {
            regions[f].bufferOffset      = f * faceBytes;
            regions[f].imageSubresource  = {VK_IMAGE_ASPECT_COLOR_BIT, 0, f, 1};
            regions[f].imageExtent       = {fw, fh, 1};
        }
        vkCmdCopyBufferToImage(cmd, staging, m_impl->starmapImage,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6, regions.data());

        // Transition TRANSFER_DST → SHADER_READ
        barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);

        vkEndCommandBuffer(cmd);
        VkSubmitInfo submit{};
        submit.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers    = &cmd;
        vkQueueSubmit(m_impl->graphicsQueue, 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(m_impl->graphicsQueue);

        vkDestroyCommandPool(m_impl->device, tmpPool, nullptr);
        vmaDestroyBuffer(m_impl->allocator, staging, stagingAlloc);

        // Image view (cubemap)
        VkImageViewCreateInfo viewCI{};
        viewCI.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewCI.image            = m_impl->starmapImage;
        viewCI.viewType         = VK_IMAGE_VIEW_TYPE_CUBE;
        viewCI.format           = VK_FORMAT_R8G8B8A8_UNORM;
        viewCI.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6};
        VK_CHECK(vkCreateImageView(m_impl->device, &viewCI, nullptr, &m_impl->starmapImageView));

        // Sampler — nearest filtering to keep stars as sharp points
        VkSamplerCreateInfo sampCI{};
        sampCI.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampCI.magFilter    = VK_FILTER_NEAREST;
        sampCI.minFilter    = VK_FILTER_NEAREST;
        sampCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        VK_CHECK(vkCreateSampler(m_impl->device, &sampCI, nullptr, &m_impl->starmapSampler));

        LOG_INFO("Star cubemap loaded ({}x{}, 6 faces)", faceW, faceH);
    }

    // 14. Descriptor pool + sets
    {
        VkDescriptorPoolSize poolSizes[] = {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         FRAMES_IN_FLIGHT},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,  FRAMES_IN_FLIGHT * 2}, // noise + starmap
        };
        VkDescriptorPoolCreateInfo poolCI{};
        poolCI.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolCI.maxSets       = FRAMES_IN_FLIGHT;
        poolCI.poolSizeCount = 2;
        poolCI.pPoolSizes    = poolSizes;
        VK_CHECK(vkCreateDescriptorPool(m_impl->device, &poolCI, nullptr, &m_impl->descriptorPool));

        VkDescriptorSetLayout layouts[FRAMES_IN_FLIGHT] = {m_impl->descriptorSetLayout, m_impl->descriptorSetLayout};
        VkDescriptorSetAllocateInfo allocI{};
        allocI.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocI.descriptorPool     = m_impl->descriptorPool;
        allocI.descriptorSetCount = FRAMES_IN_FLIGHT;
        allocI.pSetLayouts        = layouts;
        VK_CHECK(vkAllocateDescriptorSets(m_impl->device, &allocI, m_impl->descriptorSets.data()));

        for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
            VkDescriptorBufferInfo bufInfo{};
            bufInfo.buffer = m_impl->uniformBuffers[i];
            bufInfo.offset = 0;
            bufInfo.range  = sizeof(PlanetUniformsVk);

            VkDescriptorImageInfo noiseInfo{};
            noiseInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            noiseInfo.imageView   = m_impl->noiseImageView;
            noiseInfo.sampler     = m_impl->noiseSampler;

            VkDescriptorImageInfo starmapInfo{};
            starmapInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            starmapInfo.imageView   = m_impl->starmapImageView;
            starmapInfo.sampler     = m_impl->starmapSampler;

            VkWriteDescriptorSet writes[3]{};
            writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet          = m_impl->descriptorSets[i];
            writes[0].dstBinding      = 0;
            writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[0].descriptorCount = 1;
            writes[0].pBufferInfo     = &bufInfo;

            writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].dstSet          = m_impl->descriptorSets[i];
            writes[1].dstBinding      = 1;
            writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[1].descriptorCount = 1;
            writes[1].pImageInfo      = &noiseInfo;

            writes[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[2].dstSet          = m_impl->descriptorSets[i];
            writes[2].dstBinding      = 2;
            writes[2].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[2].descriptorCount = 1;
            writes[2].pImageInfo      = &starmapInfo;

            vkUpdateDescriptorSets(m_impl->device, 3, writes, 0, nullptr);
        }
    }

    // 15. ImGui descriptor pool (separate, with free flag for ImGui)
    {
        VkDescriptorPoolSize poolSizes[] = {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10},
        };
        VkDescriptorPoolCreateInfo poolCI{};
        poolCI.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolCI.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolCI.maxSets       = 10;
        poolCI.poolSizeCount = 1;
        poolCI.pPoolSizes    = poolSizes;
        VK_CHECK(vkCreateDescriptorPool(m_impl->device, &poolCI, nullptr, &m_impl->imguiDescriptorPool));
    }

    // 16. Per-frame command pools, command buffers, sync objects
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        VkCommandPoolCreateInfo poolCI{};
        poolCI.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolCI.queueFamilyIndex = m_impl->graphicsQueueFamily;
        poolCI.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        VK_CHECK(vkCreateCommandPool(m_impl->device, &poolCI, nullptr, &m_impl->commandPools[i]));

        VkCommandBufferAllocateInfo cmdAI{};
        cmdAI.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAI.commandPool        = m_impl->commandPools[i];
        cmdAI.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAI.commandBufferCount = 1;
        VK_CHECK(vkAllocateCommandBuffers(m_impl->device, &cmdAI, &m_impl->commandBuffers[i]));

        VkSemaphoreCreateInfo semCI{};
        semCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VK_CHECK(vkCreateSemaphore(m_impl->device, &semCI, nullptr, &m_impl->imageAvailableSems[i]));
        VK_CHECK(vkCreateSemaphore(m_impl->device, &semCI, nullptr, &m_impl->renderFinishedSems[i]));

        VkFenceCreateInfo fenceCI{};
        fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCI.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VK_CHECK(vkCreateFence(m_impl->device, &fenceCI, nullptr, &m_impl->inFlightFences[i]));
    }

    LOG_INFO("Vulkan renderer initialized ({}x{}, {} swapchain images)",
             width, height, m_impl->swapchainImages.size());
}

// ── Frame loop ───────────────────────────────────────────────────────────────

void VulkanRenderer::resize(int width, int height) {
    m_impl->framebufferResized = true;
    m_impl->width  = width;
    m_impl->height = height;
}

void VulkanRenderer::beginFrame() {
    uint32_t f = m_impl->currentFrame;
    vkWaitForFences(m_impl->device, 1, &m_impl->inFlightFences[f], VK_TRUE, UINT64_MAX);

    VkResult result = vkAcquireNextImageKHR(
        m_impl->device, m_impl->swapchain, UINT64_MAX,
        m_impl->imageAvailableSems[f], VK_NULL_HANDLE, &m_impl->currentImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        m_impl->recreateSwapchain();
        return;
    }

    vkResetFences(m_impl->device, 1, &m_impl->inFlightFences[f]);
    vkResetCommandBuffer(m_impl->commandBuffers[f], 0);

    VkCommandBufferBeginInfo beginI{};
    beginI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(m_impl->commandBuffers[f], &beginI);

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    VkRenderPassBeginInfo rpBegin{};
    rpBegin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.renderPass        = m_impl->renderPass;
    rpBegin.framebuffer       = m_impl->framebuffers[m_impl->currentImageIndex];
    rpBegin.renderArea.extent = m_impl->swapchainExtent;
    rpBegin.clearValueCount   = 1;
    rpBegin.pClearValues      = &clearColor;
    vkCmdBeginRenderPass(m_impl->commandBuffers[f], &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanRenderer::render(const Camera& camera) {
    uint32_t f = m_impl->currentFrame;
    VkCommandBuffer cmd = m_impl->commandBuffers[f];

    m_impl->time += 0.016f;

    // Fill UBO
    PlanetUniformsVk u{};

    glm::mat4 inv = glm::inverse(camera.getViewMatrix());
    std::memcpy(u.invView, &inv[0][0], sizeof(u.invView));

    // Planet rotation matrix
    float angle = m_impl->time * m_impl->params.rotationSpeed + m_impl->params.rotationOffset;
    float c = glm::cos(angle), s = glm::sin(angle);
    glm::mat3 rot(glm::vec3(c, 0, s), glm::vec3(0, 1, 0), glm::vec3(-s, 0, c));
    u.planetRot_col0[0] = rot[0][0]; u.planetRot_col0[1] = rot[0][1]; u.planetRot_col0[2] = rot[0][2]; u.planetRot_col0[3] = 0.f;
    u.planetRot_col1[0] = rot[1][0]; u.planetRot_col1[1] = rot[1][1]; u.planetRot_col1[2] = rot[1][2]; u.planetRot_col1[3] = 0.f;
    u.planetRot_col2[0] = rot[2][0]; u.planetRot_col2[1] = rot[2][1]; u.planetRot_col2[2] = rot[2][2]; u.planetRot_col2[3] = 0.f;

    glm::vec3 cam = camera.getPosition();
    u.camPosX = cam.x; u.camPosY = cam.y; u.camPosZ = cam.z;
    u.time = m_impl->time;

    u.planetX = 0.f; u.planetY = 0.f; u.planetZ = -10.f;
    u.radius  = m_impl->params.radius;

    u.resX = float(m_impl->width); u.resY = float(m_impl->height);
    u.rotOffset = m_impl->params.rotationOffset;
    u.rotSpeed  = m_impl->params.rotationSpeed;

    u.noiseStr    = m_impl->params.noiseStrength;
    u.quality     = m_impl->params.quality;
    u.terrainScale = m_impl->params.terrainScale;
    u.domainWarp  = m_impl->params.domainWarpStrength;

    u.fbmPersist = m_impl->params.fbmPersistence;
    u.fbmLac     = m_impl->params.fbmLacunarity;
    u.fbmExp     = m_impl->params.fbmExponentiation;
    u.fbmOct     = float(m_impl->params.fbmOctaves);

    u.ridged     = m_impl->params.ridgedStrength;
    u.crater     = m_impl->params.craterStrength;
    u.continent  = m_impl->params.continentScale;
    u.waterLevel = m_impl->params.waterLevel;

    u.bandStr  = m_impl->params.bandingStrength;
    u.bandFreq = m_impl->params.bandingFrequency;
    u.polarCap = m_impl->params.polarCapSize;

    u.cloudDensity = m_impl->params.cloudsDensity;
    u.cloudScale   = m_impl->params.cloudsScale;
    u.cloudSpeed   = m_impl->params.cloudsSpeed;
    u.cloudAlt     = m_impl->params.cloudAltitude;
    u.cloudThick   = m_impl->params.cloudThickness;
    u.sunInt       = m_impl->params.sunIntensity;
    u.ambLight     = m_impl->params.ambientLight;
    u.atmoDensity  = m_impl->params.atmosphereDensity;

    auto v3 = [](float* f, const glm::vec3& v) { f[0]=v.x; f[1]=v.y; f[2]=v.z; };
    v3(&u.atmoR,      m_impl->params.atmosphereColor);
    v3(&u.sunDirX,    glm::normalize(m_impl->params.sunDirection));
    v3(&u.sunColR,    m_impl->params.sunColor);
    v3(&u.deepSpR,    m_impl->params.deepSpaceColor);
    v3(&u.waterDeepR, m_impl->params.waterColorDeep);
    v3(&u.waterSurfR, m_impl->params.waterColorSurface);
    v3(&u.sandR,      m_impl->params.sandColor);
    v3(&u.treeR,      m_impl->params.treeColor);
    v3(&u.rockR,      m_impl->params.rockColor);
    v3(&u.iceR,       m_impl->params.iceColor);
    v3(&u.cloudColR,  m_impl->params.cloudColor);

    u.sandLev    = m_impl->params.sandLevel;
    u.treeLev    = m_impl->params.treeLevel;
    u.rockLev    = m_impl->params.rockLevel;
    u.iceLev     = m_impl->params.iceLevel;
    u.transition = m_impl->params.transition;

    // Black hole
    u.isBlackHole       = m_impl->params.isBlackHole ? 1.0f : 0.0f;
    u.bhMass            = m_impl->params.bhMass;
    u.bhAccretionInner  = m_impl->params.bhAccretionInner;
    u.bhAccretionOuter  = m_impl->params.bhAccretionOuter;
    u.bhDiskSpeed       = m_impl->params.bhDiskSpeed;
    u.bhDiskTurbulence  = m_impl->params.bhDiskTurbulence;
    u.bhDiskBrightness  = m_impl->params.bhDiskBrightness;
    u.bhTempInner       = m_impl->params.bhDiskTemperatureInner;
    u.bhTempOuter       = m_impl->params.bhDiskTemperatureOuter;
    u.bhDopplerStrength = m_impl->params.bhDopplerStrength;
    u.bhRaySteps        = float(m_impl->params.bhRaySteps);
    v3(&u.bhDiskTintR, m_impl->params.bhDiskTint);

    std::memcpy(m_impl->uniformMapped[f], &u, sizeof(u));

    // Draw
    VkViewport viewport{};
    viewport.width    = float(m_impl->swapchainExtent.width);
    viewport.height   = float(m_impl->swapchainExtent.height);
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent = m_impl->swapchainExtent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_impl->pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_impl->pipelineLayout, 0, 1, &m_impl->descriptorSets[f], 0, nullptr);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &m_impl->vertexBuffer, &offset);
    vkCmdDraw(cmd, 6, 1, 0, 0);
}

void VulkanRenderer::endFrame() {
    uint32_t f = m_impl->currentFrame;
    VkCommandBuffer cmd = m_impl->commandBuffers[f];

    vkCmdEndRenderPass(cmd);
    VK_CHECK(vkEndCommandBuffer(cmd));

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{};
    submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount   = 1;
    submit.pWaitSemaphores      = &m_impl->imageAvailableSems[f];
    submit.pWaitDstStageMask    = &waitStage;
    submit.commandBufferCount   = 1;
    submit.pCommandBuffers      = &cmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores    = &m_impl->renderFinishedSems[f];
    VK_CHECK(vkQueueSubmit(m_impl->graphicsQueue, 1, &submit, m_impl->inFlightFences[f]));

    VkPresentInfoKHR present{};
    present.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores    = &m_impl->renderFinishedSems[f];
    present.swapchainCount     = 1;
    present.pSwapchains        = &m_impl->swapchain;
    present.pImageIndices      = &m_impl->currentImageIndex;

    VkResult result = vkQueuePresentKHR(m_impl->presentQueue, &present);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_impl->framebufferResized) {
        m_impl->recreateSwapchain();
    }

    m_impl->currentFrame = (f + 1) % FRAMES_IN_FLIGHT;
}

// ── Accessors ────────────────────────────────────────────────────────────────

PlanetParams& VulkanRenderer::params() { return m_impl->params; }

void* VulkanRenderer::getInstance()             { return m_impl->instance; }
void* VulkanRenderer::getPhysicalDevice()       { return m_impl->physicalDevice; }
void* VulkanRenderer::getDevice()               { return m_impl->device; }
uint32_t VulkanRenderer::getGraphicsQueueFamily(){ return m_impl->graphicsQueueFamily; }
void* VulkanRenderer::getGraphicsQueue()        { return m_impl->graphicsQueue; }
void* VulkanRenderer::getRenderPass()           { return m_impl->renderPass; }
void* VulkanRenderer::getDescriptorPool()       { return m_impl->imguiDescriptorPool; }
void* VulkanRenderer::getCurrentCommandBuffer() { return m_impl->commandBuffers[m_impl->currentFrame]; }
uint32_t VulkanRenderer::getSwapchainImageCount() { return uint32_t(m_impl->swapchainImages.size()); }

} // namespace astrocore
