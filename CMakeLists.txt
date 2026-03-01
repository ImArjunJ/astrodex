cmake_minimum_required(VERSION 3.25)
project(AstroSplat VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Include CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(CompilerOptions)
include(Dependencies)

if(NOT EMSCRIPTEN)
    find_package(Vulkan REQUIRED)
    find_package(CURL REQUIRED)
    include(ShaderCompilation)
endif()

# Core sources (shared between all backends)
set(ASTROCORE_SOURCES
    src/core/Window.cpp
    src/core/Application.cpp
    src/core/Logger.cpp
    src/render/RendererBase.cpp
    src/render/Camera.cpp
    # GL-dependent renderers temporarily removed (will be ported to Vulkan later)
    # src/render/SphereRenderer.cpp
    # src/render/OrbitRenderer.cpp
    # src/render/RingRenderer.cpp
    # src/render/PlanetThumbnailRenderer.cpp
    src/ui/UIManager.cpp
    src/ui/GalaxyView.cpp
    src/ui/DataVisualization.cpp
    src/intro/IntroAnimation.cpp
    # Physics engine
    src/physics/CelestialBody.cpp
    src/physics/Octree.cpp
    src/physics/Integrator.cpp
    src/physics/PhysicsWorld.cpp
    # Simulation
    src/simulation/Simulation.cpp
    # Configuration
    src/config/SystemConfig.cpp
    src/config/PresetManager.cpp
    src/ui/CatalogueView.cpp
    # Data types (no curl dependency)
    src/data/ExoplanetData.cpp
    src/data/SolarSystemDatabase.cpp
    # Exoplanet rendering
    src/render/ExoplanetConverter.cpp
    src/render/PlanetParams.cpp
    src/render/ExoplanetMapper.cpp
)

# Network-dependent sources (require libcurl, desktop only)
if(NOT EMSCRIPTEN)
    list(APPEND ASTROCORE_SOURCES
        src/data/NasaApiClient.cpp
        src/data/OecClient.cpp
        src/data/GaiaClient.cpp
        src/data/CdsClient.cpp
        src/data/DataFusionEngine.cpp
        src/data/CacheManager.cpp
        src/data/ExoMastClient.cpp
        src/data/ExoplanetEuClient.cpp
        src/data/ExoplanetDataAggregator.cpp
        src/render/ThumbnailRenderer.cpp
        src/ai/InferenceEngine.cpp
        src/ai/BedrockClient.cpp
        src/ai/JimmyClient.cpp
        src/ai/GroqClient.cpp
    )
endif()

# Backend-specific renderer source
if(EMSCRIPTEN)
    list(APPEND ASTROCORE_SOURCES src/render/WebGPURenderer.cpp)
else()
    list(APPEND ASTROCORE_SOURCES src/render/VulkanRenderer.cpp)
endif()

add_library(astrocore_lib STATIC ${ASTROCORE_SOURCES})

target_include_directories(astrocore_lib
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

# Shared dependencies (available on all platforms)
target_link_libraries(astrocore_lib
    PUBLIC
        glm::glm
        spdlog::spdlog
        imgui_impl
        implot_impl
        nlohmann_json::nlohmann_json
        pugixml
)

if(NOT EMSCRIPTEN)
    target_link_libraries(astrocore_lib
        PUBLIC
            glfw
            Vulkan::Vulkan
            CURL::libcurl
            vk-bootstrap::vk-bootstrap
            GPUOpen::VulkanMemoryAllocator
    )
endif()

add_executable(astrodex src/main.cpp)
target_link_libraries(astrodex PRIVATE astrocore_lib)

if(EMSCRIPTEN)
    # Emscripten build flags
    target_link_options(astrodex PRIVATE
        -sUSE_WEBGPU=1
        -sUSE_GLFW=3
        -sALLOW_MEMORY_GROWTH=1
        --preload-file ${CMAKE_SOURCE_DIR}/assets@/assets
        --preload-file ${CMAKE_SOURCE_DIR}/shaders@/shaders
        --preload-file ${CMAKE_SOURCE_DIR}/configs@/configs
        --shell-file ${CMAKE_SOURCE_DIR}/web/shell.html
    )
    target_compile_options(astrodex PRIVATE -sUSE_WEBGPU=1 -sUSE_GLFW=3)
    target_compile_options(astrocore_lib PRIVATE -sUSE_WEBGPU=1 -sUSE_GLFW=3)
else()
    # On macOS with Homebrew Vulkan/MoltenVK, ensure loader finds the ICD and dylib
    if(APPLE)
        set_target_properties(astrodex PROPERTIES
            BUILD_RPATH "/opt/homebrew/lib;/usr/local/lib"
        )
        # Detect MoltenVK ICD
        set(_ICD_PATHS
            "/opt/homebrew/etc/vulkan/icd.d/MoltenVK_icd.json"
            "/usr/local/etc/vulkan/icd.d/MoltenVK_icd.json")
        foreach(_ICD ${_ICD_PATHS})
            if(EXISTS ${_ICD})
                set(MOLTENVK_ICD ${_ICD})
                break()
            endif()
        endforeach()
        if(MOLTENVK_ICD)
            message(STATUS "MoltenVK ICD found: ${MOLTENVK_ICD}")
        endif()
    endif()

    # Compile shaders to SPIR-V
    compile_shaders(astrodex
        ${CMAKE_SOURCE_DIR}/shaders/planet_vk.vert
        ${CMAKE_SOURCE_DIR}/shaders/planet_vk.frag
    )

    # Copy assets and configs
    add_custom_command(TARGET astrodex POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:astrodex>/assets
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/configs $<TARGET_FILE_DIR:astrodex>/configs
    )
endif()

# ── Tests ─────────────────────────────────────────────────────────────────────
if(NOT EMSCRIPTEN)
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.5.2
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(Catch2)
    add_subdirectory(tests)
endif()
