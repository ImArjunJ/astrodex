# CompilerOptions.cmake - C++23 compiler flags and warnings

# Set compiler-specific flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Wno-unused-parameter
    )

    # Enable debug symbols in debug builds
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")

    # Optimize for release
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")

    # Enable AddressSanitizer in debug builds (optional)
    option(ASTROCORE_ENABLE_ASAN "Enable AddressSanitizer" OFF)
    if(ASTROCORE_ENABLE_ASAN)
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    endif()

elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    add_compile_options(
        /W4
        /permissive-
        /Zc:__cplusplus
    )
endif()

# Position Independent Code for shared libraries
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
