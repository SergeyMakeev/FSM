cmake_minimum_required(VERSION 3.14)
project(FSM VERSION 1.0.0 LANGUAGES CXX)

# Create the FSM header-only library
set(HEADERS
    fsm/fsm.h
)

add_library(FSM INTERFACE)
target_include_directories(FSM INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(FSM INTERFACE cxx_std_17)

# Add alias for consistent naming when used as a subproject
add_library(FSM::FSM ALIAS FSM)

# Option to build tests
option(FSM_BUILD_TESTS "Build FSM unit tests" ON)

if(FSM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation rules (optional, for installing the library)
install(TARGETS FSM
    EXPORT FSMTargets
    INCLUDES DESTINATION include
)

install(FILES ${HEADERS}
    DESTINATION include/fsm
)

install(EXPORT FSMTargets
    FILE FSMTargets.cmake
    NAMESPACE FSM::
    DESTINATION lib/cmake/FSM
)

