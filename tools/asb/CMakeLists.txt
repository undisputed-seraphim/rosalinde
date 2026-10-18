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
