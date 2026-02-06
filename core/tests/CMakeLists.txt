# MakineAI Core Tests
cmake_minimum_required(VERSION 3.28)

# Find Google Test
find_package(GTest CONFIG REQUIRED)

# Test executable
add_executable(makineai_tests
    test_main.cpp
    test_game_detector.cpp
    test_asset_parser.cpp
    test_patch_engine.cpp
    test_security.cpp
    test_version_tracker.cpp
)

target_link_libraries(makineai_tests PRIVATE
    makineai_core
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
)

target_include_directories(makineai_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

# Enable CTest
include(GoogleTest)
gtest_discover_tests(makineai_tests)

# Copy test data if exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_data")
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/test_data" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
endif()
