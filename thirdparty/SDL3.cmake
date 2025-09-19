if (SDL3_FOUND)
    message(STATUS "Found SDL3 ${SDL3_VERSION}, not using vendored SDL3")
    return()
endif()

include(FetchContent)

FetchContent_Declare(SDL3
    URL "${CMAKE_CURRENT_SOURCE_DIR}/SDL3-3.2.20.tar.xz"
    URL_HASH MD5=9e0fa3620e2c7d2125db63fef842aaf0
    OVERRIDE_FIND_PACKAGE
)

set(SDL_STATIC CACHE BOOL ON)
FetchContent_MakeAvailable(SDL3)
