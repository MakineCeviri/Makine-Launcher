# MakineAI - Turkish Game Translation Platform
# Super-build: C++ Core library + Qt/QML application
#
# Presets:
#   dev/debug  → UI-only (MinGW, fast iteration)
#   release    → Full build (vcpkg, Core + QML)
#   core       → Core library only (CI/testing)
#
# Copyright (c) 2026 MakineAI Team

cmake_minimum_required(VERSION 3.28)

# Build options (before project() for toolchain logic)
option(MAKINEAI_UI_ONLY "Build UI only without core library" OFF)
option(MAKINEAI_CORE_ONLY "Build core library only without UI" OFF)
option(MAKINEAI_BUILD_TESTS "Build unit tests" OFF)

# vcpkg integration (auto-detect when building core)
if(NOT MAKINEAI_UI_ONLY)
    if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "Vcpkg toolchain file")
    endif()
endif()

project(MakineAI
    VERSION 0.1.0
    DESCRIPTION "Turkish Game Translation Platform"
    LANGUAGES CXX
)

# Keep all executables in the build root (avoid add_subdirectory nesting)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Core C++ library
if(NOT MAKINEAI_UI_ONLY)
    set(BUILD_TESTING ${MAKINEAI_BUILD_TESTS})
    add_subdirectory(core)
endif()

# Qt/QML application
if(NOT MAKINEAI_CORE_ONLY)
    add_subdirectory(qml)
endif()

# Configuration summary
message(STATUS "")
message(STATUS "=== MakineAI Build Configuration ===")
message(STATUS "Version:  ${PROJECT_VERSION}")
message(STATUS "UI Only:  ${MAKINEAI_UI_ONLY}")
message(STATUS "Core Only: ${MAKINEAI_CORE_ONLY}")
message(STATUS "Tests:    ${MAKINEAI_BUILD_TESTS}")
message(STATUS "Build:    ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "====================================")
message(STATUS "")
