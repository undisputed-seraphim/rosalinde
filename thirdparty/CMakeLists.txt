find_package(SDL3 3.1 REQUIRED)
find_package(OpenGL REQUIRED)

file(GLOB imgui_src "${CMAKE_CURRENT_SOURCE_DIR}/imgui-1.91.3/*")
add_library(imgui STATIC ${imgui_src})
target_link_libraries(imgui PUBLIC OpenGL::GL SDL3::SDL3-static)
target_include_directories(imgui PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/>
    $<INSTALL_INTERFACE:/>
)

file(GLOB detex_src "${CMAKE_CURRENT_SOURCE_DIR}/detex-0.1.2alpha2/*")
add_library(detex STATIC ${detex_src})
target_include_directories(detex PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

include(FetchContent)
FetchContent_Declare(glad
    URL "${CMAKE_CURRENT_SOURCE_DIR}/glad.zip"
)

FetchContent_MakeAvailable(glad)