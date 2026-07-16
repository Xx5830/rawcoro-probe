

file(GLOB_RECURSE TESTS_CPP CONFIGURE_DEPENDS "*.cpp")

if(TESTS_CPP)
message(STATUS "Tests is configurable")
message(STATUS "Start fetching googletest")

FetchContent_Declare(
    ggtest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)


FetchContent_MakeAvailable(ggtest)

message(STATUS "Finish fetching googletest")
    add_executable(tests ${TESTS_CPP})
    target_link_libraries(tests PRIVATE asy gtest_main)

    enable_testing(ON)

    include(GoogleTest)

    gtest_discover_tests(tests)
    message(STATUS "Tests is ready")
else()
message(STATUS "Tests is Off, it's not configurable")
endif()