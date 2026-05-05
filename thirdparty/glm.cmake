include(FetchContent)

FetchContent_Declare(glm
    URL "${CMAKE_CURRENT_SOURCE_DIR}/glm-1.0.2.tar.xz"
    URL_HASH MD5=a26d7649311af93caf17d1aee615ace5
    OVERRIDE_FIND_PACKAGE
)
FetchContent_MakeAvailable(glm)
