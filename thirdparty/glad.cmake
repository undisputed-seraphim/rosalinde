include(FetchContent)

FetchContent_Declare(glad
    URL "${CMAKE_CURRENT_SOURCE_DIR}/glad.zip"
    URL_HASH MD5=edeaf259d4eb9fa46236ebc45560936b
)
FetchContent_MakeAvailable(glad)
