# MakineAI C++ Native Core
# Copyright (c) 2026 MakineAI Team
# Modern C++20/23 game translation platform

cmake_minimum_required(VERSION 3.28)

project(MakineAI
    VERSION 0.1.0
    DESCRIPTION "Turkish Game Translation Platform - Native Core"
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

# Find required packages (existing)
find_package(Boost REQUIRED COMPONENTS filesystem)
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(lz4 CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(zstd CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

# Find new packages (from awesome-cpp research)
find_package(bit7z CONFIG QUIET)           # 7-zip C++ wrapper
find_package(LibArchive CONFIG QUIET)      # Multi-format archive (zip, tar, etc.)
find_package(simdjson CONFIG QUIET)        # Fast JSON parsing
find_package(efsw CONFIG QUIET)            # Filesystem watcher
find_package(mio CONFIG QUIET)             # Memory-mapped I/O
find_package(Taskflow CONFIG QUIET)        # Parallel task framework
find_package(unofficial-concurrentqueue CONFIG QUIET)  # Lock-free queue
find_package(simdutf CONFIG QUIET)         # SIMD UTF conversion
find_package(SQLiteCpp CONFIG QUIET)       # SQLite C++ wrapper
find_package(unofficial-sodium CONFIG QUIET)  # Modern crypto

# Build options
option(BUILD_TESTING "Build unit tests" OFF)

# Optional: Google Test for unit tests
if(BUILD_TESTING)
    find_package(GTest CONFIG)
endif()

# Include directories
set(MAKINEAI_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Source files
set(MAKINEAI_SOURCES
    # Core
    src/core.cpp

    # Config
    src/config/config.cpp

    # Database
    src/database/database.cpp

    # Asset Parsers
    src/asset_parser/asset_parser.cpp
    src/asset_parser/unity_bundle_parser.cpp
    src/asset_parser/unreal_pak_parser.cpp
    src/asset_parser/bethesda_ba2_parser.cpp
    src/asset_parser/gamemaker_data_parser.cpp

    # Patch Engine
    src/patch_engine/patch_engine.cpp

    # Game Detector
    src/game_detector/game_detector.cpp
    src/game_detector/steam_scanner.cpp
    src/game_detector/epic_scanner.cpp
    src/game_detector/gog_scanner.cpp

    # Package Manager
    src/package_manager/package_manager.cpp

    # Runtime (BepInEx/XUnity integration)
    src/runtime/runtime_manager.cpp

    # Security
    src/security/security_manager.cpp

    # Version Tracker
    src/version_tracker/version_tracker.cpp

    # Translation Services
    src/translation/translation_memory.cpp
    src/translation/glossary_service.cpp
    src/translation/qa_service.cpp
    src/translation/translation_api.cpp
    src/translation/string_classifier.cpp

    # Engine Handlers
    src/handlers/engine_handler_base.cpp
    src/handlers/unity_handler.cpp
    src/handlers/unreal_handler.cpp
    src/handlers/renpy_handler.cpp
    src/handlers/rpgmaker_handler.cpp
    src/handlers/gamemaker_handler.cpp

    # Translation Pipeline (Decision Engine)
    src/translation_pipeline/translation_pipeline.cpp

    # Recipe System
    src/recipe/recipe_loader.cpp

    # Platform (Windows)
    src/platform/windows/registry.cpp
    src/platform/windows/filesystem.cpp
)

# Header files (for IDE)
set(MAKINEAI_HEADERS
    include/makineai/core.hpp
    include/makineai/fwd.hpp
    include/makineai/pimpl.hpp
    include/makineai/config.hpp
    include/makineai/logging.hpp
    include/makineai/parallel.hpp
    include/makineai/mio_utils.hpp
    include/makineai/json_utils.hpp
    include/makineai/database.hpp
    include/makineai/asset_parser.hpp
    include/makineai/parsers_factory.hpp
    include/makineai/patch_engine.hpp
    include/makineai/game_detector.hpp
    include/makineai/package_manager.hpp
    include/makineai/runtime_manager.hpp
    include/makineai/security.hpp
    include/makineai/version_tracker.hpp
    include/makineai/translation_memory.hpp
    include/makineai/glossary_service.hpp
    include/makineai/qa_service.hpp
    include/makineai/translation_api.hpp
    include/makineai/string_classifier.hpp
    include/makineai/types.hpp
    include/makineai/error.hpp
    include/makineai/async.hpp
    include/makineai/cache.hpp
    include/makineai/validation.hpp
    include/makineai/metrics.hpp
    include/makineai/health.hpp
    include/makineai/debug.hpp
    include/makineai/string_pool.hpp
    include/makineai/audit.hpp
    include/makineai/version.hpp
    include/makineai/memory.hpp
    include/makineai/lazy.hpp
    include/makineai/batch.hpp
    include/makineai/scanner_base.hpp
    include/makineai/sandbox.hpp
    include/makineai/path_utils.hpp
    include/makineai/utf_utils.hpp
    include/makineai/sqlite_utils.hpp
    include/makineai/file_watcher.hpp
    include/makineai/crypto_utils.hpp
    include/makineai/archive_utils.hpp
    include/makineai/concurrent_queue.hpp

    # Modular Type Headers (Faz 3 refactor)
    include/makineai/types/common.hpp
    include/makineai/types/game_types.hpp
    include/makineai/types/patch_types.hpp
    include/makineai/types/translation_types.hpp
    include/makineai/types/tm_types.hpp
    include/makineai/types/pipeline_types.hpp

    # Engine Handlers
    include/makineai/handlers/engine_handler.hpp
    include/makineai/handlers/unity_handler.hpp
    include/makineai/handlers/unreal_handler.hpp
    include/makineai/handlers/renpy_handler.hpp
    include/makineai/handlers/rpgmaker_handler.hpp
    include/makineai/handlers/gamemaker_handler.hpp

    # Translation Pipeline
    include/makineai/translation_pipeline.hpp

    # Recipe System
    include/makineai/recipe_loader.hpp

    include/formats/unity_bundle.hpp
    include/formats/unreal_pak.hpp
    include/formats/bethesda_ba2.hpp
    include/formats/gamemaker_data.hpp
)

# Static library (for linking with FFI)
add_library(makineai_core STATIC ${MAKINEAI_SOURCES} ${MAKINEAI_HEADERS})

target_include_directories(makineai_core
    PUBLIC
        $<BUILD_INTERFACE:${MAKINEAI_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(makineai_core
    PUBLIC
        # Core dependencies (required)
        Boost::filesystem
        OpenSSL::SSL
        OpenSSL::Crypto
        CURL::libcurl
        nlohmann_json::nlohmann_json
        lz4::lz4
        ZLIB::ZLIB
        $<IF:$<TARGET_EXISTS:zstd::libzstd_shared>,zstd::libzstd_shared,zstd::libzstd_static>
        unofficial::sqlite3::sqlite3
        spdlog::spdlog
)

# Link new libraries (optional - graceful degradation if not found)
if(TARGET bit7z::bit7z)
    target_link_libraries(makineai_core PUBLIC bit7z::bit7z)
    target_compile_definitions(makineai_core PUBLIC MAKINEAI_HAS_BIT7Z)
    message(STATUS "bit7z found - 7-zip support enabled")
endif()

if(TARGET LibArchive::LibArchive)
    target_link_libraries(makineai_core PUBLIC LibArchive::LibArchive)
    target_compile_definitions(makineai_core PUBLIC MAKINEAI_HAS_LIBARCHIVE)
    message(STATUS "libarchive found - Multi-format archive support enabled")
endif()

if(TARGET simdjson::simdjson)
    target_link_libraries(makineai_core PUBLIC simdjson::simdjson)
    target_compile_definitions(makineai_core PUBLIC MAKINEAI_HAS_SIMDJSON)
    message(STATUS "simdjson found - Fast JSON parsing enabled")
endif()

if(TARGET efsw::efsw)
    target_link_libraries(makineai_core PUBLIC efsw::efsw)
    target_compile_definitions(makineai_core PUBLIC MAKINEAI_HAS_EFSW)
    message(STATUS "efsw found - Filesystem watcher enabled")
endif()

if(TARGET mio::mio)
    target_link_libraries(makineai_core PUBLIC mio::mio)
    target_compile_definitions(makineai_core PUBLIC MAKINEAI_HAS_MIO)
    message(STATUS "mio found - Memory-mapped I/O enabled")
endif()

if(TARGET Taskflow::Taskflow)
    target_link_libraries(makineai_core PUBLIC Taskflow::Taskflow)
    target_compile_definitions(makineai_core PUBLIC MAKINEAI_HAS_TASKFLOW)
    message(STATUS "Taskflow found - Parallel task framework enabled")
endif()

if(TARGET unofficial::concurrentqueue::concurrentqueue)
    target_link_libraries(makineai_core PUBLIC unofficial::concurrentqueue::concurrentqueue)
    target_compile_definitions(makineai_core PUBLIC MAKINEAI_HAS_CONCURRENTQUEUE)
    message(STATUS "concurrentqueue found - Lock-free queue enabled")
endif()

if(TARGET simdutf::simdutf)
    target_link_libraries(makineai_core PUBLIC simdutf::simdutf)
    target_compile_definitions(makineai_core PUBLIC MAKINEAI_HAS_SIMDUTF)
    message(STATUS "simdutf found - Fast UTF conversion enabled")
endif()

if(TARGET SQLiteCpp)
    target_link_libraries(makineai_core PUBLIC SQLiteCpp)
    target_compile_definitions(makineai_core PUBLIC MAKINEAI_HAS_SQLITECPP)
    message(STATUS "SQLiteCpp found - Modern SQLite wrapper enabled")
endif()

if(TARGET unofficial-sodium::sodium)
    target_link_libraries(makineai_core PUBLIC unofficial-sodium::sodium)
    target_compile_definitions(makineai_core PUBLIC MAKINEAI_HAS_SODIUM)
    message(STATUS "libsodium found - Modern crypto enabled")
endif()

# Windows-specific libraries
if(WIN32)
    target_link_libraries(makineai_core PUBLIC
        Shlwapi
        Advapi32
        User32
    )
endif()

# NOTE: FFI shared library removed - using Qt/QML integration instead

# Unit tests
if(GTest_FOUND)
    enable_testing()

    add_executable(makineai_tests
        tests/test_main.cpp
        tests/test_asset_parser.cpp
        tests/test_patch_engine.cpp
        tests/test_game_detector.cpp
        tests/test_security.cpp
        tests/test_version_tracker.cpp
    )

    target_link_libraries(makineai_tests PRIVATE
        makineai_core
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
    )

    target_include_directories(makineai_tests PRIVATE ${MAKINEAI_INCLUDE_DIR})

    include(GoogleTest)
    gtest_discover_tests(makineai_tests)
endif()

# Standalone test executables for real game testing
add_executable(test_undertale tests/test_undertale.cpp)
target_link_libraries(test_undertale PRIVATE makineai_core)
target_include_directories(test_undertale PRIVATE ${MAKINEAI_INCLUDE_DIR})

add_executable(test_unity tests/test_unity.cpp)
target_link_libraries(test_unity PRIVATE makineai_core)
target_include_directories(test_unity PRIVATE ${MAKINEAI_INCLUDE_DIR})

add_executable(test_translation_api tests/test_translation_api.cpp)
target_link_libraries(test_translation_api PRIVATE makineai_core)
target_include_directories(test_translation_api PRIVATE ${MAKINEAI_INCLUDE_DIR})

add_executable(test_game_scan tests/test_game_scan.cpp)
target_link_libraries(test_game_scan PRIVATE makineai_core)
target_include_directories(test_game_scan PRIVATE ${MAKINEAI_INCLUDE_DIR})

add_executable(test_string_classifier tests/test_string_classifier.cpp)
target_link_libraries(test_string_classifier PRIVATE makineai_core)
target_include_directories(test_string_classifier PRIVATE ${MAKINEAI_INCLUDE_DIR})

# Install targets
install(TARGETS makineai_core
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/ DESTINATION include)

# Print configuration summary
message(STATUS "")
message(STATUS "=== MakineAI C++ Core Configuration ===")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Tests: ${GTest_FOUND}")
message(STATUS "========================================")
message(STATUS "")
