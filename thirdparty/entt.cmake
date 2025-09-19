include(FetchContent)

FetchContent_Declare(entt
    URL "${CMAKE_CURRENT_SOURCE_DIR}/entt-3.15.0.tar.xz"
    URL_HASH MD5=cfcaf8164ed9882722b7576208ac1c6b
)
FetchContent_MakeAvailable(entt)
