# scarlett: the AngelScript runtime (decoder + hosted VM) for the game's .asb.
add_library(scarlett STATIC
	src/asb.cpp
	src/host.cpp
	src/registry.cpp
)
target_include_directories(scarlett PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>
)
target_link_libraries(scarlett PUBLIC angelscript_addon spdlog::spdlog)

include(CTest)
list(APPEND CMAKE_MODULE_PATH "${Catch2_DIR}")
include(Catch)

file(GLOB_RECURSE scarlett_test_src "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
if(scarlett_test_src)
	add_executable(scarlett_test ${scarlett_test_src})
	target_link_libraries(scarlett_test PRIVATE scarlett criware Catch2::Catch2WithMain)
	catch_discover_tests(scarlett_test)
endif()
