if (SDL3_FOUND)
    message(STATUS "Found SDL3 ${SDL3_VERSION}, not using vendored SDL3")
    return()
endif()

include(FetchContent)

FetchContent_Declare(SDL3
    URL "${CMAKE_CURRENT_SOURCE_DIR}/SDL-release-3.4.0.tar.xz"
    URL_HASH MD5=4906f6d72dee1614dbd7e52ab5809a40
    OVERRIDE_FIND_PACKAGE
)

set(SDL_STATIC CACHE BOOL ON)
FetchContent_MakeAvailable(SDL3)
