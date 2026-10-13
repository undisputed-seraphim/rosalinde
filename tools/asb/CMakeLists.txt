add_executable(asb_harvest "main.cpp")
target_link_libraries(asb_harvest PRIVATE eltolinde)

add_executable(asb_run "run.cpp")
target_link_libraries(asb_run PRIVATE eltolinde angelscript_addon)
