# MakineLauncher C++ Native Core — Launcher Library
# Copyright (c) 2026 MakineCeviri Team
# Game detection, package management, patching, security

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


# ===== Dependencies (formerly in makine-common) =====
# Find required packages
find_package(Boost REQUIRED COMPONENTS filesystem)
find_package(OpenSSL REQUIRED)
# Workaround: vcpkg curl static build references CURL::win32_winsock but doesn't define it
if(NOT TARGET CURL::win32_winsock)
    add_library(CURL::win32_winsock INTERFACE IMPORTED)
    set_target_properties(CURL::win32_winsock PROPERTIES INTERFACE_LINK_LIBRARIES "ws2_32")
endif()
find_package(CURL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(lz4 CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(zstd CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

# Launcher-specific optional packages
find_package(bit7z CONFIG QUIET)           # 7-zip C++ wrapper
find_package(LibArchive CONFIG QUIET)      # Multi-format archive (zip, tar, etc.)
find_package(efsw CONFIG QUIET)            # Filesystem watcher
# Common optional packages (formerly in makine-common)
find_package(mio CONFIG QUIET)
find_package(Taskflow CONFIG QUIET)
find_package(unofficial-concurrentqueue CONFIG QUIET)
find_package(simdutf CONFIG QUIET)
find_package(simdjson CONFIG QUIET)
find_package(SQLiteCpp CONFIG QUIET)
find_package(unofficial-sodium CONFIG QUIET)

# Build options
option(BUILD_TESTING "Build unit tests" OFF)

# Optional: Google Test for unit tests
if(BUILD_TESTING)
    find_package(GTest CONFIG)
endif()

# Include directories
set(MAKINE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Source files — launcher only (intelligence code moved to Makine/engine)
set(MAKINE_SOURCES
    # Core
    src/core.cpp

    # Patch Engine
    src/patch_engine/patch_engine.cpp

    # Game Detector
    src/game_detector/game_detector.cpp
    src/game_detector/steam_scanner.cpp
    src/game_detector/epic_scanner.cpp
    src/game_detector/gog_scanner.cpp

    # Engine Detectors (extracted free functions)
    src/game_detector/engines/unity_detector.cpp
    src/game_detector/engines/unreal_detector.cpp
    src/game_detector/engines/renpy_detector.cpp
    src/game_detector/engines/rpgmaker_detector.cpp
    src/game_detector/engines/misc_detectors.cpp

    # Package Catalog (pure C++ catalog — network-based index + enrichment)
    src/package_catalog/package_catalog.cpp

    # Runtime Manager
    src/runtime/runtime_manager.cpp

    # Security
    src/security/security_manager.cpp
    src/security/credential_store.cpp
    src/security/ssl_pinning.cpp

    # File Integrity
    src/file_integrity/file_integrity.cpp

    # Crash Recovery
    src/crash_recovery/crash_recovery.cpp

    # Platform (Windows)
    src/platform/windows/registry.cpp
    src/platform/windows/filesystem.cpp
    
    # Common (formerly makine-common submodule)
    src/config/config.cpp
    src/database/database.cpp
)

# Header files (for IDE)
set(MAKINE_HEADERS
    include/makine/core.hpp
    include/makine/patch_engine.hpp
    include/makine/game_detector.hpp
    include/makine/vdf_parser.hpp
    include/makine/package_catalog.hpp
    include/makine/runtime_manager.hpp
    include/makine/security.hpp
    include/makine/ssl_pinning.hpp
    include/makine/credential_store.hpp
    include/makine/file_integrity.hpp
    include/makine/crash_recovery.hpp
    include/makine/detail/scoped_metrics.hpp
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

        # Launcher-specific dependencies
        CURL::libcurl
        
        # Core dependencies (formerly in makine-common)
        Boost::filesystem
        OpenSSL::SSL
        OpenSSL::Crypto
        nlohmann_json::nlohmann_json
        lz4::lz4
        ZLIB::ZLIB
        $<IF:$<TARGET_EXISTS:zstd::libzstd_shared>,zstd::libzstd_shared,zstd::libzstd_static>
        unofficial::sqlite3::sqlite3
        spdlog::spdlog
)

# Launcher-specific optional libraries
if(TARGET bit7z::bit7z)
    target_link_libraries(makine_core PUBLIC bit7z::bit7z)
    target_compile_definitions(makine_core PUBLIC MAKINE_HAS_BIT7Z)
    message(STATUS "bit7z found - 7-zip support enabled")
endif()

if(TARGET LibArchive::LibArchive)
    target_link_libraries(makine_core PUBLIC LibArchive::LibArchive)
    target_compile_definitions(makine_core PUBLIC MAKINE_HAS_LIBARCHIVE)
    message(STATUS "libarchive found - Multi-format archive support enabled")
endif()

if(TARGET efsw::efsw)
    target_link_libraries(makine_core PUBLIC efsw::efsw)
    target_compile_definitions(makine_core PUBLIC MAKINE_HAS_EFSW)
    message(STATUS "efsw found - Filesystem watcher enabled")
endif()

# Common optional libraries (formerly in makine-common)
if(TARGET mio::mio)
    target_link_libraries(makine_core PUBLIC mio::mio)
    target_compile_definitions(makine_core PUBLIC MAKINE_HAS_MIO)
endif()

if(TARGET Taskflow::Taskflow)
    target_link_libraries(makine_core PUBLIC Taskflow::Taskflow)
    target_compile_definitions(makine_core PUBLIC MAKINE_HAS_TASKFLOW)
endif()

if(TARGET unofficial::concurrentqueue::concurrentqueue)
    target_link_libraries(makine_core PUBLIC unofficial::concurrentqueue::concurrentqueue)
    target_compile_definitions(makine_core PUBLIC MAKINE_HAS_CONCURRENTQUEUE)
endif()

if(TARGET simdutf::simdutf)
    target_link_libraries(makine_core PUBLIC simdutf::simdutf)
    target_compile_definitions(makine_core PUBLIC MAKINE_HAS_SIMDUTF)
endif()

if(TARGET simdjson::simdjson)
    target_link_libraries(makine_core PUBLIC simdjson::simdjson)
    target_compile_definitions(makine_core PUBLIC MAKINE_HAS_SIMDJSON)
endif()

if(TARGET SQLiteCpp)
    target_link_libraries(makine_core PUBLIC SQLiteCpp)
    target_compile_definitions(makine_core PUBLIC MAKINE_HAS_SQLITECPP)
endif()

if(TARGET unofficial-sodium::sodium)
    target_link_libraries(makine_core PUBLIC unofficial-sodium::sodium)
    target_compile_definitions(makine_core PUBLIC MAKINE_HAS_SODIUM)
endif()

# Windows-specific libraries
if(WIN32)
    target_link_libraries(makine_core PUBLIC
        Shlwapi
        Advapi32
        Crypt32
        Wintrust
        User32
    )
endif()

# Unit tests
if(GTest_FOUND)
    enable_testing()

    add_executable(makine_tests
        tests/test_main.cpp
        tests/test_patch_engine.cpp
        tests/test_game_detector.cpp
        tests/test_security.cpp
        tests/test_config.cpp
        tests/test_database.cpp
        tests/test_path_utils.cpp
        tests/test_json_utils.cpp
        tests/test_validation.cpp
        tests/test_crypto_utils.cpp
        tests/test_file_integrity.cpp
        tests/test_crash_recovery.cpp
        tests/test_package_catalog.cpp
        tests/test_ssl_pinning.cpp
        tests/test_cache.cpp
        tests/test_parallel.cpp
        tests/test_sandbox.cpp
        tests/test_runtime_manager.cpp
        tests/test_error.cpp
        tests/test_utf_utils.cpp
        tests/test_version.cpp
        tests/test_memory.cpp
        tests/test_metrics.cpp
        tests/test_audit.cpp
        tests/test_string_pool.cpp
        tests/test_concurrent_queue.cpp
        tests/test_archive_utils.cpp
        tests/test_lazy.cpp
        tests/test_async.cpp
        tests/test_core.cpp
        tests/test_health.cpp
        tests/test_sqlite_utils.cpp
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
