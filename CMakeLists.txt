cmake_minimum_required(VERSION 3.25)
project(rosalinde VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATIONS ON)
#set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(Boost_USE_STATIC_LIBS ON)
find_package(glm CONFIG 1.0.0 REQUIRED)
find_package(ZLIB 1.3.1 REQUIRED)
find_package(Boost CONFIG 1.88 REQUIRED COMPONENTS system program_options iostreams)
find_package(SDL3 3.2 REQUIRED)
find_package(EnTT 3.14 REQUIRED)
find_package(OpenGL REQUIRED)

message(STATUS "Boost version ${Boost_VERSION}")
message(STATUS "glm version ${glm_VERSION}")
message(STATUS "SDL version ${SDL3_VERSION}")
message(STATUS "EnTT version ${EnTT_VERSION}")

add_subdirectory(thirdparty)

file(GLOB_RECURSE lib_src "src/*.*")
add_library(eltolinde STATIC ${lib_src})
target_link_libraries(eltolinde PUBLIC Boost::boost Boost::iostreams glm::glm ZLIB::ZLIB detex)
target_compile_definitions(eltolinde PUBLIC
    "GLM_FORCE_AVX2=1"
    "GLM_FORCE_SWIZZLE=1"
    "GLM_FORCE_EXPLICIT_CTOR=1"
    "GLM_FORCE_ALIGNED_GENTYPES=1"
    "GLM_FORCE_RADIANS=1"
    "GLM_ENABLE_EXPERIMENTAL=1"
)
target_include_directories(eltolinde PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:/src>
)

add_executable(rosalinde "tools/extractor.cpp")
target_link_libraries(rosalinde PRIVATE eltolinde Boost::program_options)

add_subdirectory(viewer)
