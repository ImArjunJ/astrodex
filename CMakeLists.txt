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
    src/ui/UIManager.cpp
    # Physics engine
    src/physics/CelestialBody.cpp
    src/physics/Octree.cpp
    src/physics/Integrator.cpp
    src/physics/PhysicsWorld.cpp
    # Simulation
    src/simulation/Simulation.cpp
)

add_library(astrocore_lib STATIC ${ASTROCORE_SOURCES})

target_include_directories(astrocore_lib
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(astrocore_lib
    PUBLIC
        glad_gl45_core
        glfw
        OpenGL::GL
        glm::glm
        spdlog::spdlog
        imgui_impl
)

add_executable(astrosplat src/main.cpp)
target_link_libraries(astrosplat PRIVATE astrocore_lib)

# Copy shaders and assets
add_custom_command(TARGET astrosplat POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:astrosplat>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:astrosplat>/assets
)
