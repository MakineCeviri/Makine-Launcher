# MakineLauncher C++ Native Core — Launcher Library
# Copyright (c) 2026 MakineCeviri Team
# Package catalog, crash recovery, file integrity

cmake_minimum_required(VERSION 3.28)

project(MakineLauncher
    VERSION 0.1.0
    DESCRIPTION "Turkish Game Translation Launcher - Native Core"
    LANGUAGES CXX
)

# C++23 standard (C++20 fallback)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Windows-specific settings
if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE)
    add_definitions(-DNOMINMAX)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# vcpkg integration (auto-detect toolchain)
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()


# ===== Dependencies =====
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

# Build options
option(BUILD_TESTING "Build unit tests" OFF)

# Optional: Google Test for unit tests
if(BUILD_TESTING)
    find_package(GTest CONFIG)
endif()

# Include directories
set(MAKINE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Source files. Only what the launcher calls into; the rest of the old core
# (detector, patch engine, database, runtime manager...) had no caller and
# was removed in 0.1.5-beta.
set(MAKINE_SOURCES
    src/package_catalog/package_catalog.cpp
    src/crash_recovery/crash_recovery.cpp
    src/file_integrity/file_integrity.cpp
)

# Header files (for IDE)
set(MAKINE_HEADERS
    include/makine/package_catalog.hpp
    include/makine/crash_recovery.hpp
    include/makine/file_integrity.hpp
    include/makine/path_utils.hpp
    include/makine/error.hpp
    include/makine/logging.hpp
    include/makine/types/common.hpp
    include/makine/plugin/plugin_api.h
    include/makine/plugin/plugin_types.h
)

# Static library
add_library(makine_core STATIC ${MAKINE_SOURCES} ${MAKINE_HEADERS})

# ===== Security Hardening =====
if(MSVC)
    target_compile_options(makine_core PRIVATE
        /GS           # Buffer security check (stack cookies)
        /guard:cf     # Control Flow Guard
        /sdl          # Additional security checks
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(makine_core PRIVATE
        -fstack-protector-strong     # Stack smashing protection
        -D_FORTIFY_SOURCE=2          # Buffer overflow detection
        -Wformat -Wformat-security   # Format string warnings
        -fPIE                        # Position-independent executable
    )
    # M-15: Linker hardening flags
    target_link_options(makine_core PRIVATE
        -Wl,-z,relro                 # Partial RELRO (read-only relocations)
        -Wl,-z,now                   # Full RELRO (resolve all symbols at load time)
        -Wl,-z,noexecstack           # Non-executable stack
    )
endif()

# Include directories:
# - core/include — launcher-specific headers

target_include_directories(makine_core
    PUBLIC
        $<BUILD_INTERFACE:${MAKINE_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(makine_core
    PUBLIC
        OpenSSL::Crypto
        nlohmann_json::nlohmann_json
        spdlog::spdlog
)

# Unit tests
if(GTest_FOUND)
    enable_testing()

    add_executable(makine_tests
        tests/test_main.cpp
        tests/test_error.cpp
        tests/test_path_utils.cpp
        tests/test_file_integrity.cpp
        tests/test_crash_recovery.cpp
        tests/test_package_catalog.cpp
    )

    target_link_libraries(makine_tests PRIVATE
        makine_core
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
    )

    target_include_directories(makine_tests PRIVATE ${MAKINE_INCLUDE_DIR})

    add_test(NAME makine_tests COMMAND makine_tests)
endif()

# Install targets
install(TARGETS makine_core
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/ DESTINATION include)

# Print configuration summary
message(STATUS "")
message(STATUS "=== MakineLauncher Core Configuration ===")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Tests: ${GTest_FOUND}")
message(STATUS "=============================================")
message(STATUS "")
