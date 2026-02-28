# Dependencies.cmake — FetchContent declarations (Vulkan unified backend)

include(FetchContent)

# ── GLM ───────────────────────────────────────────────────────────────────────
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE
)

# ── nlohmann/json ─────────────────────────────────────────────────────────────
if(NOT TARGET nlohmann_json::nlohmann_json)
    add_library(nlohmann_json INTERFACE)
    add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
    target_include_directories(nlohmann_json INTERFACE
        "${CMAKE_SOURCE_DIR}/third_party"
    )
endif()

# ── spdlog ────────────────────────────────────────────────────────────────────
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
    GIT_SHALLOW    TRUE
)

# ── GLFW ──────────────────────────────────────────────────────────────────────
if(APPLE)
    find_package(glfw3 3.3 QUIET CONFIG
        HINTS /opt/homebrew/lib/cmake/glfw3
              /usr/local/lib/cmake/glfw3)
endif()

if(NOT glfw3_FOUND)
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.3.9
        GIT_SHALLOW    TRUE
    )
    set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
    list(APPEND _FETCH_TARGETS glfw)
endif()

# ── vk-bootstrap ──────────────────────────────────────────────────────────────
FetchContent_Declare(
    vk_bootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
    GIT_TAG        v1.3.290
    GIT_SHALLOW    TRUE
)
list(APPEND _FETCH_TARGETS vk_bootstrap)

# ── VulkanMemoryAllocator ─────────────────────────────────────────────────────
set(VMA_VULKAN_VERSION 1002000)    # Vulkan 1.2
FetchContent_Declare(
    VulkanMemoryAllocator
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG        v3.1.0
    GIT_SHALLOW    TRUE
)
list(APPEND _FETCH_TARGETS VulkanMemoryAllocator)

FetchContent_MakeAvailable(glm spdlog ${_FETCH_TARGETS})

# Normalize GLFW target name
if(glfw3_FOUND AND NOT TARGET glfw)
    add_library(glfw ALIAS glfw3::glfw)
endif()

# ── Dear ImGui (Vulkan backend) ──────────────────────────────────────────────
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.6-docking
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

add_library(imgui_impl STATIC ${IMGUI_SOURCES})
target_include_directories(imgui_impl
    PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_impl PUBLIC glfw Vulkan::Vulkan)

# ── pugixml — XML parsing for OEC ────────────────────────────────────────────
FetchContent_Declare(
    pugixml
    GIT_REPOSITORY https://github.com/zeux/pugixml.git
    GIT_TAG        v1.14
    GIT_SHALLOW    TRUE
)

# ── Catch2 — Unit testing framework ─────────────────────────────────────────
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(pugixml Catch2)
