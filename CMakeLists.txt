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
option(MAKINEAI_PROFILER "Enable Tracy profiler instrumentation" OFF)
option(MAKINEAI_CRASH_REPORTING "Enable Sentry crash reporting" OFF)

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

# Version suffix (change here only — propagates everywhere)
set(MAKINEAI_VERSION_SUFFIX "pre-alpha" CACHE STRING "Version pre-release suffix (empty for stable)")
if(MAKINEAI_VERSION_SUFFIX)
    set(MAKINEAI_VERSION_FULL "${PROJECT_VERSION}-${MAKINEAI_VERSION_SUFFIX}")
else()
    set(MAKINEAI_VERSION_FULL "${PROJECT_VERSION}")
endif()

# Keep all executables in the build root (avoid add_subdirectory nesting)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Tracy profiler (on-demand: zero overhead until profiler connects)
if(MAKINEAI_PROFILER)
    include(FetchContent)
    FetchContent_Declare(tracy
        GIT_REPOSITORY https://github.com/wolfpld/tracy.git
        GIT_TAG        v0.13.1
        GIT_SHALLOW    TRUE
    )
    set(TRACY_ENABLE ON CACHE BOOL "" FORCE)
    set(TRACY_ON_DEMAND ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(tracy)
    message(STATUS "Tracy profiler: ENABLED (on-demand)")
endif()

# Sentry crash reporting (breakpad backend for MinGW compatibility)
if(MAKINEAI_CRASH_REPORTING)
    include(FetchContent)
    FetchContent_Declare(sentry
        GIT_REPOSITORY https://github.com/getsentry/sentry-native.git
        GIT_TAG        0.9.1
        GIT_SHALLOW    TRUE
    )
    # Use breakpad backend for MinGW (crashpad requires MSVC)
    if(MINGW)
        set(SENTRY_BACKEND "breakpad" CACHE STRING "" FORCE)
    else()
        set(SENTRY_BACKEND "crashpad" CACHE STRING "" FORCE)
    endif()
    set(SENTRY_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SENTRY_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(sentry)
    message(STATUS "Sentry crash reporting: ENABLED (backend: ${SENTRY_BACKEND})")
endif()

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
message(STATUS "Version:  ${MAKINEAI_VERSION_FULL}")
message(STATUS "UI Only:  ${MAKINEAI_UI_ONLY}")
message(STATUS "Core Only: ${MAKINEAI_CORE_ONLY}")
message(STATUS "Tests:    ${MAKINEAI_BUILD_TESTS}")
message(STATUS "Profiler: ${MAKINEAI_PROFILER}")
message(STATUS "Sentry:   ${MAKINEAI_CRASH_REPORTING}")
message(STATUS "Build:    ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "====================================")
message(STATUS "")
