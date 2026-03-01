#ifdef __EMSCRIPTEN__

#include "render/WebGPURenderer.hpp"
#include "render/VulkanTypes.hpp"
#include "render/Camera.hpp"
#include "core/Logger.hpp"

#include <webgpu/webgpu.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "../../external/stb_image.h"

#include <cstring>
#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>
#include <array>

namespace astrocore {

// ── Helpers ──────────────────────────────────────────────────────────────────

static std::string loadShaderSource(const std::string& path) {
    std::string resolved = RendererBase::resolveAssetPath(path);
    std::ifstream f(resolved);
    if (!f.is_open()) {
        LOG_ERROR("Failed to open shader: {}", path);
        return "";
    }
    return std::string((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
}

// ── Pimpl ────────────────────────────────────────────────────────────────────

struct WebGPURenderer::Impl {
    GLFWwindow* window = nullptr;

    WGPUInstance    instance = nullptr;
    WGPUSurface     surface  = nullptr;
    WGPUAdapter     adapter  = nullptr;
    WGPUDevice      device   = nullptr;
    WGPUQueue       queue    = nullptr;

    WGPUTextureFormat surfaceFormat = WGPUTextureFormat_BGRA8Unorm;

    // Pipeline
    WGPUShaderModule    shaderModule    = nullptr;
    WGPURenderPipeline  pipeline        = nullptr;
    WGPUPipelineLayout  pipelineLayout  = nullptr;
    WGPUBindGroupLayout bindGroupLayout = nullptr;
    WGPUBindGroup       bindGroup       = nullptr;

    // Resources
    WGPUBuffer      vertexBuffer       = nullptr;
    WGPUBuffer      uniformBuffer      = nullptr;
    WGPUTexture     noiseTexture       = nullptr;
    WGPUTextureView noiseTextureView   = nullptr;
    WGPUSampler     noiseSampler       = nullptr;
    WGPUTexture     starmapTexture     = nullptr;
    WGPUTextureView starmapTextureView = nullptr;
    WGPUSampler     starmapSampler     = nullptr;

    // Frame state
    WGPUCommandEncoder    encoder          = nullptr;
    WGPURenderPassEncoder passEncoder      = nullptr;
    WGPUTextureView       currentTextureView = nullptr;

    int surfaceWidth  = 0;
    int surfaceHeight = 0;
};

// ── Constructor / Destructor ─────────────────────────────────────────────────

WebGPURenderer::WebGPURenderer() : m_impl(std::make_unique<Impl>()) {}

WebGPURenderer::~WebGPURenderer() {
    if (!m_impl) return;

    // Release WebGPU resources in reverse order of creation
    if (m_impl->bindGroup)       wgpuBindGroupRelease(m_impl->bindGroup);
    if (m_impl->pipeline)        wgpuRenderPipelineRelease(m_impl->pipeline);
    if (m_impl->pipelineLayout)  wgpuPipelineLayoutRelease(m_impl->pipelineLayout);
    if (m_impl->bindGroupLayout) wgpuBindGroupLayoutRelease(m_impl->bindGroupLayout);
    if (m_impl->shaderModule)    wgpuShaderModuleRelease(m_impl->shaderModule);
    if (m_impl->vertexBuffer)    wgpuBufferRelease(m_impl->vertexBuffer);
    if (m_impl->uniformBuffer)   wgpuBufferRelease(m_impl->uniformBuffer);
    if (m_impl->noiseSampler)    wgpuSamplerRelease(m_impl->noiseSampler);
    if (m_impl->noiseTextureView) wgpuTextureViewRelease(m_impl->noiseTextureView);
    if (m_impl->noiseTexture)    wgpuTextureRelease(m_impl->noiseTexture);
    if (m_impl->starmapSampler)  wgpuSamplerRelease(m_impl->starmapSampler);
    if (m_impl->starmapTextureView) wgpuTextureViewRelease(m_impl->starmapTextureView);
    if (m_impl->starmapTexture)  wgpuTextureRelease(m_impl->starmapTexture);
    if (m_impl->surface)         wgpuSurfaceRelease(m_impl->surface);
    if (m_impl->device)          wgpuDeviceRelease(m_impl->device);
    if (m_impl->adapter)         wgpuAdapterRelease(m_impl->adapter);
    if (m_impl->instance)        wgpuInstanceRelease(m_impl->instance);
}

// ── init ─────────────────────────────────────────────────────────────────────

void WebGPURenderer::init(int width, int height, void* glfwWindow) {
    width_  = width;
    height_ = height;
    m_impl->surfaceWidth  = width;
    m_impl->surfaceHeight = height;
    m_impl->window = static_cast<GLFWwindow*>(glfwWindow);

    LOG_INFO("WebGPURenderer::init ({}x{})", width, height);

    // 1. Create WebGPU instance
    m_impl->instance = wgpuCreateInstance(nullptr);
    if (!m_impl->instance) {
        throw std::runtime_error("Failed to create WebGPU instance");
    }

    // 2. Get surface from GLFW (Emscripten provides glfwGetWGPUSurface)
    m_impl->surface = glfwGetWGPUSurface(m_impl->instance, m_impl->window);
    if (!m_impl->surface) {
        throw std::runtime_error("Failed to get WebGPU surface from GLFW window");
    }

    // 3. Request adapter (synchronous under Emscripten's Dawn implementation)
    {
        WGPURequestAdapterOptions options{};
        options.compatibleSurface = m_impl->surface;
        options.powerPreference   = WGPUPowerPreference_HighPerformance;

        struct AdapterData {
            WGPUAdapter adapter = nullptr;
            bool done = false;
        } adapterData;

        wgpuInstanceRequestAdapter(
            m_impl->instance, &options,
            [](WGPURequestAdapterStatus status, WGPUAdapter adapter, const char* message, void* userdata) {
                auto* data = static_cast<AdapterData*>(userdata);
                if (status == WGPURequestAdapterStatus_Success) {
                    data->adapter = adapter;
                } else {
                    LOG_ERROR("Failed to request WebGPU adapter: {}", message ? message : "unknown");
                }
                data->done = true;
            },
            &adapterData);

        // Under Emscripten, the callback fires synchronously
        if (!adapterData.adapter) {
            throw std::runtime_error("Failed to obtain WebGPU adapter");
        }
        m_impl->adapter = adapterData.adapter;
    }

    // 4. Request device
    {
        WGPUDeviceDescriptor deviceDesc{};
        deviceDesc.label = "AstrocoreDevice";

        struct DeviceData {
            WGPUDevice device = nullptr;
            bool done = false;
        } deviceData;

        wgpuAdapterRequestDevice(
            m_impl->adapter, &deviceDesc,
            [](WGPURequestDeviceStatus status, WGPUDevice device, const char* message, void* userdata) {
                auto* data = static_cast<DeviceData*>(userdata);
                if (status == WGPURequestDeviceStatus_Success) {
                    data->device = device;
                } else {
                    LOG_ERROR("Failed to request WebGPU device: {}", message ? message : "unknown");
                }
                data->done = true;
            },
            &deviceData);

        if (!deviceData.device) {
            throw std::runtime_error("Failed to obtain WebGPU device");
        }
        m_impl->device = deviceData.device;
    }

    // Set up uncaptured error callback for debugging
    wgpuDeviceSetUncapturedErrorCallback(
        m_impl->device,
        [](WGPUErrorType type, const char* message, void*) {
            LOG_ERROR("WebGPU error (type {}): {}", static_cast<int>(type), message ? message : "unknown");
        },
        nullptr);

    // 5. Get queue
    m_impl->queue = wgpuDeviceGetQueue(m_impl->device);

    // 6. Configure surface
    {
        WGPUSurfaceConfiguration config{};
        config.device      = m_impl->device;
        config.format      = m_impl->surfaceFormat;
        config.usage       = WGPUTextureUsage_RenderAttachment;
        config.alphaMode   = WGPUCompositeAlphaMode_Auto;
        config.width       = static_cast<uint32_t>(width);
        config.height      = static_cast<uint32_t>(height);
        config.presentMode = WGPUPresentMode_Fifo;
        wgpuSurfaceConfigure(m_impl->surface, &config);
    }

    // 7. Create shader module from WGSL
    {
        std::string wgslSource = loadShaderSource("shaders/planet_web.wgsl");
        if (wgslSource.empty()) {
            throw std::runtime_error("Failed to load planet_web.wgsl shader");
        }

        WGPUShaderModuleWGSLDescriptor wgslDesc{};
        wgslDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
        wgslDesc.code        = wgslSource.c_str();

        WGPUShaderModuleDescriptor shaderDesc{};
        shaderDesc.nextInChain = &wgslDesc.chain;
        shaderDesc.label       = "PlanetShader";

        m_impl->shaderModule = wgpuDeviceCreateShaderModule(m_impl->device, &shaderDesc);
        if (!m_impl->shaderModule) {
            throw std::runtime_error("Failed to create WebGPU shader module");
        }
        LOG_INFO("WGSL shader module created");
    }

    // 8. Create bind group layout (5 entries matching WGSL bindings)
    {
        std::array<WGPUBindGroupLayoutEntry, 5> entries{};

        // @binding(0): uniform buffer (PlanetUniforms, 544 bytes)
        entries[0].binding               = 0;
        entries[0].visibility            = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
        entries[0].buffer.type           = WGPUBufferBindingType_Uniform;
        entries[0].buffer.minBindingSize = sizeof(PlanetUniforms);

        // @binding(1): texture_3d<f32> (noise)
        entries[1].binding               = 1;
        entries[1].visibility            = WGPUShaderStage_Fragment;
        entries[1].texture.sampleType    = WGPUTextureSampleType_Float;
        entries[1].texture.viewDimension = WGPUTextureViewDimension_3D;

        // @binding(2): sampler (noise)
        entries[2].binding          = 2;
        entries[2].visibility       = WGPUShaderStage_Fragment;
        entries[2].sampler.type     = WGPUSamplerBindingType_Filtering;

        // @binding(3): texture_cube<f32> (starmap)
        entries[3].binding               = 3;
        entries[3].visibility            = WGPUShaderStage_Fragment;
        entries[3].texture.sampleType    = WGPUTextureSampleType_Float;
        entries[3].texture.viewDimension = WGPUTextureViewDimension_Cube;

        // @binding(4): sampler (starmap)
        entries[4].binding          = 4;
        entries[4].visibility       = WGPUShaderStage_Fragment;
        entries[4].sampler.type     = WGPUSamplerBindingType_Filtering;

        WGPUBindGroupLayoutDescriptor layoutDesc{};
        layoutDesc.label      = "PlanetBindGroupLayout";
        layoutDesc.entryCount = entries.size();
        layoutDesc.entries    = entries.data();

        m_impl->bindGroupLayout = wgpuDeviceCreateBindGroupLayout(m_impl->device, &layoutDesc);
    }

    // 9. Create pipeline layout
    {
        WGPUPipelineLayoutDescriptor pipelineLayoutDesc{};
        pipelineLayoutDesc.label                = "PlanetPipelineLayout";
        pipelineLayoutDesc.bindGroupLayoutCount = 1;
        pipelineLayoutDesc.bindGroupLayouts     = &m_impl->bindGroupLayout;

        m_impl->pipelineLayout = wgpuDeviceCreatePipelineLayout(m_impl->device, &pipelineLayoutDesc);
    }

    // 10. Create render pipeline
    {
        // Vertex attributes: position (vec2f) at location 0, uv (vec2f) at location 1
        std::array<WGPUVertexAttribute, 2> vertexAttribs{};
        vertexAttribs[0].format         = WGPUVertexFormat_Float32x2;
        vertexAttribs[0].offset         = 0;
        vertexAttribs[0].shaderLocation = 0;
        vertexAttribs[1].format         = WGPUVertexFormat_Float32x2;
        vertexAttribs[1].offset         = 2 * sizeof(float);
        vertexAttribs[1].shaderLocation = 1;

        WGPUVertexBufferLayout vertexBufferLayout{};
        vertexBufferLayout.arrayStride    = 4 * sizeof(float); // 16 bytes stride
        vertexBufferLayout.stepMode       = WGPUVertexStepMode_Vertex;
        vertexBufferLayout.attributeCount = vertexAttribs.size();
        vertexBufferLayout.attributes     = vertexAttribs.data();

        // Vertex state
        WGPUVertexState vertexState{};
        vertexState.module      = m_impl->shaderModule;
        vertexState.entryPoint  = "vs_main";
        vertexState.bufferCount = 1;
        vertexState.buffers     = &vertexBufferLayout;

        // Fragment state
        WGPUColorTargetState colorTarget{};
        colorTarget.format    = m_impl->surfaceFormat;
        colorTarget.writeMask = WGPUColorWriteMask_All;

        WGPUFragmentState fragmentState{};
        fragmentState.module      = m_impl->shaderModule;
        fragmentState.entryPoint  = "fs_main";
        fragmentState.targetCount = 1;
        fragmentState.targets     = &colorTarget;

        // Primitive state
        WGPUPrimitiveState primitiveState{};
        primitiveState.topology  = WGPUPrimitiveTopology_TriangleList;
        primitiveState.frontFace = WGPUFrontFace_CCW;
        primitiveState.cullMode  = WGPUCullMode_None;

        // Multisample state
        WGPUMultisampleState multisampleState{};
        multisampleState.count = 1;
        multisampleState.mask  = 0xFFFFFFFF;

        WGPURenderPipelineDescriptor pipelineDesc{};
        pipelineDesc.label       = "PlanetRenderPipeline";
        pipelineDesc.layout      = m_impl->pipelineLayout;
        pipelineDesc.vertex      = vertexState;
        pipelineDesc.fragment    = &fragmentState;
        pipelineDesc.primitive   = primitiveState;
        pipelineDesc.multisample = multisampleState;

        m_impl->pipeline = wgpuDeviceCreateRenderPipeline(m_impl->device, &pipelineDesc);
        if (!m_impl->pipeline) {
            throw std::runtime_error("Failed to create WebGPU render pipeline");
        }
        LOG_INFO("WebGPU render pipeline created");
    }

    // 11. Create vertex buffer (fullscreen quad, 6 vertices)
    {
        auto quadVerts    = getQuadVertices();
        size_t bufferSize = quadVerts.size() * sizeof(float);

        WGPUBufferDescriptor bufDesc{};
        bufDesc.label            = "QuadVertexBuffer";
        bufDesc.size             = bufferSize;
        bufDesc.usage            = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
        bufDesc.mappedAtCreation = true;

        m_impl->vertexBuffer = wgpuDeviceCreateBuffer(m_impl->device, &bufDesc);
        void* mapped = wgpuBufferGetMappedRange(m_impl->vertexBuffer, 0, bufferSize);
        std::memcpy(mapped, quadVerts.data(), bufferSize);
        wgpuBufferUnmap(m_impl->vertexBuffer);
    }

    // 12. Create uniform buffer (544 bytes)
    {
        WGPUBufferDescriptor bufDesc{};
        bufDesc.label = "PlanetUniformBuffer";
        bufDesc.size  = sizeof(PlanetUniforms);
        bufDesc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;

        m_impl->uniformBuffer = wgpuDeviceCreateBuffer(m_impl->device, &bufDesc);
    }

    // 13. Create 3D noise texture (512^3, R8Unorm)
    {
        constexpr uint32_t sz = 512;
        auto noiseData = generateNoiseData();

        WGPUTextureDescriptor texDesc{};
        texDesc.label         = "NoiseTexture3D";
        texDesc.dimension     = WGPUTextureDimension_3D;
        texDesc.size          = {sz, sz, sz};
        texDesc.format        = WGPUTextureFormat_R8Unorm;
        texDesc.mipLevelCount = 1;
        texDesc.sampleCount   = 1;
        texDesc.usage         = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;

        m_impl->noiseTexture = wgpuDeviceCreateTexture(m_impl->device, &texDesc);

        // Upload noise data via queue write
        WGPUImageCopyTexture dest{};
        dest.texture  = m_impl->noiseTexture;
        dest.mipLevel = 0;
        dest.origin   = {0, 0, 0};

        WGPUTextureDataLayout layout{};
        layout.offset       = 0;
        layout.bytesPerRow  = sz;       // 1 byte per texel (R8)
        layout.rowsPerImage = sz;

        WGPUExtent3D extent = {sz, sz, sz};
        wgpuQueueWriteTexture(m_impl->queue, &dest, noiseData.data(), noiseData.size(), &layout, &extent);

        // Create texture view
        WGPUTextureViewDescriptor viewDesc{};
        viewDesc.label           = "NoiseTextureView3D";
        viewDesc.format          = WGPUTextureFormat_R8Unorm;
        viewDesc.dimension       = WGPUTextureViewDimension_3D;
        viewDesc.mipLevelCount   = 1;
        viewDesc.arrayLayerCount = 1;
        m_impl->noiseTextureView = wgpuTextureCreateView(m_impl->noiseTexture, &viewDesc);

        // Sampler: linear filtering, repeat wrap
        WGPUSamplerDescriptor sampDesc{};
        sampDesc.label        = "NoiseSampler";
        sampDesc.addressModeU = WGPUAddressMode_Repeat;
        sampDesc.addressModeV = WGPUAddressMode_Repeat;
        sampDesc.addressModeW = WGPUAddressMode_Repeat;
        sampDesc.magFilter    = WGPUFilterMode_Linear;
        sampDesc.minFilter    = WGPUFilterMode_Linear;
        sampDesc.mipmapFilter = WGPUMipmapFilterMode_Nearest;
        m_impl->noiseSampler  = wgpuDeviceCreateSampler(m_impl->device, &sampDesc);

        LOG_INFO("3D noise texture created ({}^3, R8Unorm)", sz);
    }

    // 14. Create cubemap starmap texture (6 faces from PNG)
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
            std::string path = RendererBase::resolveAssetPath(faceFiles[f]);
            facePixels[f] = stbi_load(path.c_str(), &faceW, &faceH, &faceChannels, 4);
            if (!facePixels[f]) {
                LOG_WARN("Failed to load starmap face: {} -- using black fallback", faceFiles[f]);
                if (f == 0) { faceW = 1; faceH = 1; }
                facePixels[f] = static_cast<stbi_uc*>(calloc(
                    static_cast<size_t>(faceW) * faceH * 4, 1));
            }
        }

        uint32_t fw = static_cast<uint32_t>(faceW);
        uint32_t fh = static_cast<uint32_t>(faceH);
        size_t faceBytes = static_cast<size_t>(faceW) * faceH * 4;

        // Create cubemap texture (2D array with 6 layers)
        WGPUTextureDescriptor texDesc{};
        texDesc.label         = "StarmapCubemap";
        texDesc.dimension     = WGPUTextureDimension_2D;
        texDesc.size          = {fw, fh, 6};
        texDesc.format        = WGPUTextureFormat_RGBA8Unorm;
        texDesc.mipLevelCount = 1;
        texDesc.sampleCount   = 1;
        texDesc.usage         = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;

        m_impl->starmapTexture = wgpuDeviceCreateTexture(m_impl->device, &texDesc);

        // Upload each face
        for (int f = 0; f < 6; f++) {
            WGPUImageCopyTexture dest{};
            dest.texture  = m_impl->starmapTexture;
            dest.mipLevel = 0;
            dest.origin   = {0, 0, static_cast<uint32_t>(f)};

            WGPUTextureDataLayout layout{};
            layout.offset       = 0;
            layout.bytesPerRow  = fw * 4;
            layout.rowsPerImage = fh;

            WGPUExtent3D extent = {fw, fh, 1};
            wgpuQueueWriteTexture(m_impl->queue, &dest, facePixels[f], faceBytes, &layout, &extent);

            stbi_image_free(facePixels[f]);
        }

        // Cubemap texture view
        WGPUTextureViewDescriptor viewDesc{};
        viewDesc.label           = "StarmapCubemapView";
        viewDesc.format          = WGPUTextureFormat_RGBA8Unorm;
        viewDesc.dimension       = WGPUTextureViewDimension_Cube;
        viewDesc.mipLevelCount   = 1;
        viewDesc.arrayLayerCount = 6;
        m_impl->starmapTextureView = wgpuTextureCreateView(m_impl->starmapTexture, &viewDesc);

        // Sampler: nearest filtering to keep stars sharp, clamp to edge
        WGPUSamplerDescriptor sampDesc{};
        sampDesc.label        = "StarmapSampler";
        sampDesc.addressModeU = WGPUAddressMode_ClampToEdge;
        sampDesc.addressModeV = WGPUAddressMode_ClampToEdge;
        sampDesc.addressModeW = WGPUAddressMode_ClampToEdge;
        sampDesc.magFilter    = WGPUFilterMode_Nearest;
        sampDesc.minFilter    = WGPUFilterMode_Nearest;
        sampDesc.mipmapFilter = WGPUMipmapFilterMode_Nearest;
        m_impl->starmapSampler = wgpuDeviceCreateSampler(m_impl->device, &sampDesc);

        LOG_INFO("Starmap cubemap loaded ({}x{}, 6 faces)", faceW, faceH);
    }

    // 15. Create bind group (5 bindings)
    {
        std::array<WGPUBindGroupEntry, 5> entries{};

        // @binding(0): uniform buffer
        entries[0].binding = 0;
        entries[0].buffer  = m_impl->uniformBuffer;
        entries[0].offset  = 0;
        entries[0].size    = sizeof(PlanetUniforms);

        // @binding(1): noise texture view
        entries[1].binding     = 1;
        entries[1].textureView = m_impl->noiseTextureView;

        // @binding(2): noise sampler
        entries[2].binding = 2;
        entries[2].sampler = m_impl->noiseSampler;

        // @binding(3): starmap texture view
        entries[3].binding     = 3;
        entries[3].textureView = m_impl->starmapTextureView;

        // @binding(4): starmap sampler
        entries[4].binding = 4;
        entries[4].sampler = m_impl->starmapSampler;

        WGPUBindGroupDescriptor bgDesc{};
        bgDesc.label      = "PlanetBindGroup";
        bgDesc.layout     = m_impl->bindGroupLayout;
        bgDesc.entryCount = entries.size();
        bgDesc.entries    = entries.data();

        m_impl->bindGroup = wgpuDeviceCreateBindGroup(m_impl->device, &bgDesc);
    }

    LOG_INFO("WebGPURenderer initialization complete");
}

// ── resize ───────────────────────────────────────────────────────────────────

void WebGPURenderer::resize(int width, int height) {
    if (width <= 0 || height <= 0) return;

    width_  = width;
    height_ = height;
    m_impl->surfaceWidth  = width;
    m_impl->surfaceHeight = height;

    // Reconfigure the surface with the new dimensions
    WGPUSurfaceConfiguration config{};
    config.device      = m_impl->device;
    config.format      = m_impl->surfaceFormat;
    config.usage       = WGPUTextureUsage_RenderAttachment;
    config.alphaMode   = WGPUCompositeAlphaMode_Auto;
    config.width       = static_cast<uint32_t>(width);
    config.height      = static_cast<uint32_t>(height);
    config.presentMode = WGPUPresentMode_Fifo;
    wgpuSurfaceConfigure(m_impl->surface, &config);

    LOG_INFO("WebGPURenderer resized to {}x{}", width, height);
}

// ── beginFrame ───────────────────────────────────────────────────────────────

void WebGPURenderer::beginFrame() {
    // Acquire the next surface texture
    WGPUSurfaceTexture surfaceTexture{};
    wgpuSurfaceGetCurrentTexture(m_impl->surface, &surfaceTexture);

    if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
        LOG_WARN("Failed to get current surface texture (status={})", static_cast<int>(surfaceTexture.status));
        return;
    }

    // Create a view of the surface texture
    WGPUTextureViewDescriptor viewDesc{};
    viewDesc.label = "SurfaceTextureView";
    m_impl->currentTextureView = wgpuTextureCreateView(surfaceTexture.texture, &viewDesc);

    // Create command encoder
    WGPUCommandEncoderDescriptor encDesc{};
    encDesc.label = "FrameCommandEncoder";
    m_impl->encoder = wgpuDeviceCreateCommandEncoder(m_impl->device, &encDesc);

    // Begin render pass
    WGPURenderPassColorAttachment colorAttach{};
    colorAttach.view       = m_impl->currentTextureView;
    colorAttach.loadOp     = WGPULoadOp_Clear;
    colorAttach.storeOp    = WGPUStoreOp_Store;
    colorAttach.clearValue = {0.0, 0.0, 0.0, 1.0};
#ifndef WEBGPU_BACKEND_WGPU
    colorAttach.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif

    WGPURenderPassDescriptor passDesc{};
    passDesc.label                = "MainRenderPass";
    passDesc.colorAttachmentCount = 1;
    passDesc.colorAttachments     = &colorAttach;

    m_impl->passEncoder = wgpuCommandEncoderBeginRenderPass(m_impl->encoder, &passDesc);
}

// ── render ───────────────────────────────────────────────────────────────────

void WebGPURenderer::render(const Camera& camera) {
    if (!m_impl->passEncoder) return;

    // Update uniforms
    advanceTime(0.016f); // TODO: pass real frame dt from Application
    PlanetUniforms u = fillUniforms(camera);
    wgpuQueueWriteBuffer(m_impl->queue, m_impl->uniformBuffer, 0, &u, sizeof(u));

    // Draw the fullscreen quad (6 verts * 4 floats * 4 bytes = 96 bytes)
    static constexpr size_t kVertexBufferSize = 6 * 4 * sizeof(float);

    wgpuRenderPassEncoderSetPipeline(m_impl->passEncoder, m_impl->pipeline);
    wgpuRenderPassEncoderSetBindGroup(m_impl->passEncoder, 0, m_impl->bindGroup, 0, nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(m_impl->passEncoder, 0, m_impl->vertexBuffer, 0, kVertexBufferSize);
    wgpuRenderPassEncoderDraw(m_impl->passEncoder, 6, 1, 0, 0);
}

// ── endFrame ─────────────────────────────────────────────────────────────────

void WebGPURenderer::endFrame() {
    if (!m_impl->passEncoder) return;

    wgpuRenderPassEncoderEnd(m_impl->passEncoder);
    wgpuRenderPassEncoderRelease(m_impl->passEncoder);
    m_impl->passEncoder = nullptr;

    WGPUCommandBufferDescriptor cmdDesc{};
    cmdDesc.label = "FrameCommandBuffer";
    WGPUCommandBuffer cmdBuffer = wgpuCommandEncoderFinish(m_impl->encoder, &cmdDesc);
    wgpuCommandEncoderRelease(m_impl->encoder);
    m_impl->encoder = nullptr;

    wgpuQueueSubmit(m_impl->queue, 1, &cmdBuffer);
    wgpuCommandBufferRelease(cmdBuffer);

    wgpuSurfacePresent(m_impl->surface);

    if (m_impl->currentTextureView) {
        wgpuTextureViewRelease(m_impl->currentTextureView);
        m_impl->currentTextureView = nullptr;
    }
}

// ── Accessors for ImGui integration ──────────────────────────────────────────

WGPUDevice WebGPURenderer::getDevice() {
    return m_impl->device;
}

WGPUTextureFormat WebGPURenderer::getSurfaceFormat() {
    return m_impl->surfaceFormat;
}

}  // namespace astrocore

#endif  // __EMSCRIPTEN__
