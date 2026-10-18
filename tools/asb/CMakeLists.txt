add_executable(asb_harvest "main.cpp")
target_link_libraries(asb_harvest PRIVATE scarlett criware)

add_executable(asb_run "run.cpp")
target_link_libraries(asb_run PRIVATE scarlett criware)

add_executable(asb_record "record.cpp")
target_link_libraries(asb_record PRIVATE scarlett criware)

add_executable(asb_trace "trace.cpp")
target_link_libraries(asb_trace PRIVATE scarlett criware)

add_executable(asbrun
	"asbrun.cpp"
	"asbrun_bulk.cpp"
	"asbrun_common.cpp"
	"asbrun_inspect.cpp"
	"asbrun_record.cpp"
	"asbrun_runtime.cpp"
	"asbrun_trace.cpp"
	"../line_editor.cpp"
)
target_include_directories(asbrun PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(asbrun PRIVATE scarlett criware eltolinde)
