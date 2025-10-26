if (glm_FOUND)
    message(STATUS "Found GLM ${glm_VERSION}, notusing vendored GLM")
    return()
endif()

include(FetchContent)

FetchContent_Declare(glm
    URL "${CMAKE_CURRENT_SOURCE_DIR}/glm-1.0.2.tar.xz"
    URL_HASH MD5=a26d7649311af93caf17d1aee615ace5
)
FetchContent_MakeAvailable(glm)
