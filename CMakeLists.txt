cmake_minimum_required(VERSION 3.25)
project(rosalinde VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATIONS ON)
#set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(ZLIB 1.3.1 REQUIRED)

file(GLOB_RECURSE lib_src "src/*.*")
add_library(eltolinde STATIC ${lib_src})
target_link_libraries(eltolinde PUBLIC ZLIB::ZLIB)
target_include_directories(eltolinde PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:/src>
)

add_executable(rosalinde "tools/extractor.cpp")
target_link_libraries(rosalinde PRIVATE eltolinde)

add_subdirectory(viewer)
