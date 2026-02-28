# ShaderCompilation.cmake — compile GLSL shaders to SPIR-V via glslc

find_program(GLSLC glslc HINTS $ENV{VULKAN_SDK}/bin)
if(NOT GLSLC)
    message(FATAL_ERROR "glslc not found. Install the Vulkan SDK.")
endif()
message(STATUS "Found glslc: ${GLSLC}")

function(compile_shaders TARGET)
    set(SPIRV_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")
    file(MAKE_DIRECTORY ${SPIRV_OUTPUT_DIR})

    foreach(SHADER_SOURCE ${ARGN})
        get_filename_component(SHADER_NAME ${SHADER_SOURCE} NAME)
        set(SPIRV_OUTPUT "${SPIRV_OUTPUT_DIR}/${SHADER_NAME}.spv")

        add_custom_command(
            OUTPUT ${SPIRV_OUTPUT}
            COMMAND ${GLSLC} ${SHADER_SOURCE} -o ${SPIRV_OUTPUT}
            DEPENDS ${SHADER_SOURCE}
            COMMENT "Compiling ${SHADER_NAME} -> SPIR-V")

        list(APPEND SPIRV_OUTPUTS ${SPIRV_OUTPUT})
    endforeach()

    add_custom_target(${TARGET}_shaders DEPENDS ${SPIRV_OUTPUTS})
    add_dependencies(${TARGET} ${TARGET}_shaders)

    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${SPIRV_OUTPUT_DIR} $<TARGET_FILE_DIR:${TARGET}>/shaders)
endfunction()
