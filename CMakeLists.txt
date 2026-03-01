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

find_package(OpenGL REQUIRED)
find_package(CURL REQUIRED)

# Core sources
set(ASTROCORE_SOURCES
    src/core/Window.cpp
    src/core/Application.cpp
    src/core/Logger.cpp
    src/render/ShaderProgram.cpp
    src/render/Renderer.cpp
    src/render/Camera.cpp
    src/render/SphereRenderer.cpp
    src/render/OrbitRenderer.cpp
    src/render/RingRenderer.cpp
    src/render/PlanetThumbnailRenderer.cpp
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
    # Data / API
    src/data/ExoplanetData.cpp
    src/data/ExoplanetPredictor.cpp
    src/data/NasaApiClient.cpp
    src/data/ExoMastClient.cpp
    src/data/ExoplanetEuClient.cpp
    src/data/ExoplanetDataAggregator.cpp
    # Exoplanet rendering
    src/render/ExoplanetConverter.cpp
    # Star explorer (real Gaia star field for galaxy view)
    src/explorer/StarRenderer.cpp
    src/explorer/FreeFlyCamera.cpp
    src/explorer/StarData.cpp
    src/explorer/StarLOD.cpp
    # AI inference
    src/ai/InferenceEngine.cpp
    src/ai/BedrockClient.cpp
    src/ai/JimmyClient.cpp
    src/ai/GroqClient.cpp
)

add_library(astrocore_lib STATIC ${ASTROCORE_SOURCES})

target_include_directories(astrocore_lib
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(astrocore_lib
    PUBLIC
        glad_gl41_core
        glfw
        OpenGL::GL
        glm::glm
        spdlog::spdlog
        imgui_impl
        implot_impl
        nlohmann_json::nlohmann_json
        CURL::libcurl
)

add_executable(astrosplat src/main.cpp)
target_link_libraries(astrosplat PRIVATE astrocore_lib)

# Copy shaders, assets, and configs
add_custom_command(TARGET astrosplat POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:astrosplat>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:astrosplat>/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/configs $<TARGET_FILE_DIR:astrosplat>/configs
)

# ── Star Explorer ────────────────────────────────────────────────────────────
# Separate executable for 3D star navigation using OpenGL point rendering.
# Compiles shared sources (Window, Logger, ShaderProgram) directly to avoid
# pulling in the full astrocore_lib dependency chain.

set(STAREXPLORER_SOURCES
    src/explorer/main_explorer.cpp
    src/explorer/StarExplorerApp.cpp
    src/explorer/StarRenderer.cpp
    src/explorer/FreeFlyCamera.cpp
    src/explorer/StarData.cpp
    src/explorer/StarLOD.cpp
    src/explorer/ExplorerUI.cpp
    src/core/Window.cpp
    src/core/Logger.cpp
    src/render/ShaderProgram.cpp
)

add_executable(starexplorer ${STAREXPLORER_SOURCES})
target_include_directories(starexplorer PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(starexplorer
    PRIVATE
        glad_gl41_core
        glfw
        OpenGL::GL
        glm::glm
        spdlog::spdlog
        imgui_impl
)

add_custom_command(TARGET starexplorer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:starexplorer>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:starexplorer>/assets
)

# ── Star LOD Builder (offline preprocessing tool) ────────────────────────────
# Reads gaia_raw.bin and produces gaia_multilod.bin with per-cell LOD assignment.

add_executable(star_lod_builder src/explorer/star_lod_builder.cpp)
target_compile_features(star_lod_builder PRIVATE cxx_std_17)

find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_link_libraries(star_lod_builder PRIVATE OpenMP::OpenMP_CXX)
endif()
