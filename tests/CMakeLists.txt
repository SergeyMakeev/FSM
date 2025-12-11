cmake_minimum_required(VERSION 3.14)

# Fetch GoogleTest
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# Enable testing
enable_testing()

# Create test executable
add_executable(fsm_tests
    test_fsm_basic.cpp
    test_fsm_performance.cpp
    test_fsm_single_transition.cpp
)

# Link against FSM library and GoogleTest
target_link_libraries(fsm_tests
    PRIVATE
        FSM::FSM
        GTest::gtest_main
)

# Discover tests
include(GoogleTest)
gtest_discover_tests(fsm_tests)

