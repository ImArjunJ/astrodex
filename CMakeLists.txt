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

find_package(OpenGL)
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
    # CURL creates CURL::libcurl target automatically, no need for alias
endif()

# Core sources - data layer only for now (OpenGL sources excluded in headless mode)
set(ASTROCORE_SOURCES
    src/core/Logger.cpp
    src/data/NasaApiClient.cpp
    src/data/OecClient.cpp
    src/data/GaiaClient.cpp
    src/data/CdsClient.cpp
    src/data/ExoplanetData.cpp
)

# Add OpenGL-dependent sources only if OpenGL is available
if(OpenGL_FOUND)
    list(APPEND ASTROCORE_SOURCES
        src/core/Window.cpp
        src/core/Application.cpp
        src/render/ShaderProgram.cpp
        src/render/Renderer.cpp
        src/render/Camera.cpp
        src/ui/UIManager.cpp
    )
endif()

add_library(astrocore_lib STATIC ${ASTROCORE_SOURCES})

target_include_directories(astrocore_lib
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(astrocore_lib
    PUBLIC
        glm::glm
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        CURL::libcurl
        pugixml
)

# Link OpenGL-related libraries only if OpenGL is available
if(OpenGL_FOUND)
    target_link_libraries(astrocore_lib PUBLIC
        glad_gl45_core
        glfw
        OpenGL::GL
        imgui_impl
    )
endif()

# Only build the main executable if OpenGL is available
if(OpenGL_FOUND)
    add_executable(astrosplat src/main.cpp)
    target_link_libraries(astrosplat PRIVATE astrocore_lib)

    # Copy shaders and assets
    add_custom_command(TARGET astrosplat POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:astrosplat>/shaders
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:astrosplat>/assets
    )
endif()

# Tests
if(EXISTS ${CMAKE_SOURCE_DIR}/tests/CMakeLists.txt)
    add_subdirectory(tests)
endif()
