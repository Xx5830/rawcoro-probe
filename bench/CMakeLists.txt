file(GLOB_RECURSE BENCH_CPP CONFIGURE_DEPENDS "*.cpp")

add_executable(bench ${BENCH_CPP})

set(JSON_BuildTests OFF CACHE INTERNAL "")

FetchContent_Declare(
        nlohmann
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
)

FetchContent_MakeAvailable(nlohmann)

set(JSON_BuildTests OFF CACHE INTERNAL "")

target_link_libraries(example PUBLIC async)
target_link_libraries(example PRIVATE nlohmann_json::nlohmann_json)
