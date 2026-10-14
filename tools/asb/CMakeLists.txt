add_executable(asb_harvest "main.cpp")
target_link_libraries(asb_harvest PRIVATE scarlett criware)

add_executable(asb_run "run.cpp")
target_link_libraries(asb_run PRIVATE scarlett criware)
