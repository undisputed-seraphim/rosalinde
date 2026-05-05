file(GLOB_RECURSE criware_src "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_library(criware STATIC ${criware_src})
target_include_directories(criware PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

include(CTest)
list(APPEND CMAKE_MODULE_PATH "${Catch2_DIR}")
include(Catch)

file(GLOB_RECURSE criware_src_test "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
add_executable(criware_test ${criware_src_test})
target_link_libraries(criware_test PRIVATE criware Catch2::Catch2WithMain)
catch_discover_tests(criware_test)