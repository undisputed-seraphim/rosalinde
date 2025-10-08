file(GLOB_RECURSE viewer_src "${CMAKE_CURRENT_SOURCE_DIR}/src/*")
add_executable(viewer ${viewer_src})
target_include_directories(viewer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(viewer PRIVATE eltolinde imgui glad glm::glm Boost::program_options)
