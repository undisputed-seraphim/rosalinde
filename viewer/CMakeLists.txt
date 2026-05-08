file(GLOB_RECURSE viewer_src "${CMAKE_CURRENT_SOURCE_DIR}/src/*")
add_executable(viewer ${viewer_src})
target_include_directories(viewer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/thirdparty/imgui-1.91.3")
target_sources(viewer PRIVATE
	${IMGUI_DIR}/imgui.cpp
	${IMGUI_DIR}/imgui_draw.cpp
	${IMGUI_DIR}/imgui_tables.cpp
	${IMGUI_DIR}/imgui_widgets.cpp
	${IMGUI_DIR}/imgui_impl_sdl3.cpp
	${IMGUI_DIR}/imgui_impl_opengl3.cpp
)
target_include_directories(viewer PRIVATE ${IMGUI_DIR})

target_link_libraries(viewer PRIVATE eltolinde glad glm::glm Boost::program_options SDL3::SDL3)
