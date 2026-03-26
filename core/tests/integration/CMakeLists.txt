# MakineLauncher Integration Tests
# =========================
#
# Integration tests verify complete workflows across multiple modules.
# These tests may require test fixtures (sample game files) to run.
#
# Build: cmake --build --preset core-debug --target makine_integration_tests
# Run: ctest -R integration
#

cmake_minimum_required(VERSION 3.28)

# Find dependencies
find_package(GTest CONFIG REQUIRED)

# Collect all integration test sources
set(INTEGRATION_TEST_SOURCES
    test_integration_main.cpp
    test_renpy_workflow.cpp
    test_rpgmaker_workflow.cpp
    test_unity_workflow.cpp
    test_gamemaker_workflow.cpp
    test_end_to_end.cpp
)

# Integration test executable
add_executable(makine_integration_tests
    ${INTEGRATION_TEST_SOURCES}
)

target_link_libraries(makine_integration_tests PRIVATE
    makine_core
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
)

target_include_directories(makine_integration_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../mocks
)

# Compile definitions
target_compile_definitions(makine_integration_tests PRIVATE
    MAKINE_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../fixtures"
    MAKINE_TEST_TEMP_DIR="${CMAKE_CURRENT_BINARY_DIR}/temp"
)

# Create temp directory for tests
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/temp")

# Enable CTest with integration label
include(GoogleTest)
gtest_discover_tests(makine_integration_tests
    TEST_PREFIX "integration."
    PROPERTIES LABELS "integration"
)

# Copy fixtures if they exist
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../fixtures")
    # Don't copy, use in-place to save space
    # Tests should use MAKINE_TEST_FIXTURES_DIR
endif()
