add_library(asblib STATIC asb.cpp)
target_include_directories(asblib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(asb_harvest "main.cpp")
target_link_libraries(asb_harvest PRIVATE asblib criware)
