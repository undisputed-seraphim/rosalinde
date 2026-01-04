file(GLOB_RECURSE criware_src "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_library(criware STATIC ${criware_src})
target_include_directories(criware PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")