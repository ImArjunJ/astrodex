# Dependencies.cmake — FetchContent declarations

include(FetchContent)

# ── GLM ───────────────────────────────────────────────────────────────────────
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE
)

# ── nlohmann/json ─────────────────────────────────────────────────────────────
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)

# ── spdlog ────────────────────────────────────────────────────────────────────
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
    GIT_SHALLOW    TRUE
)

# ── GLFW ─────────────────────────────────────────────────────────────────────
# Prefer the Homebrew-installed GLFW on macOS to skip source compilation
# entirely (install with: brew install glfw).  Fall back to FetchContent if not
# found — pinned to 3.3.9 which is compatible with Xcode 16 / macOS SDK 15.
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

FetchContent_MakeAvailable(glm nlohmann_json spdlog ${_FETCH_TARGETS})

# Normalize the GLFW target name: find_package gives glfw3::glfw,
# FetchContent gives glfw.  Create an alias so both cases look the same.
if(glfw3_FOUND AND NOT TARGET glfw)
    add_library(glfw ALIAS glfw3::glfw)
endif()

# ── GLAD (OpenGL only) ────────────────────────────────────────────────────────
if(NOT USE_METAL)
    FetchContent_Declare(
        glad
        GIT_REPOSITORY https://github.com/Dav1dde/glad.git
        GIT_TAG        v2.0.6
        GIT_SHALLOW    TRUE
    )
    FetchContent_GetProperties(glad)
    if(NOT glad_POPULATED)
        FetchContent_Populate(glad)
        add_subdirectory(${glad_SOURCE_DIR}/cmake ${glad_BINARY_DIR})
    endif()
    glad_add_library(glad_gl45_core REPRODUCIBLE API gl:core=4.5)
endif()

# ── Dear ImGui ────────────────────────────────────────────────────────────────
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.6-docking
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

# Common ImGui sources (always needed)
set(IMGUI_COMMON_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
)

if(USE_METAL)
    # Metal backend — .mm (Objective-C++) required
    list(APPEND IMGUI_COMMON_SOURCES
        ${imgui_SOURCE_DIR}/backends/imgui_impl_metal.mm
    )
else()
    # OpenGL3 backend
    list(APPEND IMGUI_COMMON_SOURCES
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
endif()

add_library(imgui_impl STATIC ${IMGUI_COMMON_SOURCES})

target_include_directories(imgui_impl
    PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui_impl PUBLIC glfw)

if(USE_METAL)
    find_library(METAL_FW_DEP      Metal      REQUIRED)
    find_library(QUARTZCORE_FW_DEP QuartzCore REQUIRED)
    target_link_libraries(imgui_impl PUBLIC ${METAL_FW_DEP} ${QUARTZCORE_FW_DEP})
    # Ensure Objective-C++ is used for the .mm file
    set_source_files_properties(
        ${imgui_SOURCE_DIR}/backends/imgui_impl_metal.mm
        PROPERTIES COMPILE_FLAGS "-fobjc-arc"
    )
endif()

# pybind11 (Phase 7)
# FetchContent_Declare(pybind11 ...)

# Catch2 (Phase 8)
# FetchContent_Declare(Catch2 ...)
