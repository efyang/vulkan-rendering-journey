find_package(Vulkan REQUIRED)
set(GLSL_VALIDATOR Vulkan::glslangValidator)

file(GLOB_RECURSE GLSL_SOURCE_FILES
    "shaders/*.frag"
    "shaders/*.vert"
    )

foreach(GLSL ${GLSL_SOURCE_FILES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${PROJECT_BINARY_DIR}/assets/shaders/${FILE_NAME}.spv")
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/assets/shaders/"
        COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
        DEPENDS ${GLSL})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)
  
add_custom_target(
    Shaders 
    DEPENDS ${SPIRV_BINARY_FILES}
    )

# add_custom_command(TARGET YourMainTarget POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:YourMainTarget>/shaders/"
#     COMMAND ${CMAKE_COMMAND} -E copy_directory
#         "${PROJECT_BINARY_DIR}/shaders"
#         "$<TARGET_FILE_DIR:YourMainTarget>/shaders"
#         )