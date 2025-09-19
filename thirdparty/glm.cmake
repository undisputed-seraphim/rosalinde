if (glm_FOUND)
    message(STATUS "Found GLM ${glm_VERSION}, notusing vendored GLM")
    return()
endif()

include(FetchContent)

FetchContent_Declare(glm
    URL "${CMAKE_CURRENT_SOURCE_DIR}/glm-1.0.1-light.tar.xz"
    URL_HASH MD5=00324ba06260860c834e80f5e429f8b2
)
FetchContent_MakeAvailable(glm)
