include(FetchContent)

if (NOT SDL3_FOUND)
    FetchContent_Declare(SDL3
        URL "${CMAKE_CURRENT_SOURCE_DIR}/SDL3-3.2.20.tar.xz"
        URL_HASH MD5=9e0fa3620e2c7d2125db63fef842aaf0
        CMAKE_ARGS "-DSDL_STATIC=ON"
        OVERRIDE_FIND_PACKAGE
    )
    FetchContent_MakeAvailable(SDL3)
endif()

file(GLOB imgui_src "${CMAKE_CURRENT_SOURCE_DIR}/imgui-1.91.3/*")
add_library(imgui STATIC ${imgui_src})
target_link_libraries(imgui PUBLIC OpenGL::GL SDL3::SDL3-static)
target_include_directories(imgui PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/>
    $<INSTALL_INTERFACE:/>
)

FetchContent_Declare(detex
    URL "${CMAKE_CURRENT_SOURCE_DIR}/detex-0.1.2.tar.xz"
    URL_HASH MD5=4954ac7aa89e65b05fee9df6dda34177
)
FetchContent_MakeAvailable(detex)

FetchContent_Declare(glad
    URL "${CMAKE_CURRENT_SOURCE_DIR}/glad.zip"
)
FetchContent_MakeAvailable(glad)