include(detex.cmake)
include(entt.cmake)
include(glad.cmake)
include(glm.cmake)
include(SDL3.cmake)

file(GLOB imgui_src "${CMAKE_CURRENT_SOURCE_DIR}/imgui-1.91.3/*")
add_library(imgui STATIC ${imgui_src})
target_link_libraries(imgui PUBLIC OpenGL::GL SDL3::SDL3-static)
target_include_directories(imgui PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/>
    $<INSTALL_INTERFACE:/>
)
