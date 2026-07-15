FetchContent_Declare(
    ggtest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)

FetchContent_MakeAvailable(ggtest)

file(GLOB_RECURSE TESTS_CPP CONFIGURE_DEPENDS "*.cpp")

add_executable(tests ${TESTS_CPP})
target_link_libraries(tests PRIVATE async gtest_main)

enable_testing(ON)

include(GoogleTest)

gtest_discover_tests(tests)