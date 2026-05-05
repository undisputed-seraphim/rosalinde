include(FetchContent)

set(DETEX_VALIDATE OFF CACHE BOOL "" FORCE)
set(DETEX_VIEW OFF CACHE BOOL "" FORCE)
set(DETEX_CONVERT OFF CACHE BOOL "" FORCE)

FetchContent_Declare(detex
    URL "${CMAKE_CURRENT_LIST_DIR}/detex-0.1.2.tar.xz"
    URL_HASH MD5=4954ac7aa89e65b05fee9df6dda34177
)
FetchContent_MakeAvailable(detex)