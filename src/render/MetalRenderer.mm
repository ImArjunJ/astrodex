#ifdef ASTRO_METAL

#include "render/MetalRenderer.hpp"
#include "render/Camera.hpp"
#include "core/Logger.hpp"

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <AppKit/AppKit.h>

#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <cstdint>
#include <cstring>

namespace astrocore {

// ── Uniform buffer layout ─────────────────────────────────────────────────────
// Must match PlanetUniforms struct in planet.metal byte-for-byte.
// All vec3 fields are padded to float4 (16 bytes) for GPU alignment.

struct alignas(16) PlanetUniformsMetal {
    // 64 bytes: float4x4
    float invView[16];

    // Groups of float4 (16 bytes each)
    float camPosX, camPosY, camPosZ, time;
    float planetX, planetY, planetZ, radius;
    float resX, resY, rotOffset, rotSpeed;
    float noiseStr, quality, terrainScale, domainWarp;
    float fbmPersist, fbmLac, fbmExp, fbmOct;   // fbmOct stored as float
    float ridged, crater, continent, waterLevel;
    float bandStr, bandFreq, polarCap, _pad0;
    float cloudDensity, cloudScale, cloudSpeed, cloudAlt;
    float cloudThick, sunInt, ambLight, atmoDensity;
    float atmoR,      atmoG,      atmoB,      _pad1;
    float sunDirX,    sunDirY,    sunDirZ,    _pad2;
    float sunColR,    sunColG,    sunColB,    _pad3;
    float deepSpR,    deepSpG,    deepSpB,    _pad4;
    float waterDeepR, waterDeepG, waterDeepB, _pad5;
    float waterSurfR, waterSurfG, waterSurfB, _pad6;
    float sandR,      sandG,      sandB,      _pad7;
    float treeR,      treeG,      treeB,      _pad8;
    float rockR,      rockG,      rockB,      _pad9;
    float iceR,       iceG,       iceB,       _pad10;
    float cloudColR,  cloudColG,  cloudColB,  _pad11;
    float sandLev,    treeLev,    rockLev,    iceLev;
    float transition, _pad12, _pad13, _pad14;
};

// ── Pimpl ─────────────────────────────────────────────────────────────────────

struct MetalRenderer::Impl {
    id<MTLDevice>              device          = nil;
    id<MTLCommandQueue>        commandQueue    = nil;
    id<MTLRenderPipelineState> planetPipeline  = nil;
    id<MTLTexture>             noiseTexture    = nil;
    id<MTLBuffer>              uniformBuffer   = nil;
    id<MTLBuffer>              vertexBuffer    = nil;
    id<MTLSamplerState>        noiseSampler    = nil;
    id<MTLDepthStencilState>   depthState      = nil;
    CAMetalLayer*              metalLayer      = nil;

    // Per-frame state (valid between beginFrame / endFrame)
    id<CAMetalDrawable>          currentDrawable    = nil;
    id<MTLCommandBuffer>         currentCmdBuf      = nil;
    MTLRenderPassDescriptor*     currentPassDesc     = nil;
    id<MTLRenderCommandEncoder>  currentEncoder      = nil;

    PlanetParams params;
    float        time   = 0.0f;
    int          width  = 0;
    int          height = 0;
};

// ── Constructor / destructor ──────────────────────────────────────────────────

MetalRenderer::MetalRenderer() : m_impl(std::make_unique<Impl>()) {}
MetalRenderer::~MetalRenderer() = default;

PlanetParams& MetalRenderer::params() { return m_impl->params; }

void* MetalRenderer::getMetalDevice() {
    return (__bridge void*)m_impl->device;
}

MetalFrameContext MetalRenderer::getMetalContext() {
    return {
        (__bridge void*)m_impl->currentCmdBuf,
        (__bridge void*)m_impl->currentEncoder,
        (__bridge void*)m_impl->currentPassDesc
    };
}

// ── init ─────────────────────────────────────────────────────────────────────

static id<MTLLibrary> loadMetalLibrary(id<MTLDevice> device, GLFWwindow* window) {
    // Resolve shader path relative to the executable
    NSString* exeDir = [[[NSBundle mainBundle] executablePath]
                        stringByDeletingLastPathComponent];
    NSString* shaderPath = [exeDir stringByAppendingPathComponent:@"shaders/planet.metal"];

    NSError* err = nil;
    NSString* src = [NSString stringWithContentsOfFile:shaderPath
                                              encoding:NSUTF8StringEncoding
                                                 error:&err];
    if (!src) {
        LOG_ERROR("Failed to read planet.metal at {}: {}",
                  [shaderPath UTF8String], [err.localizedDescription UTF8String]);
        return nil;
    }

    MTLCompileOptions* opts = [MTLCompileOptions new];
    id<MTLLibrary> lib = [device newLibraryWithSource:src options:opts error:&err];
    if (!lib) {
        LOG_ERROR("Metal shader compile error: {}", [err.localizedDescription UTF8String]);
        return nil;
    }
    return lib;
}

void MetalRenderer::init(int width, int height, void* glfwWindowPtr) {
    m_impl->width  = width;
    m_impl->height = height;

    // --- Device ---
    m_impl->device = MTLCreateSystemDefaultDevice();
    if (!m_impl->device) {
        LOG_ERROR("Failed to create Metal device");
        return;
    }
    LOG_INFO("Metal device: {}", [[m_impl->device name] UTF8String]);

    m_impl->commandQueue = [m_impl->device newCommandQueue];

    // --- CAMetalLayer on the GLFW NSWindow ---
    NSWindow* nsWin  = glfwGetCocoaWindow(static_cast<GLFWwindow*>(glfwWindowPtr));
    NSView*   nsView = [nsWin contentView];

    m_impl->metalLayer = [CAMetalLayer layer];
    m_impl->metalLayer.device        = m_impl->device;
    m_impl->metalLayer.pixelFormat   = MTLPixelFormatBGRA8Unorm;
    m_impl->metalLayer.framebufferOnly = YES;
    m_impl->metalLayer.frame         = nsView.bounds;
    m_impl->metalLayer.contentsScale = [NSScreen mainScreen].backingScaleFactor;
    m_impl->metalLayer.drawableSize  = CGSizeMake(width, height);

    [nsView setLayer:m_impl->metalLayer];
    [nsView setWantsLayer:YES];

    // --- Shaders ---
    id<MTLLibrary> lib = loadMetalLibrary(m_impl->device,
                                          static_cast<GLFWwindow*>(glfwWindowPtr));
    if (!lib) return;

    id<MTLFunction> vtxFn  = [lib newFunctionWithName:@"planetVertex"];
    id<MTLFunction> fragFn = [lib newFunctionWithName:@"planetFragment"];

    // --- Vertex descriptor ---
    // Quad vertices: float2 pos + float2 uv = 16 bytes/vertex, bound at index 1
    MTLVertexDescriptor* vtxDesc = [MTLVertexDescriptor new];
    vtxDesc.attributes[0].format      = MTLVertexFormatFloat2;
    vtxDesc.attributes[0].offset      = 0;
    vtxDesc.attributes[0].bufferIndex = 1;
    vtxDesc.attributes[1].format      = MTLVertexFormatFloat2;
    vtxDesc.attributes[1].offset      = 8;
    vtxDesc.attributes[1].bufferIndex = 1;
    vtxDesc.layouts[1].stride         = 16;
    vtxDesc.layouts[1].stepFunction   = MTLVertexStepFunctionPerVertex;

    // --- Render pipeline ---
    NSError* err = nil;
    MTLRenderPipelineDescriptor* pDesc = [MTLRenderPipelineDescriptor new];
    pDesc.vertexFunction                    = vtxFn;
    pDesc.fragmentFunction                  = fragFn;
    pDesc.colorAttachments[0].pixelFormat   = MTLPixelFormatBGRA8Unorm;
    pDesc.vertexDescriptor                  = vtxDesc;

    m_impl->planetPipeline = [m_impl->device newRenderPipelineStateWithDescriptor:pDesc
                                                                            error:&err];
    if (!m_impl->planetPipeline) {
        LOG_ERROR("Metal pipeline error: {}", [err.localizedDescription UTF8String]);
        return;
    }

    // --- Fullscreen quad ---
    float verts[] = {
        -1.f, -1.f,  0.f, 0.f,
         1.f, -1.f,  1.f, 0.f,
         1.f,  1.f,  1.f, 1.f,
        -1.f, -1.f,  0.f, 0.f,
         1.f,  1.f,  1.f, 1.f,
        -1.f,  1.f,  0.f, 1.f,
    };
    m_impl->vertexBuffer = [m_impl->device newBufferWithBytes:verts
                                                       length:sizeof(verts)
                                                      options:MTLResourceStorageModeShared];

    // --- Uniform buffer ---
    m_impl->uniformBuffer = [m_impl->device
        newBufferWithLength:sizeof(PlanetUniformsMetal)
                    options:MTLResourceStorageModeShared];

    // --- 3D noise texture (512^3) ---
    {
        const int sz = 512;
        LOG_INFO("Generating {}^3 noise texture for Metal...", sz);
        const size_t total = size_t(sz) * sz * sz;
        std::vector<uint8_t> data(total);
        uint32_t seed = 0x12345678u;
        for (size_t i = 0; i < total; ++i) {
            seed = seed * 1664525u + 1013904223u;
            data[i] = static_cast<uint8_t>(seed >> 24);
        }

        MTLTextureDescriptor* td = [MTLTextureDescriptor new];
        td.textureType  = MTLTextureType3D;
        td.pixelFormat  = MTLPixelFormatR8Unorm;
        td.width        = sz;
        td.height       = sz;
        td.depth        = sz;
        td.usage        = MTLTextureUsageShaderRead;
        td.storageMode  = MTLStorageModeShared;

        m_impl->noiseTexture = [m_impl->device newTextureWithDescriptor:td];
        MTLRegion region = MTLRegionMake3D(0, 0, 0, sz, sz, sz);
        [m_impl->noiseTexture replaceRegion:region
                                mipmapLevel:0
                                      slice:0
                                  withBytes:data.data()
                                bytesPerRow:sz
                              bytesPerImage:(NSUInteger)sz * sz];
        LOG_INFO("Metal 3D noise texture ready");
    }

    // --- Noise sampler ---
    MTLSamplerDescriptor* sd = [MTLSamplerDescriptor new];
    sd.minFilter    = MTLSamplerMinMagFilterLinear;
    sd.magFilter    = MTLSamplerMinMagFilterLinear;
    sd.sAddressMode = MTLSamplerAddressModeRepeat;
    sd.tAddressMode = MTLSamplerAddressModeRepeat;
    sd.rAddressMode = MTLSamplerAddressModeRepeat;
    m_impl->noiseSampler = [m_impl->device newSamplerStateWithDescriptor:sd];

    // --- Depth stencil (always-pass, no writes — fullscreen quad) ---
    MTLDepthStencilDescriptor* dd = [MTLDepthStencilDescriptor new];
    dd.depthCompareFunction = MTLCompareFunctionAlways;
    dd.depthWriteEnabled    = NO;
    m_impl->depthState = [m_impl->device newDepthStencilStateWithDescriptor:dd];

    LOG_INFO("MetalRenderer ready: {}x{}", width, height);
}

// ── resize ────────────────────────────────────────────────────────────────────

void MetalRenderer::resize(int width, int height) {
    m_impl->width  = width;
    m_impl->height = height;
    if (m_impl->metalLayer) {
        m_impl->metalLayer.drawableSize = CGSizeMake(width, height);
    }
}

// ── Frame lifecycle ───────────────────────────────────────────────────────────

void MetalRenderer::beginFrame() {
    m_impl->currentDrawable = [m_impl->metalLayer nextDrawable];
    if (!m_impl->currentDrawable) return;

    m_impl->currentCmdBuf = [m_impl->commandQueue commandBuffer];

    m_impl->currentPassDesc = [MTLRenderPassDescriptor renderPassDescriptor];
    m_impl->currentPassDesc.colorAttachments[0].texture     = m_impl->currentDrawable.texture;
    m_impl->currentPassDesc.colorAttachments[0].loadAction  = MTLLoadActionClear;
    m_impl->currentPassDesc.colorAttachments[0].clearColor  = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
    m_impl->currentPassDesc.colorAttachments[0].storeAction = MTLStoreActionStore;

    m_impl->currentEncoder = [m_impl->currentCmdBuf
        renderCommandEncoderWithDescriptor:m_impl->currentPassDesc];
}

void MetalRenderer::render(const Camera& camera) {
    if (!m_impl->currentEncoder) return;
    if (!m_impl->planetPipeline)  return;  // shader failed to compile — skip draw

    m_impl->time += 0.016f;

    // --- Build uniforms ---
    PlanetUniformsMetal u{};

    glm::mat4 inv = glm::inverse(camera.getViewMatrix());
    std::memcpy(u.invView, &inv[0][0], sizeof(u.invView));

    glm::vec3 cam = camera.getPosition();
    u.camPosX = cam.x;  u.camPosY = cam.y;  u.camPosZ = cam.z;
    u.time    = m_impl->time;

    u.planetX = 0.f;  u.planetY = 0.f;  u.planetZ = -10.f;
    u.radius  = m_impl->params.radius;

    u.resX = float(m_impl->width);  u.resY = float(m_impl->height);
    u.rotOffset = m_impl->params.rotationOffset;
    u.rotSpeed  = m_impl->params.rotationSpeed;

    u.noiseStr    = m_impl->params.noiseStrength;
    u.quality     = m_impl->params.quality;
    u.terrainScale= m_impl->params.terrainScale;
    u.domainWarp  = m_impl->params.domainWarpStrength;

    u.fbmPersist = m_impl->params.fbmPersistence;
    u.fbmLac     = m_impl->params.fbmLacunarity;
    u.fbmExp     = m_impl->params.fbmExponentiation;
    u.fbmOct     = float(m_impl->params.fbmOctaves);

    u.ridged     = m_impl->params.ridgedStrength;
    u.crater     = m_impl->params.craterStrength;
    u.continent  = m_impl->params.continentScale;
    u.waterLevel = m_impl->params.waterLevel;

    u.bandStr   = m_impl->params.bandingStrength;
    u.bandFreq  = m_impl->params.bandingFrequency;
    u.polarCap  = m_impl->params.polarCapSize;

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

    u.sandLev   = m_impl->params.sandLevel;
    u.treeLev   = m_impl->params.treeLevel;
    u.rockLev   = m_impl->params.rockLevel;
    u.iceLev    = m_impl->params.iceLevel;
    u.transition= m_impl->params.transition;

    std::memcpy(m_impl->uniformBuffer.contents, &u, sizeof(u));

    // --- Draw ---
    [m_impl->currentEncoder setRenderPipelineState:m_impl->planetPipeline];
    [m_impl->currentEncoder setDepthStencilState:m_impl->depthState];
    [m_impl->currentEncoder setVertexBuffer:m_impl->vertexBuffer  offset:0 atIndex:1];
    [m_impl->currentEncoder setFragmentBuffer:m_impl->uniformBuffer offset:0 atIndex:0];
    [m_impl->currentEncoder setFragmentTexture:m_impl->noiseTexture atIndex:0];
    [m_impl->currentEncoder setFragmentSamplerState:m_impl->noiseSampler atIndex:0];
    [m_impl->currentEncoder drawPrimitives:MTLPrimitiveTypeTriangle
                               vertexStart:0
                               vertexCount:6];
}

void MetalRenderer::endFrame() {
    if (!m_impl->currentEncoder) return;
    [m_impl->currentEncoder endEncoding];
    if (m_impl->currentDrawable) {
        [m_impl->currentCmdBuf presentDrawable:m_impl->currentDrawable];
    }
    [m_impl->currentCmdBuf commit];

    m_impl->currentEncoder  = nil;
    m_impl->currentCmdBuf   = nil;
    m_impl->currentDrawable = nil;
    m_impl->currentPassDesc = nil;
}

}  // namespace astrocore

#endif  // ASTRO_METAL
