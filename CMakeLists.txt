# MakineLauncher - Turkish Game Translation Platform
# Super-build: C++ Core library + Qt/QML application
#
# Presets:
#   dev/debug  → UI-only (MinGW, fast iteration)
#   release    → Full build (vcpkg, Core + QML)
#   core       → Core library only (CI/testing)
#
# Copyright (c) 2026 MakineCeviri Team

cmake_minimum_required(VERSION 3.28)

# Build options (before project() for toolchain logic)
option(MAKINE_UI_ONLY "Build UI only without core library" OFF)
option(MAKINE_CORE_ONLY "Build core library only without UI" OFF)
option(MAKINE_BUILD_TESTS "Build unit tests" OFF)
option(MAKINE_PROFILER "Enable Tracy profiler instrumentation" OFF)
option(MAKINE_CRASH_REPORTING "Enable Sentry crash reporting" OFF)
option(MAKINE_RELEASE_VERIFIED "Enable release integrity checks (SSL pin validation, integrity service)" OFF)

# vcpkg integration (auto-detect when building core)
if(NOT MAKINE_UI_ONLY)
    if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "Vcpkg toolchain file")
    endif()
endif()

project(MakineLauncher
    VERSION 0.1.0
    DESCRIPTION "Turkish Game Translation Platform"
    LANGUAGES CXX
)

# Version suffix (change here only — propagates everywhere)
set(MAKINE_VERSION_SUFFIX "beta" CACHE STRING "Version pre-release suffix (empty for stable)")
if(MAKINE_VERSION_SUFFIX)
    set(MAKINE_VERSION_FULL "${PROJECT_VERSION}-${MAKINE_VERSION_SUFFIX}")
else()
    set(MAKINE_VERSION_FULL "${PROJECT_VERSION}")
endif()

# Keep all executables in the build root (avoid add_subdirectory nesting)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Tracy profiler (on-demand: zero overhead until profiler connects)
if(MAKINE_PROFILER)
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
if(MAKINE_CRASH_REPORTING)
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
if(NOT MAKINE_UI_ONLY)
    set(BUILD_TESTING ${MAKINE_BUILD_TESTS})
    if(MAKINE_BUILD_TESTS)
        enable_testing()
    endif()
    add_subdirectory(core)
endif()

# Qt/QML application
if(NOT MAKINE_CORE_ONLY)
    add_subdirectory(qml)
endif()

# Configuration summary
message(STATUS "")
message(STATUS "=== MakineLauncher Build Configuration ===")
message(STATUS "Version:  ${MAKINE_VERSION_FULL}")
message(STATUS "UI Only:  ${MAKINE_UI_ONLY}")
message(STATUS "Core Only: ${MAKINE_CORE_ONLY}")
message(STATUS "Tests:    ${MAKINE_BUILD_TESTS}")
message(STATUS "Profiler: ${MAKINE_PROFILER}")
message(STATUS "Sentry:   ${MAKINE_CRASH_REPORTING}")
message(STATUS "Verified: ${MAKINE_RELEASE_VERIFIED}")
message(STATUS "Build:    ${CMAKE_BUILD_TYPE}")

# Release integrity: SSL pin validation + integrity service (SEC-6)
if(MAKINE_RELEASE_VERIFIED)
    add_compile_definitions(MAKINE_RELEASE_VERIFIED)
    message(STATUS "Release verification: ENABLED (SSL pin check + integrity service)")
endif()

# Invariant: a distribution-shaped build (Release type, no DEV_TOOLS) without
# RELEASE_VERIFIED skips Authenticode + SSL pin + integrity service. Dev
# builds use DEV_TOOLS so they bypass this. Release/release-static must
# always set RELEASE_VERIFIED=ON — surface the gap loudly if they don't.
if(CMAKE_BUILD_TYPE STREQUAL "Release"
   AND NOT MAKINE_RELEASE_VERIFIED
   AND NOT MAKINE_DEV_TOOLS)
    message(WARNING
        "Distribution-shaped build (Release, no DEV_TOOLS) without "
        "MAKINE_RELEASE_VERIFIED=ON. Authenticode signature checks and "
        "SSL pin validation are DISABLED. This must NEVER be used for "
        "public distribution.")
endif()
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "====================================")
message(STATUS "")
