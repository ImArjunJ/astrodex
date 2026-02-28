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

# Detect display backends before loading dependencies (so Dependencies.cmake
# can skip GLFW/GLAD/ImGui on headless builds).
if(NOT USE_METAL)
    find_package(OpenGL)
endif()

include(Dependencies)

find_package(CURL)

# Use FetchContent to build curl if not found on system
if(NOT CURL_FOUND)
    message(STATUS "CURL not found, fetching from source")
    include(FetchContent)
    FetchContent_Declare(
        curl
        GIT_REPOSITORY https://github.com/curl/curl.git
        GIT_TAG        curl-8_5_0
        GIT_SHALLOW    TRUE
    )
    set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(CURL_USE_OPENSSL OFF CACHE BOOL "" FORCE)
    set(HTTP_ONLY ON CACHE BOOL "" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(CURL_DISABLE_LDAP ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_LDAPS ON CACHE BOOL "" FORCE)
    set(CURL_USE_LIBSSH2 OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(curl)
    set(CURL_FOUND TRUE)
endif()

# ── Core sources (always compiled — no display backend required) ─────────────
set(ASTROCORE_SOURCES
    src/core/Logger.cpp
    src/data/NasaApiClient.cpp
    src/data/OecClient.cpp
    src/data/GaiaClient.cpp
    src/data/CdsClient.cpp
    src/data/CoordinateMatcher.cpp
    src/data/DataFusionEngine.cpp
    src/data/CacheManager.cpp
    src/data/ExoplanetData.cpp
    src/data/SolarSystemDatabase.cpp
    src/ai/InferenceEngine.cpp
    src/ai/BedrockClient.cpp
    src/render/PlanetParams.cpp
    src/render/ExoplanetMapper.cpp
)

# ── Display-dependent sources (need GLFW / window system) ────────────────────
if(USE_METAL OR OpenGL_FOUND)
    list(APPEND ASTROCORE_SOURCES
        src/core/Window.cpp
        src/core/Application.cpp
        src/render/Camera.cpp
        src/ui/UIManager.cpp
    )
endif()

# ── Backend-specific sources ─────────────────────────────────────────────────
if(USE_METAL)
    list(APPEND ASTROCORE_SOURCES src/render/MetalRenderer.mm)
elseif(OpenGL_FOUND)
    list(APPEND ASTROCORE_SOURCES
        src/render/ShaderProgram.cpp
        src/render/Renderer.cpp
    )
endif()

add_library(astrocore_lib STATIC ${ASTROCORE_SOURCES})

target_include_directories(astrocore_lib PUBLIC ${CMAKE_SOURCE_DIR}/src)

# ── Core link libraries (always linked) ──────────────────────────────────────
target_link_libraries(astrocore_lib
    PUBLIC
        glm::glm
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        CURL::libcurl
        pugixml
)

# ── Display backend libraries ────────────────────────────────────────────────
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
            imgui_impl
            ${METAL_FW}
            ${METALKIT_FW}
            ${FOUNDATION_FW}
            ${QUARTZCORE_FW}
            ${APPKIT_FW}
    )
elseif(OpenGL_FOUND)
    target_link_libraries(astrocore_lib
        PUBLIC
            glad_gl45_core
            glfw
            OpenGL::GL
            imgui_impl
    )
endif()

# ── Objective-C++ compilation for Metal translation units ─────────────────────
if(USE_METAL)
    set_source_files_properties(src/ui/UIManager.cpp
        PROPERTIES LANGUAGE OBJCXX)

    foreach(_src src/render/MetalRenderer.mm src/ui/UIManager.cpp)
        set_source_files_properties(${_src}
            PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    endforeach()
endif()

# ── Executable (only with a display backend) ─────────────────────────────────
if(USE_METAL OR OpenGL_FOUND)
    add_executable(astrosplat src/main.cpp)
    target_link_libraries(astrosplat PRIVATE astrocore_lib)

    add_custom_command(TARGET astrosplat POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:astrosplat>/shaders
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:astrosplat>/assets
    )
endif()

# ── Tests ────────────────────────────────────────────────────────────────────
if(EXISTS ${CMAKE_SOURCE_DIR}/tests/CMakeLists.txt)
    add_subdirectory(tests)
endif()
