include(FetchContent)

set(OPUS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_SHARED_LIBRARY OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_PKG_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF CACHE BOOL "" FORCE)

set(_opus_saved_build_testing "${BUILD_TESTING}")
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

FetchContent_Declare(opus
	URL "${CMAKE_CURRENT_LIST_DIR}/opus-1.6.1.tar.xz"
	URL_HASH MD5=b8e7aadb9ced4227d109a52cf4534990
)

FetchContent_MakeAvailable(opus)

if(_opus_saved_build_testing STREQUAL "")
	unset(BUILD_TESTING CACHE)
else()
	set(BUILD_TESTING "${_opus_saved_build_testing}" CACHE BOOL "" FORCE)
endif()
unset(_opus_saved_build_testing)
