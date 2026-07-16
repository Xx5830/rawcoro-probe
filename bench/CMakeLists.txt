message(STATUS "Bench is configurable")

file(GLOB_RECURSE BENCH_CPP CONFIGURE_DEPENDS "*.cpp")

add_executable(bench ${BENCH_CPP})

set(JSON_BuildTests OFF CACHE INTERNAL "")

message(STATUS "Start fetching nlohmann/json")
FetchContent_Declare(
        nlohmann
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
)

FetchContent_MakeAvailable(nlohmann)

message(STATUS "Finish fetching nlohmann/json")

target_link_libraries(bench PUBLIC asy)
target_link_libraries(bench PRIVATE nlohmann_json::nlohmann_json)

message(STATUS "Bench is ready")