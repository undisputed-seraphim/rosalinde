cmake_minimum_required(VERSION 3.25)
project(rosalinde VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_VIEWER "Build the OpenGL viewer" ON)

list(PREPEND CMAKE_PREFIX_PATH "$ENV{HOME}/.local")

set(Boost_USE_STATIC_LIBS ON)

include(thirdparty/detex.cmake)
include(thirdparty/glm.cmake)

if(BUILD_VIEWER)
	include(thirdparty/SDL3.cmake)
	include(thirdparty/entt.cmake)
	include(thirdparty/glad.cmake)
	find_package(SDL3 3.4 REQUIRED)
	find_package(OpenGL REQUIRED)
	find_package(EnTT 3.14 REQUIRED)
endif()

find_package(ZLIB 1.3.1 REQUIRED)
find_package(Boost CONFIG 1.88 REQUIRED COMPONENTS system program_options iostreams)
find_package(glm CONFIG 1.0.0 REQUIRED)
find_package(Catch2 3 REQUIRED)

message(STATUS "Boost version ${Boost_VERSION}")
message(STATUS "glm version ${glm_VERSION}")
message(STATUS "Catch version ${Catch2_VERSION}")

if(BUILD_VIEWER)
	message(STATUS "SDL version ${SDL3_VERSION}")
	message(STATUS "EnTT version ${EnTT_VERSION}")
endif()

add_subdirectory(thirdparty)

message(STATUS "Boost version ${Boost_VERSION}")
message(STATUS "glm version ${glm_VERSION}")
message(STATUS "Catch version ${Catch2_VERSION}")

if(BUILD_VIEWER)
	message(STATUS "SDL version ${SDL3_VERSION}")
	message(STATUS "EnTT version ${EnTT_VERSION}")
endif()

add_subdirectory(libcriware)

file(GLOB_RECURSE lib_src "src/*.*")
add_library(eltolinde STATIC ${lib_src})
target_link_libraries(eltolinde PUBLIC Boost::boost Boost::iostreams glm::glm ZLIB::ZLIB detex criware)
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

if(BUILD_VIEWER)
	add_subdirectory(viewer)
	add_executable(acbplay "tools/acbplay.cpp")
	target_link_libraries(acbplay PRIVATE criware Boost::program_options SDL3::SDL3)
endif()
