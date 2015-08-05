file(GLOB_RECURSE FRAMEWORK_SHADER_VERTEX RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.vert)
file(GLOB_RECURSE FRAMEWORK_SHADER_FRAGME RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.frag)

add_executable(UniverseVisualizer visualize_universe.cpp
              ${FRAMEWORK_SHADER_VERTEX}
              ${FRAMEWORK_SHADER_FRAGME})
#add_test(buffer-uniform COMMAND $<TARGET_FILE:UniverseVisualizer>)

target_link_libraries(UniverseVisualizer ${FRAMEWORK_NAME} ${BINARY_FILES})
add_dependencies(UniverseVisualizer glfw ${FRAMEWORK_NAME} ${COPY_BINARY})

install(TARGETS UniverseVisualizer DESTINATION .)
