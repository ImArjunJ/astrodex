#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <imgui.h>
#include <string>
#include <vector>
#include <cstdint>

namespace astrocore {

struct PlanetParams;
class Camera;

/// Offscreen Vulkan thumbnail renderer for mini planet previews.
/// Manages a single reusable framebuffer at configurable resolution
/// (default 128px square). Renders a simplified planet representation
/// to the FBO, supports PNG serialization via stb_image_write, and
/// registers the rendered texture with ImGui for catalogue card display.
///
/// Usage:
///   ThumbnailRenderer thumbs(device, allocator, queue, cmdPool, 128);
///   ImTextureID tex = thumbs.renderThumbnail(params, camera);
///   thumbs.saveToPNG(".cache/thumbnails/kepler-186f.png");
///
/// The FBO is reused across multiple renderThumbnail() calls to save VRAM.
/// Full procedural planet rendering is deferred to a future optimization pass;
/// current thumbnails render a solid colored sphere based on planet type color.
class ThumbnailRenderer {
public:
    /// Create the offscreen renderer.
    /// @param device   Vulkan logical device
    /// @param allocator VMA allocator for all GPU memory
    /// @param queue    Graphics queue for command submission
    /// @param cmdPool  Command pool for allocating command buffers
    /// @param size     Thumbnail resolution in pixels (square, default 128)
    ThumbnailRenderer(VkDevice device, VmaAllocator allocator,
                      VkQueue queue, VkCommandPool cmdPool,
                      uint32_t size = 128);
    ~ThumbnailRenderer();

    ThumbnailRenderer(const ThumbnailRenderer&) = delete;
    ThumbnailRenderer& operator=(const ThumbnailRenderer&) = delete;

    /// Render planet to offscreen FBO and return ImTextureID for ImGui display.
    /// The camera should be positioned to frame the planet (e.g., distance = radius * 3).
    /// Currently renders a solid colored circle based on planet atmosphere/surface color.
    ImTextureID renderThumbnail(const PlanetParams& params, const Camera& camera);

    /// Save current FBO contents to PNG file (call after renderThumbnail).
    /// Creates parent directories if needed.
    /// @return true on success
    bool saveToPNG(const std::string& filepath);

    /// Get the current rendered texture as ImTextureID (reuse without re-rendering).
    ImTextureID getImGuiTexture() const { return m_imguiTexture; }

    uint32_t getSize() const { return m_size; }

private:
    void createOffscreenResources();
    void cleanupOffscreenResources();
    void transitionImageLayout(VkCommandBuffer cmd, VkImage image,
                               VkImageLayout oldLayout, VkImageLayout newLayout,
                               VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                               VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage);
    void readbackPixels(std::vector<uint8_t>& pixels);

    VkDevice m_device;
    VmaAllocator m_allocator;
    VkQueue m_queue;
    VkCommandPool m_cmdPool;
    uint32_t m_size;

    // Offscreen FBO resources
    VkImage m_colorImage = VK_NULL_HANDLE;
    VmaAllocation m_colorAlloc = VK_NULL_HANDLE;
    VkImageView m_colorView = VK_NULL_HANDLE;

    VkImage m_depthImage = VK_NULL_HANDLE;
    VmaAllocation m_depthAlloc = VK_NULL_HANDLE;
    VkImageView m_depthView = VK_NULL_HANDLE;

    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkFramebuffer m_framebuffer = VK_NULL_HANDLE;

    VkSampler m_sampler = VK_NULL_HANDLE;
    VkDescriptorSet m_imguiDescSet = VK_NULL_HANDLE;
    ImTextureID m_imguiTexture = 0;

    // Staging buffer for pixel readback (reused)
    VkBuffer m_stagingBuffer = VK_NULL_HANDLE;
    VmaAllocation m_stagingAlloc = VK_NULL_HANDLE;
    VmaAllocationInfo m_stagingAllocInfo{};

    // Track whether we have rendered at least once (for layout transitions)
    bool m_hasRendered = false;
};

}  // namespace astrocore
