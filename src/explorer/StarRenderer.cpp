#include "explorer/StarRenderer.hpp"
#include "explorer/StarData.hpp"
#include "explorer/FreeFlyCamera.hpp"
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

static std::string resolveAssetPath(const std::string& relative) {
#ifdef __APPLE__
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
    return relative;
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

// ── UBO (std140 layout) ─────────────────────────────────────────────────────

struct alignas(16) StarUniformsVk {
    float view[16];         // mat4 (64 bytes)
    float projection[16];   // mat4 (64 bytes)
    float cameraPos_time[4]; // vec4 (16 bytes)
    float renderParams[4];   // vec4 (16 bytes)
};
static_assert(sizeof(StarUniformsVk) == 160, "StarUniforms must be 160 bytes");

// ── Pimpl ────────────────────────────────────────────────────────────────────

static constexpr uint32_t FRAMES_IN_FLIGHT = 2;

struct StarRenderer::Impl {
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

    // Star vertex buffer (static — used by uploadStars / HYG fallback)
    VkBuffer       vertexBuffer     = VK_NULL_HANDLE;
    VmaAllocation  vertexAlloc      = VK_NULL_HANDLE;
    uint32_t       starCount        = 0;

    // Dynamic vertex buffer (per-frame — used by updateStars / octree)
    static constexpr uint32_t MAX_DYNAMIC_STARS = 2'000'000;
    VkBuffer       dynamicVertexBuffer = VK_NULL_HANDLE;
    VmaAllocation  dynamicVertexAlloc  = VK_NULL_HANDLE;
    void*          dynamicVertexMapped = nullptr;
    uint32_t       dynamicStarCount    = 0;
    bool           useDynamic          = false;

    // Uniform buffers (per frame)
    std::array<VkBuffer,      FRAMES_IN_FLIGHT> uniformBuffers{};
    std::array<VmaAllocation, FRAMES_IN_FLIGHT> uniformAllocs{};
    std::array<void*,         FRAMES_IN_FLIGHT> uniformMapped{};

    // State
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

void StarRenderer::Impl::createSwapchain(int w, int h) {
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

void StarRenderer::Impl::createFramebuffers() {
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

void StarRenderer::Impl::cleanupSwapchain() {
    for (auto fb : framebuffers) vkDestroyFramebuffer(device, fb, nullptr);
    framebuffers.clear();
    for (auto iv : swapchainImageViews) vkDestroyImageView(device, iv, nullptr);
    swapchainImageViews.clear();
}

void StarRenderer::Impl::recreateSwapchain() {
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

StarRenderer::StarRenderer()  : m_impl(std::make_unique<Impl>()) {}
StarRenderer::~StarRenderer() {
    if (!m_impl || m_impl->device == VK_NULL_HANDLE) return;
    vkDeviceWaitIdle(m_impl->device);

    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        vkDestroyFence(m_impl->device,     m_impl->inFlightFences[i],    nullptr);
        vkDestroySemaphore(m_impl->device,  m_impl->renderFinishedSems[i], nullptr);
        vkDestroySemaphore(m_impl->device,  m_impl->imageAvailableSems[i], nullptr);
        vkDestroyCommandPool(m_impl->device, m_impl->commandPools[i],     nullptr);
    }

    vkDestroyDescriptorPool(m_impl->device, m_impl->descriptorPool,      nullptr);
    vkDestroyDescriptorPool(m_impl->device, m_impl->imguiDescriptorPool, nullptr);

    vkDestroyPipeline(m_impl->device,       m_impl->pipeline,       nullptr);
    vkDestroyPipelineLayout(m_impl->device,  m_impl->pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_impl->device, m_impl->descriptorSetLayout, nullptr);
    vkDestroyRenderPass(m_impl->device,      m_impl->renderPass,    nullptr);

    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        vmaDestroyBuffer(m_impl->allocator, m_impl->uniformBuffers[i], m_impl->uniformAllocs[i]);
    }
    if (m_impl->dynamicVertexBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(m_impl->allocator, m_impl->dynamicVertexBuffer, m_impl->dynamicVertexAlloc);
    }
    if (m_impl->vertexBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(m_impl->allocator, m_impl->vertexBuffer, m_impl->vertexAlloc);
    }

    m_impl->cleanupSwapchain();
    vkDestroySwapchainKHR(m_impl->device, m_impl->swapchain, nullptr);

    vmaDestroyAllocator(m_impl->allocator);
    vkDestroySurfaceKHR(m_impl->instance, m_impl->surface, nullptr);
    vkb::destroy_device(m_impl->vkbDevice);
    vkb::destroy_instance(m_impl->vkbInstance);
}

// ── Initialization ───────────────────────────────────────────────────────────

void StarRenderer::init(int width, int height, GLFWwindow* glfwWindow) {
    m_impl->window = glfwWindow;
    m_impl->width  = width;
    m_impl->height = height;

    // 0. macOS MoltenVK ICD detection
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
    vkb::InstanceBuilder instBuilder(vkGetInstanceProcAddr);
    instBuilder.set_app_name("StarExplorer")
               .require_api_version(1, 2, 0)
               .set_engine_name("AstroCore");
#ifndef NDEBUG
    instBuilder.request_validation_layers()
               .use_default_debug_messenger();
#endif
    instBuilder.enable_extension(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);

    auto instResult = instBuilder.build();
    if (!instResult) throw std::runtime_error("Failed to create Vulkan instance: " + instResult.error().message());
    m_impl->vkbInstance = instResult.value();
    m_impl->instance    = m_impl->vkbInstance.instance;
    LOG_INFO("Vulkan instance created (StarExplorer)");

    // 2. Surface
    VK_CHECK(glfwCreateWindowSurface(m_impl->instance, m_impl->window, nullptr, &m_impl->surface));

    // 3. Physical + logical device
    vkb::PhysicalDeviceSelector selector{m_impl->vkbInstance};
    selector.set_surface(m_impl->surface)
            .set_minimum_version(1, 2)
            .add_required_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
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

    // 8. Descriptor set layout (UBO only, vertex+fragment)
    {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding         = 0;
        binding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        binding.descriptorCount = 1;
        binding.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo ci{};
        ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        ci.bindingCount = 1;
        ci.pBindings    = &binding;
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

    // 10. Graphics pipeline — POINT_LIST, additive blending, no depth test
    {
        auto vertCode = readFile("shaders/star.vert.spv");
        auto fragCode = readFile("shaders/star.frag.spv");
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

        // StarVertex: position(vec3) + magnitude(float) + color(vec3) = 28 bytes
        VkVertexInputBindingDescription vertBind{};
        vertBind.binding   = 0;
        vertBind.stride    = 28;
        vertBind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription vertAttrs[3]{};
        // location 0: position (vec3, offset 0)
        vertAttrs[0].location = 0;
        vertAttrs[0].binding  = 0;
        vertAttrs[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
        vertAttrs[0].offset   = 0;
        // location 1: magnitude (float, offset 12)
        vertAttrs[1].location = 1;
        vertAttrs[1].binding  = 0;
        vertAttrs[1].format   = VK_FORMAT_R32_SFLOAT;
        vertAttrs[1].offset   = 12;
        // location 2: color (vec3, offset 16)
        vertAttrs[2].location = 2;
        vertAttrs[2].binding  = 0;
        vertAttrs[2].format   = VK_FORMAT_R32G32B32_SFLOAT;
        vertAttrs[2].offset   = 16;

        VkPipelineVertexInputStateCreateInfo vertInput{};
        vertInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertInput.vertexBindingDescriptionCount   = 1;
        vertInput.pVertexBindingDescriptions      = &vertBind;
        vertInput.vertexAttributeDescriptionCount = 3;
        vertInput.pVertexAttributeDescriptions    = vertAttrs;

        // POINT_LIST topology
        VkPipelineInputAssemblyStateCreateInfo inputAsm{};
        inputAsm.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAsm.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

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

        // No depth testing for additive point cloud
        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable  = VK_FALSE;
        depthStencil.depthWriteEnable = VK_FALSE;

        // Additive blending: src * srcAlpha + dst * ONE
        VkPipelineColorBlendAttachmentState blend{};
        blend.blendEnable         = VK_TRUE;
        blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        blend.colorBlendOp        = VK_BLEND_OP_ADD;
        blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blend.alphaBlendOp        = VK_BLEND_OP_ADD;
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
        pipeCI.pDepthStencilState  = &depthStencil;
        pipeCI.pColorBlendState    = &blendState;
        pipeCI.pDynamicState       = &dynState;
        pipeCI.layout              = m_impl->pipelineLayout;
        pipeCI.renderPass          = m_impl->renderPass;
        pipeCI.subpass             = 0;

        VK_CHECK(vkCreateGraphicsPipelines(m_impl->device, VK_NULL_HANDLE, 1, &pipeCI, nullptr, &m_impl->pipeline));

        vkDestroyShaderModule(m_impl->device, fragMod, nullptr);
        vkDestroyShaderModule(m_impl->device, vertMod, nullptr);
    }

    // 11. Uniform buffers (per frame, host-visible, persistently mapped)
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        VkBufferCreateInfo bufCI{};
        bufCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufCI.size  = sizeof(StarUniformsVk);
        bufCI.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

        VmaAllocationCreateInfo vmaCI{};
        vmaCI.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        vmaCI.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo allocInfo{};
        VK_CHECK(vmaCreateBuffer(m_impl->allocator, &bufCI, &vmaCI,
                                 &m_impl->uniformBuffers[i], &m_impl->uniformAllocs[i], &allocInfo));
        m_impl->uniformMapped[i] = allocInfo.pMappedData;
    }

    // 11b. Dynamic vertex buffer (CPU_TO_GPU, persistently mapped, for octree per-frame updates)
    {
        VkBufferCreateInfo bufCI{};
        bufCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufCI.size  = Impl::MAX_DYNAMIC_STARS * sizeof(StarVertex);
        bufCI.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

        VmaAllocationCreateInfo vmaCI{};
        vmaCI.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        vmaCI.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo allocInfo{};
        VK_CHECK(vmaCreateBuffer(m_impl->allocator, &bufCI, &vmaCI,
                                 &m_impl->dynamicVertexBuffer, &m_impl->dynamicVertexAlloc, &allocInfo));
        m_impl->dynamicVertexMapped = allocInfo.pMappedData;
    }

    // 12. Descriptor pool + sets
    {
        VkDescriptorPoolSize poolSize{};
        poolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize.descriptorCount = FRAMES_IN_FLIGHT;

        VkDescriptorPoolCreateInfo poolCI{};
        poolCI.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolCI.maxSets       = FRAMES_IN_FLIGHT;
        poolCI.poolSizeCount = 1;
        poolCI.pPoolSizes    = &poolSize;
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
            bufInfo.range  = sizeof(StarUniformsVk);

            VkWriteDescriptorSet write{};
            write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet          = m_impl->descriptorSets[i];
            write.dstBinding      = 0;
            write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.descriptorCount = 1;
            write.pBufferInfo     = &bufInfo;

            vkUpdateDescriptorSets(m_impl->device, 1, &write, 0, nullptr);
        }
    }

    // 13. ImGui descriptor pool (separate, with free flag)
    {
        VkDescriptorPoolSize poolSize{};
        poolSize.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = 10;

        VkDescriptorPoolCreateInfo poolCI{};
        poolCI.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolCI.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolCI.maxSets       = 10;
        poolCI.poolSizeCount = 1;
        poolCI.pPoolSizes    = &poolSize;
        VK_CHECK(vkCreateDescriptorPool(m_impl->device, &poolCI, nullptr, &m_impl->imguiDescriptorPool));
    }

    // 14. Per-frame command pools, command buffers, sync objects
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

    LOG_INFO("Star renderer initialized ({}x{}, {} swapchain images)",
             width, height, m_impl->swapchainImages.size());
}

// ── Upload star data ─────────────────────────────────────────────────────────

void StarRenderer::uploadStars(const std::vector<StarVertex>& stars) {
    if (stars.empty()) return;

    m_impl->starCount = static_cast<uint32_t>(stars.size());
    VkDeviceSize bufferSize = stars.size() * sizeof(StarVertex);

    // Create GPU vertex buffer
    VkBufferCreateInfo bufCI{};
    bufCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufCI.size  = bufferSize;
    bufCI.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo allocCI{};
    allocCI.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VK_CHECK(vmaCreateBuffer(m_impl->allocator, &bufCI, &allocCI,
                             &m_impl->vertexBuffer, &m_impl->vertexAlloc, nullptr));

    // Staging buffer
    VkBuffer staging; VmaAllocation stagingAlloc;
    VkBufferCreateInfo stageBufCI{};
    stageBufCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stageBufCI.size  = bufferSize;
    stageBufCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VmaAllocationCreateInfo stageAllocCI{};
    stageAllocCI.usage = VMA_MEMORY_USAGE_CPU_ONLY;
    VK_CHECK(vmaCreateBuffer(m_impl->allocator, &stageBufCI, &stageAllocCI, &staging, &stagingAlloc, nullptr));

    void* mapped;
    vmaMapMemory(m_impl->allocator, stagingAlloc, &mapped);
    std::memcpy(mapped, stars.data(), bufferSize);
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
    VkBufferCopy region{0, 0, bufferSize};
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

    LOG_INFO("Uploaded {} stars to GPU ({} KB)", m_impl->starCount, bufferSize / 1024);
}

void StarRenderer::updateStars(const std::vector<StarVertex>& stars) {
    uint32_t count = std::min(static_cast<uint32_t>(stars.size()), Impl::MAX_DYNAMIC_STARS);
    if (count == 0) {
        m_impl->dynamicStarCount = 0;
        m_impl->useDynamic = true;
        return;
    }
    std::memcpy(m_impl->dynamicVertexMapped, stars.data(), count * sizeof(StarVertex));
    m_impl->dynamicStarCount = count;
    m_impl->useDynamic = true;
}

// ── Frame loop ───────────────────────────────────────────────────────────────

void StarRenderer::resize(int width, int height) {
    m_impl->framebufferResized = true;
    m_impl->width  = width;
    m_impl->height = height;
}

void StarRenderer::beginFrame() {
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

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.005f, 1.0f}}}; // very dark blue
    VkRenderPassBeginInfo rpBegin{};
    rpBegin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.renderPass        = m_impl->renderPass;
    rpBegin.framebuffer       = m_impl->framebuffers[m_impl->currentImageIndex];
    rpBegin.renderArea.extent = m_impl->swapchainExtent;
    rpBegin.clearValueCount   = 1;
    rpBegin.pClearValues      = &clearColor;
    vkCmdBeginRenderPass(m_impl->commandBuffers[f], &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
}

void StarRenderer::render(const FreeFlyCamera& camera, float time, float pointScale, float brightnessBoost) {
    uint32_t f = m_impl->currentFrame;
    VkCommandBuffer cmd = m_impl->commandBuffers[f];

    // Fill UBO
    StarUniformsVk u{};
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 proj = camera.getProjectionMatrix();
    std::memcpy(u.view, &view[0][0], sizeof(u.view));
    std::memcpy(u.projection, &proj[0][0], sizeof(u.projection));

    glm::vec3 camPos = camera.getPosition();
    u.cameraPos_time[0] = camPos.x;
    u.cameraPos_time[1] = camPos.y;
    u.cameraPos_time[2] = camPos.z;
    u.cameraPos_time[3] = time;

    u.renderParams[0] = pointScale;
    u.renderParams[1] = brightnessBoost;
    u.renderParams[2] = 0.0f;
    u.renderParams[3] = 0.0f;

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

    // Choose dynamic (octree) or static (HYG) vertex buffer
    if (m_impl->useDynamic && m_impl->dynamicStarCount > 0) {
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &m_impl->dynamicVertexBuffer, &offset);
        vkCmdDraw(cmd, m_impl->dynamicStarCount, 1, 0, 0);
    } else if (m_impl->vertexBuffer != VK_NULL_HANDLE && m_impl->starCount > 0) {
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &m_impl->vertexBuffer, &offset);
        vkCmdDraw(cmd, m_impl->starCount, 1, 0, 0);
    }
}

void StarRenderer::endFrame() {
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

// ── Vulkan accessors for ImGui ───────────────────────────────────────────────

void* StarRenderer::getInstance()          { return m_impl->instance; }
void* StarRenderer::getPhysicalDevice()    { return m_impl->physicalDevice; }
void* StarRenderer::getDevice()            { return m_impl->device; }
uint32_t StarRenderer::getGraphicsQueueFamily() { return m_impl->graphicsQueueFamily; }
void* StarRenderer::getGraphicsQueue()     { return m_impl->graphicsQueue; }
void* StarRenderer::getRenderPass()        { return m_impl->renderPass; }
void* StarRenderer::getDescriptorPool()    { return m_impl->imguiDescriptorPool; }
void* StarRenderer::getCurrentCommandBuffer() { return m_impl->commandBuffers[m_impl->currentFrame]; }
uint32_t StarRenderer::getSwapchainImageCount() { return static_cast<uint32_t>(m_impl->swapchainImages.size()); }

} // namespace astrocore
