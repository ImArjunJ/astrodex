cmake_minimum_required(VERSION 3.25)
project(AstroSplat VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── CMake modules ─────────────────────────────────────────────────────────────
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(CompilerOptions)
include(Dependencies)

# ── Vulkan ────────────────────────────────────────────────────────────────────
find_package(Vulkan REQUIRED)
include(ShaderCompilation)

# ── Core sources (unified — no conditional compilation) ──────────────────────
set(ASTROCORE_SOURCES
    src/core/Window.cpp
    src/core/Application.cpp
    src/core/Logger.cpp
    src/render/Camera.cpp
    src/render/VulkanRenderer.cpp
    src/ui/UIManager.cpp
)

add_library(astrocore_lib STATIC ${ASTROCORE_SOURCES})
target_include_directories(astrocore_lib PUBLIC ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(astrocore_lib
    PUBLIC
        glfw
        Vulkan::Vulkan
        glm::glm
        spdlog::spdlog
        imgui_impl
        vk-bootstrap::vk-bootstrap
        GPUOpen::VulkanMemoryAllocator
)

# ── Executable ────────────────────────────────────────────────────────────────
add_executable(astrosplat src/main.cpp)
target_link_libraries(astrosplat PRIVATE astrocore_lib)

# On macOS with Homebrew Vulkan/MoltenVK, ensure loader finds the ICD and dylib
if(APPLE)
    set_target_properties(astrosplat PROPERTIES
        BUILD_RPATH "/opt/homebrew/lib;/usr/local/lib"
    )
    # Generate a launch wrapper that sets the MoltenVK ICD path
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

# ── Compile shaders to SPIR-V ────────────────────────────────────────────────
compile_shaders(astrosplat
    ${CMAKE_SOURCE_DIR}/shaders/planet_vk.vert
    ${CMAKE_SOURCE_DIR}/shaders/planet_vk.frag
)

# ── Post-build: copy assets ──────────────────────────────────────────────────
add_custom_command(TARGET astrosplat POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:astrosplat>/assets
)

# ── Star Explorer ────────────────────────────────────────────────────────────
# Separate executable with its own Vulkan renderer for 3D star navigation.
# Compiles Window.cpp and Logger.cpp directly (not via astrocore_lib) to avoid
# duplicate VMA_IMPLEMENTATION / STB_IMAGE_IMPLEMENTATION symbols.

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
)

add_executable(starexplorer ${STAREXPLORER_SOURCES})
target_include_directories(starexplorer PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(starexplorer
    PRIVATE
        glfw
        Vulkan::Vulkan
        glm::glm
        spdlog::spdlog
        imgui_impl
        vk-bootstrap::vk-bootstrap
        GPUOpen::VulkanMemoryAllocator
)

if(APPLE)
    set_target_properties(starexplorer PROPERTIES
        BUILD_RPATH "/opt/homebrew/lib;/usr/local/lib"
    )
endif()

compile_shaders(starexplorer
    ${CMAKE_SOURCE_DIR}/shaders/star.vert
    ${CMAKE_SOURCE_DIR}/shaders/star.frag
)

add_custom_command(TARGET starexplorer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:starexplorer>/assets
)

# ── Star LOD Builder (offline preprocessing tool) ────────────────────────────
# C++ executable that reads gaia_raw.bin and produces gaia_multilod.bin
# with OpenMP-parallelized per-cell LOD assignment.

add_executable(star_lod_builder src/explorer/star_lod_builder.cpp)
target_compile_features(star_lod_builder PRIVATE cxx_std_17)

find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_link_libraries(star_lod_builder PRIVATE OpenMP::OpenMP_CXX)
endif()
