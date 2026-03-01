# MakineAI Core Tests
# ====================
#
# Test structure:
# - Unit tests: Fast, isolated tests for individual components
# - Integration tests: Full workflow tests requiring game data/fixtures
# - Benchmarks: Performance tests (in benchmark/ subdirectory)
# - Mocks: Test doubles for external dependencies (in mocks/ directory)
# - Fuzz: Fuzz testing targets (in fuzz/ subdirectory)
#
cmake_minimum_required(VERSION 3.28)

# Find Google Test
find_package(GTest CONFIG REQUIRED)

# =============================================================================
# UNIT TESTS
# =============================================================================

add_executable(makineai_tests
    test_main.cpp
    test_game_detector.cpp
    test_patch_engine.cpp
    test_security.cpp
    test_config.cpp
    test_database.cpp
)

target_link_libraries(makineai_tests PRIVATE
    makineai_core
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
)

target_include_directories(makineai_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
)

# Compile definitions for test paths
target_compile_definitions(makineai_tests PRIVATE
    MAKINEAI_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    MAKINEAI_TEST_TEMP_DIR="${CMAKE_CURRENT_BINARY_DIR}/temp"
)

# Enable CTest
include(GoogleTest)
gtest_discover_tests(makineai_tests
    TEST_PREFIX "unit."
    PROPERTIES LABELS "unit"
)

# =============================================================================
# INTEGRATION TESTS
# =============================================================================
# NOTE: Game-specific integration tests (test_game_scan, test_disco_elysium,
# test_undertale, test_unity) are disabled — they require engine handler
# implementations that are deferred. Re-enable when handlers are ready.

# =============================================================================
# BENCHMARKS
# =============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/CMakeLists.txt")
    add_subdirectory(benchmark)
endif()

# =============================================================================
# TEST DATA
# =============================================================================

# Create temp directory for tests
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/temp")

# Copy test data if exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_data")
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/test_data" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
endif()

# =============================================================================
# CUSTOM TARGETS
# =============================================================================

# Run all tests
add_custom_target(run_all_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS makineai_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running all tests"
)

# Run unit tests only
add_custom_target(run_unit_tests
    COMMAND ${CMAKE_CTEST_COMMAND} -L unit --output-on-failure
    DEPENDS makineai_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running unit tests"
)
