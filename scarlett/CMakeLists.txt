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
target_link_libraries(scarlett PUBLIC angelscript_addon)
