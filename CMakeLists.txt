cmake_minimum_required(VERSION 3.25)
project(AstroSplat VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Metal backend flag ────────────────────────────────────────────────────────
# Automatically ON for Apple Silicon (arm64 macOS); override with -DUSE_METAL=OFF.

# Auto-detect Apple Silicon — always use Metal on arm64 macOS.
# Override: cmake -DUSE_METAL=OFF (must also delete CMakeCache.txt).
set(USE_METAL OFF CACHE BOOL "Use Metal rendering backend (Apple Silicon)")
if(APPLE)
    execute_process(
        COMMAND uname -m
        OUTPUT_VARIABLE HOST_ARCH
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(HOST_ARCH STREQUAL "arm64")
        set(USE_METAL ON CACHE BOOL "Metal backend auto-enabled on Apple Silicon" FORCE)
        message(STATUS "Apple Silicon (arm64) detected — Metal backend ENABLED")
    endif()
endif()

if(USE_METAL AND NOT APPLE)
    message(WARNING "USE_METAL=ON is only supported on macOS — ignoring.")
    set(USE_METAL OFF CACHE BOOL "" FORCE)
endif()

if(USE_METAL)
    message(STATUS "AstroSplat: Metal backend ENABLED (Apple Silicon)")
    # OBJC must be enabled alongside OBJCXX so CMake doesn't compile GLFW's .m
    # files as Objective-C++ (which applies C++ strict type rules and breaks them).
    enable_language(OBJC)
    enable_language(OBJCXX)
else()
    message(STATUS "AstroSplat: OpenGL backend")
endif()

# ── CMake modules ─────────────────────────────────────────────────────────────
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(CompilerOptions)
include(Dependencies)

if(NOT USE_METAL)
    find_package(OpenGL REQUIRED)
endif()

find_package(CURL REQUIRED)

# ── Core sources (always compiled) ───────────────────────────────────────────
set(ASTROCORE_SOURCES
    src/core/Window.cpp
    src/core/Application.cpp
    src/core/Logger.cpp
    src/render/Camera.cpp
    src/ui/UIManager.cpp
    # Data layer
    src/data/ExoplanetData.cpp
    src/data/NasaApiClient.cpp
    src/data/SolarSystemDatabase.cpp
    # AI / inference layer
    src/ai/BedrockClient.cpp
    src/ai/InferenceEngine.cpp
    # ML / render pipeline
    src/render/ExoplanetMapper.cpp
)

# ── Backend-specific sources ─────────────────────────────────────────────────
if(USE_METAL)
    list(APPEND ASTROCORE_SOURCES src/render/MetalRenderer.mm)
else()
    list(APPEND ASTROCORE_SOURCES
        src/render/ShaderProgram.cpp
        src/render/Renderer.cpp
    )
endif()

add_library(astrocore_lib STATIC ${ASTROCORE_SOURCES})

target_include_directories(astrocore_lib PUBLIC ${CMAKE_SOURCE_DIR}/src)

# ── Link libraries ────────────────────────────────────────────────────────────
if(USE_METAL)
    target_compile_definitions(astrocore_lib PUBLIC ASTRO_METAL)

    find_library(METAL_FW         Metal         REQUIRED)
    find_library(METALKIT_FW      MetalKit      REQUIRED)
    find_library(FOUNDATION_FW    Foundation    REQUIRED)
    find_library(QUARTZCORE_FW    QuartzCore    REQUIRED)
    find_library(APPKIT_FW        AppKit        REQUIRED)

    target_link_libraries(astrocore_lib
        PUBLIC
            glfw
            glm::glm
            spdlog::spdlog
            nlohmann_json::nlohmann_json
            CURL::libcurl
            imgui_impl
            ${METAL_FW}
            ${METALKIT_FW}
            ${FOUNDATION_FW}
            ${QUARTZCORE_FW}
            ${APPKIT_FW}
    )
else()
    target_link_libraries(astrocore_lib
        PUBLIC
            glad_gl45_core
            glfw
            OpenGL::GL
            glm::glm
            spdlog::spdlog
            nlohmann_json::nlohmann_json
            CURL::libcurl
            imgui_impl
    )
endif()

# ── Objective-C++ compilation for Metal translation units ─────────────────────
if(USE_METAL)
    # UIManager.cpp uses ObjC types from imgui_impl_metal.h — must compile as OBJCXX.
    set_source_files_properties(src/ui/UIManager.cpp
        PROPERTIES LANGUAGE OBJCXX)

    # Enable ARC on all Objective-C++ sources
    foreach(_src src/render/MetalRenderer.mm src/ui/UIManager.cpp)
        set_source_files_properties(${_src}
            PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    endforeach()
endif()

# ── Executable ────────────────────────────────────────────────────────────────
add_executable(astrosplat src/main.cpp)
target_link_libraries(astrosplat PRIVATE astrocore_lib)

# ── Post-build: copy shaders and assets ──────────────────────────────────────
add_custom_command(TARGET astrosplat POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:astrosplat>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:astrosplat>/assets
)
