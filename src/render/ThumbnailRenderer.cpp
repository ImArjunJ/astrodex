#include "render/ThumbnailRenderer.hpp"
#include "render/IRenderer.hpp"  // PlanetParams
#include "render/Camera.hpp"
#include "core/Logger.hpp"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <imgui.h>
#include <imgui_impl_vulkan.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <filesystem>
#include <cstring>
#include <cmath>
#include <stdexcept>
#include <algorithm>

// stb_image_write for PNG serialization (suppress third-party warnings)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wconversion"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../external/stb_image_write.h"
#pragma GCC diagnostic pop

namespace astrocore {

// ── Helpers ──────────────────────────────────────────────────────────────────

#define VK_THUMB_CHECK(x)                                                      \
    do {                                                                        \
        VkResult _r = (x);                                                      \
        if (_r != VK_SUCCESS) {                                                 \
            LOG_ERROR("ThumbnailRenderer Vulkan error {} at {}:{}", int(_r),    \
                      __FILE__, __LINE__);                                       \
            throw std::runtime_error("ThumbnailRenderer: Vulkan call failed");  \
        }                                                                       \
    } while (0)

// ── Constructor / Destructor ─────────────────────────────────────────────────

ThumbnailRenderer::ThumbnailRenderer(VkDevice device, VmaAllocator allocator,
                                     VkQueue queue, VkCommandPool cmdPool,
                                     uint32_t size)
    : m_device(device), m_allocator(allocator), m_queue(queue),
      m_cmdPool(cmdPool), m_size(size) {
    createOffscreenResources();
    LOG_INFO("ThumbnailRenderer created ({}x{} px)", m_size, m_size);
}

ThumbnailRenderer::~ThumbnailRenderer() {
    if (m_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device);
    }
    cleanupOffscreenResources();
}

// ── Image Layout Transition ──────────────────────────────────────────────────

void ThumbnailRenderer::transitionImageLayout(
    VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout,
    VkImageLayout newLayout, VkAccessFlags srcAccess, VkAccessFlags dstAccess,
    VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage) {

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1,
                         &barrier);
}

// ── Create Offscreen Resources ───────────────────────────────────────────────

void ThumbnailRenderer::createOffscreenResources() {
    // 1. Color image (RGBA8, for rendering + sampling + transfer readback)
    {
        VkImageCreateInfo imgCI{};
        imgCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgCI.imageType = VK_IMAGE_TYPE_2D;
        imgCI.format = VK_FORMAT_R8G8B8A8_UNORM;
        imgCI.extent = {m_size, m_size, 1};
        imgCI.mipLevels = 1;
        imgCI.arrayLayers = 1;
        imgCI.samples = VK_SAMPLE_COUNT_1_BIT;
        imgCI.tiling = VK_IMAGE_TILING_OPTIMAL;
        imgCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                      VK_IMAGE_USAGE_SAMPLED_BIT |
                      VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imgCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo allocCI{};
        allocCI.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        VK_THUMB_CHECK(vmaCreateImage(m_allocator, &imgCI, &allocCI,
                                      &m_colorImage, &m_colorAlloc, nullptr));
    }

    // 2. Depth image (D32_SFLOAT, depth-only attachment)
    {
        VkImageCreateInfo imgCI{};
        imgCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgCI.imageType = VK_IMAGE_TYPE_2D;
        imgCI.format = VK_FORMAT_D32_SFLOAT;
        imgCI.extent = {m_size, m_size, 1};
        imgCI.mipLevels = 1;
        imgCI.arrayLayers = 1;
        imgCI.samples = VK_SAMPLE_COUNT_1_BIT;
        imgCI.tiling = VK_IMAGE_TILING_OPTIMAL;
        imgCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imgCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo allocCI{};
        allocCI.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        VK_THUMB_CHECK(vmaCreateImage(m_allocator, &imgCI, &allocCI,
                                      &m_depthImage, &m_depthAlloc, nullptr));
    }

    // 3. Image views
    {
        VkImageViewCreateInfo viewCI{};
        viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewCI.image = m_colorImage;
        viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewCI.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewCI.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VK_THUMB_CHECK(
            vkCreateImageView(m_device, &viewCI, nullptr, &m_colorView));

        viewCI.image = m_depthImage;
        viewCI.format = VK_FORMAT_D32_SFLOAT;
        viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        VK_THUMB_CHECK(
            vkCreateImageView(m_device, &viewCI, nullptr, &m_depthView));
    }

    // 4. Render pass (color + depth, single subpass)
    {
        VkAttachmentDescription attachments[2]{};

        // Color attachment
        attachments[0].format = VK_FORMAT_R8G8B8A8_UNORM;
        attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // Depth attachment
        attachments[1].format = VK_FORMAT_D32_SFLOAT;
        attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[1].finalLayout =
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorRef{};
        colorRef.attachment = 0;
        colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthRef{};
        depthRef.attachment = 1;
        depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;
        subpass.pDepthStencilAttachment = &depthRef;

        // Subpass dependency: ensure external writes complete before we render
        VkSubpassDependency dep{};
        dep.srcSubpass = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass = 0;
        dep.srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                           VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dep.srcAccessMask = 0;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo rpCI{};
        rpCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpCI.attachmentCount = 2;
        rpCI.pAttachments = attachments;
        rpCI.subpassCount = 1;
        rpCI.pSubpasses = &subpass;
        rpCI.dependencyCount = 1;
        rpCI.pDependencies = &dep;

        VK_THUMB_CHECK(
            vkCreateRenderPass(m_device, &rpCI, nullptr, &m_renderPass));
    }

    // 5. Framebuffer
    {
        VkImageView fbAttachments[] = {m_colorView, m_depthView};

        VkFramebufferCreateInfo fbCI{};
        fbCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbCI.renderPass = m_renderPass;
        fbCI.attachmentCount = 2;
        fbCI.pAttachments = fbAttachments;
        fbCI.width = m_size;
        fbCI.height = m_size;
        fbCI.layers = 1;

        VK_THUMB_CHECK(
            vkCreateFramebuffer(m_device, &fbCI, nullptr, &m_framebuffer));
    }

    // 6. Sampler (linear filtering for ImGui display)
    {
        VkSamplerCreateInfo sampCI{};
        sampCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampCI.magFilter = VK_FILTER_LINEAR;
        sampCI.minFilter = VK_FILTER_LINEAR;
        sampCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

        VK_THUMB_CHECK(
            vkCreateSampler(m_device, &sampCI, nullptr, &m_sampler));
    }

    // 7. Staging buffer for GPU->CPU pixel readback (persistently mapped)
    {
        VkBufferCreateInfo bufCI{};
        bufCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufCI.size = static_cast<VkDeviceSize>(m_size) * m_size * 4; // RGBA
        bufCI.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VmaAllocationCreateInfo allocCI{};
        allocCI.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
        allocCI.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VK_THUMB_CHECK(vmaCreateBuffer(m_allocator, &bufCI, &allocCI,
                                       &m_stagingBuffer, &m_stagingAlloc,
                                       &m_stagingAllocInfo));
    }

    // 8. Register texture with ImGui for display
    //    We need the image in SHADER_READ_ONLY layout first.
    //    Do an initial transition + register.
    {
        VkCommandBuffer cmd;
        VkCommandBufferAllocateInfo cmdAI{};
        cmdAI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAI.commandPool = m_cmdPool;
        cmdAI.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAI.commandBufferCount = 1;
        VK_THUMB_CHECK(vkAllocateCommandBuffers(m_device, &cmdAI, &cmd));

        VkCommandBufferBeginInfo beginI{};
        beginI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginI.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &beginI);

        transitionImageLayout(
            cmd, m_colorImage, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0,
            VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

        vkEndCommandBuffer(cmd);

        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;
        vkQueueSubmit(m_queue, 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(m_queue);

        vkFreeCommandBuffers(m_device, m_cmdPool, 1, &cmd);

        // Register with ImGui -- store descriptor set and derive ImTextureID
        m_imguiDescSet = ImGui_ImplVulkan_AddTexture(
            m_sampler, m_colorView,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        m_imguiTexture = reinterpret_cast<ImTextureID>(m_imguiDescSet);
    }

    LOG_DEBUG("ThumbnailRenderer offscreen resources created");
}

// ── Cleanup ──────────────────────────────────────────────────────────────────

void ThumbnailRenderer::cleanupOffscreenResources() {
    // Remove ImGui texture registration
    if (m_imguiDescSet != VK_NULL_HANDLE) {
        ImGui_ImplVulkan_RemoveTexture(m_imguiDescSet);
        m_imguiDescSet = VK_NULL_HANDLE;
        m_imguiTexture = 0;
    }

    if (m_stagingBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(m_allocator, m_stagingBuffer, m_stagingAlloc);
        m_stagingBuffer = VK_NULL_HANDLE;
    }

    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }

    if (m_framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(m_device, m_framebuffer, nullptr);
        m_framebuffer = VK_NULL_HANDLE;
    }

    if (m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }

    if (m_depthView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, m_depthView, nullptr);
        m_depthView = VK_NULL_HANDLE;
    }

    if (m_colorView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, m_colorView, nullptr);
        m_colorView = VK_NULL_HANDLE;
    }

    if (m_depthImage != VK_NULL_HANDLE) {
        vmaDestroyImage(m_allocator, m_depthImage, m_depthAlloc);
        m_depthImage = VK_NULL_HANDLE;
    }

    if (m_colorImage != VK_NULL_HANDLE) {
        vmaDestroyImage(m_allocator, m_colorImage, m_colorAlloc);
        m_colorImage = VK_NULL_HANDLE;
    }
}

// ── Render Thumbnail ─────────────────────────────────────────────────────────

ImTextureID ThumbnailRenderer::renderThumbnail(const PlanetParams& params,
                                               const Camera& /*camera*/) {
    // Allocate a one-shot command buffer
    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo cmdAI{};
    cmdAI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAI.commandPool = m_cmdPool;
    cmdAI.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAI.commandBufferCount = 1;
    VK_THUMB_CHECK(vkAllocateCommandBuffers(m_device, &cmdAI, &cmd));

    VkCommandBufferBeginInfo beginI{};
    beginI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginI.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginI);

    // Transition color image: SHADER_READ_ONLY -> rendered via render pass
    // The render pass handles the initial layout transition from UNDEFINED
    // (or we could transition from SHADER_READ -> COLOR_ATTACHMENT here).
    // Since render pass initialLayout is UNDEFINED and loadOp is CLEAR,
    // the render pass will handle transition implicitly.

    // Begin render pass with clear values
    VkClearValue clearValues[2]{};
    // Clear color: dark space background
    clearValues[0].color = {{0.0f, 0.0f, 0.02f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rpBI{};
    rpBI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBI.renderPass = m_renderPass;
    rpBI.framebuffer = m_framebuffer;
    rpBI.renderArea.offset = {0, 0};
    rpBI.renderArea.extent = {m_size, m_size};
    rpBI.clearValueCount = 2;
    rpBI.pClearValues = clearValues;

    vkCmdBeginRenderPass(cmd, &rpBI, VK_SUBPASS_CONTENTS_INLINE);

    // Set viewport and scissor for the offscreen FBO
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_size);
    viewport.height = static_cast<float>(m_size);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {m_size, m_size};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Renders planet-type color as clear color (each type gets a unique thumbnail).
    // Full procedural planet pipeline integration is a future enhancement.
    //
    // Extract a representative color from the planet params:
    //   - Use atmosphere color as the primary indicator of planet type
    //   - This gives terrestrial=blue, gas giants=orange/brown, ice giants=cyan
    glm::vec3 planetColor = params.atmosphereColor;
    float intensity = std::max({planetColor.r, planetColor.g, planetColor.b});
    if (intensity < 0.05f) {
        // Fallback for very dark atmosphere colors: use rock/sand color
        planetColor = params.rockColor;
    }

    // We cannot retroactively change the clear color after vkCmdBeginRenderPass,
    // so the FBO has been cleared with the dark space color above.
    // The actual planet color differentiation will come from the draw commands
    // once the full pipeline is wired in.
    //
    // For now, we re-clear by ending this pass and starting a new one
    // with the planet-specific color. This is a simple approach for placeholders.
    vkCmdEndRenderPass(cmd);

    // Second pass: planet-colored clear
    clearValues[0].color = {
        {planetColor.r, planetColor.g, planetColor.b, 1.0f}};

    rpBI.pClearValues = clearValues;
    vkCmdBeginRenderPass(cmd, &rpBI, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdEndRenderPass(cmd);

    // Transition color image: COLOR_ATTACHMENT -> SHADER_READ_ONLY for ImGui
    transitionImageLayout(
        cmd, m_colorImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    vkEndCommandBuffer(cmd);

    // Submit and wait
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkQueueSubmit(m_queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_queue);

    vkFreeCommandBuffers(m_device, m_cmdPool, 1, &cmd);

    m_hasRendered = true;
    return m_imguiTexture;
}

// ── Pixel Readback ───────────────────────────────────────────────────────────

void ThumbnailRenderer::readbackPixels(std::vector<uint8_t>& pixels) {
    pixels.resize(static_cast<size_t>(m_size) * m_size * 4);

    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo cmdAI{};
    cmdAI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAI.commandPool = m_cmdPool;
    cmdAI.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAI.commandBufferCount = 1;
    VK_THUMB_CHECK(vkAllocateCommandBuffers(m_device, &cmdAI, &cmd));

    VkCommandBufferBeginInfo beginI{};
    beginI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginI.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginI);

    // Transition: SHADER_READ_ONLY -> TRANSFER_SRC
    transitionImageLayout(cmd, m_colorImage,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          VK_ACCESS_SHADER_READ_BIT,
                          VK_ACCESS_TRANSFER_READ_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                          VK_PIPELINE_STAGE_TRANSFER_BIT);

    // Copy image to staging buffer
    VkBufferImageCopy copyRegion{};
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = 0;   // tightly packed
    copyRegion.bufferImageHeight = 0; // tightly packed
    copyRegion.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copyRegion.imageOffset = {0, 0, 0};
    copyRegion.imageExtent = {m_size, m_size, 1};

    vkCmdCopyImageToBuffer(cmd, m_colorImage,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           m_stagingBuffer, 1, &copyRegion);

    // Transition back: TRANSFER_SRC -> SHADER_READ_ONLY
    transitionImageLayout(cmd, m_colorImage,
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_ACCESS_TRANSFER_READ_BIT,
                          VK_ACCESS_SHADER_READ_BIT,
                          VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkQueueSubmit(m_queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_queue);

    vkFreeCommandBuffers(m_device, m_cmdPool, 1, &cmd);

    // Copy from mapped staging buffer to output
    if (m_stagingAllocInfo.pMappedData) {
        std::memcpy(pixels.data(), m_stagingAllocInfo.pMappedData,
                    pixels.size());
    } else {
        LOG_ERROR("ThumbnailRenderer: staging buffer not mapped");
    }
}

// ── Save to PNG ──────────────────────────────────────────────────────────────

bool ThumbnailRenderer::saveToPNG(const std::string& filepath) {
    if (!m_hasRendered) {
        LOG_WARN("ThumbnailRenderer::saveToPNG called before renderThumbnail");
        return false;
    }

    // Read pixels from GPU
    std::vector<uint8_t> pixels;
    readbackPixels(pixels);

    // Flip Y-axis: Vulkan origin is top-left, PNG convention is top-left too,
    // BUT if the viewport was not Y-flipped, the image may be inverted.
    // For Vulkan with standard viewport (origin top-left), no flip needed
    // for PNG since PNG also starts from top-left.
    // We keep the data as-is since both Vulkan and PNG use top-left origin.

    // Create parent directories if needed
    auto parentPath = std::filesystem::path(filepath).parent_path();
    if (!parentPath.empty()) {
        std::filesystem::create_directories(parentPath);
    }

    // Write PNG via stb_image_write
    int result = stbi_write_png(filepath.c_str(),
                                static_cast<int>(m_size),
                                static_cast<int>(m_size), 4, pixels.data(),
                                static_cast<int>(m_size) * 4);

    if (result != 0) {
        LOG_DEBUG("Thumbnail saved: {} ({}x{})", filepath, m_size, m_size);
        return true;
    } else {
        LOG_ERROR("Failed to save thumbnail PNG: {}", filepath);
        return false;
    }
}

}  // namespace astrocore
