cmake_minimum_required(VERSION 3.25)

# UI service unit tests
# Pure C++ tests (no Qt runtime needed): VDF parser, tar helpers, CDN config
# Qt-dependent tests: version comparison, path security

# Support both standalone and subdirectory builds
if(NOT DEFINED MAKINE_PROJECT_ROOT)
    set(MAKINE_PROJECT_ROOT "${CMAKE_SOURCE_DIR}")
endif()

# These targets are standalone-buildable, so they do not inherit the project's
# language level. Without it test_cdnconfig fails to compile on MinGW GCC 13.1:
# std::string_view::starts_with/ends_with are C++20.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(GTest REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core)

enable_testing()

# Pure C++ tests (header-only, no vcpkg)
add_executable(test_vdfparser test_vdfparser.cpp)
target_include_directories(test_vdfparser PRIVATE ${MAKINE_PROJECT_ROOT}/qml/src/services)
target_link_libraries(test_vdfparser PRIVATE GTest::gtest GTest::gtest_main)
add_test(NAME test_vdfparser COMMAND test_vdfparser)

# Qt-dependent tests
add_executable(test_updateservice test_updateservice.cpp)
target_include_directories(test_updateservice PRIVATE ${MAKINE_PROJECT_ROOT}/qml/src/services)
target_link_libraries(test_updateservice PRIVATE GTest::gtest GTest::gtest_main Qt6::Core)
add_test(NAME test_updateservice COMMAND test_updateservice)

add_executable(test_pathsecurity test_pathsecurity.cpp)
target_include_directories(test_pathsecurity PRIVATE ${MAKINE_PROJECT_ROOT}/qml/src/services)
target_link_libraries(test_pathsecurity PRIVATE GTest::gtest GTest::gtest_main Qt6::Core)
add_test(NAME test_pathsecurity COMMAND test_pathsecurity)

add_executable(test_cdnconfig test_cdnconfig.cpp)
target_include_directories(test_cdnconfig PRIVATE ${MAKINE_PROJECT_ROOT}/qml/src/services)
target_link_libraries(test_cdnconfig PRIVATE GTest::gtest GTest::gtest_main Qt6::Core)
add_test(NAME test_cdnconfig COMMAND test_cdnconfig)

# CatalogStore tests (Qt-dependent, uses core path_utils.hpp header-only)
add_executable(test_catalogstore test_catalogstore.cpp
    ${MAKINE_PROJECT_ROOT}/qml/src/services/catalogstore.cpp)
target_include_directories(test_catalogstore PRIVATE
    ${MAKINE_PROJECT_ROOT}/qml/src/services
    ${MAKINE_PROJECT_ROOT}/core/include)
target_link_libraries(test_catalogstore PRIVATE GTest::gtest Qt6::Core)
add_test(NAME test_catalogstore COMMAND test_catalogstore)

# Install-step contract rules (header-only, no MOC, no filesystem)
# Catalog sync decision (pure ints — needs neither Qt nor a network stack)
add_executable(test_catalogsyncrules test_catalogsyncrules.cpp)
target_include_directories(test_catalogsyncrules PRIVATE
    ${MAKINE_PROJECT_ROOT}/qml/src/services)
target_link_libraries(test_catalogsyncrules PRIVATE GTest::gtest GTest::gtest_main)
add_test(NAME test_catalogsyncrules COMMAND test_catalogsyncrules)

# Catalog-vs-local game name matching (needs QString, no service construction)
add_executable(test_gamenamerules test_gamenamerules.cpp)
target_include_directories(test_gamenamerules PRIVATE
    ${MAKINE_PROJECT_ROOT}/qml/src/services)
target_link_libraries(test_gamenamerules PRIVATE GTest::gtest GTest::gtest_main Qt6::Core)
add_test(NAME test_gamenamerules COMMAND test_gamenamerules)

# What a hand-picked game folder is: its root, a library, or no game at all
# (real directory trees under QTemporaryDir, no service construction)
add_executable(test_gamefolderrules test_gamefolderrules.cpp)
target_include_directories(test_gamefolderrules PRIVATE
    ${MAKINE_PROJECT_ROOT}/qml/src/services)
target_link_libraries(test_gamefolderrules PRIVATE GTest::gtest GTest::gtest_main Qt6::Core)
add_test(NAME test_gamefolderrules COMMAND test_gamefolderrules)

add_executable(test_installsteprules test_installsteprules.cpp)
target_include_directories(test_installsteprules PRIVATE
    ${MAKINE_PROJECT_ROOT}/qml/src/services)
target_link_libraries(test_installsteprules PRIVATE GTest::gtest GTest::gtest_main Qt6::Core)
add_test(NAME test_installsteprules COMMAND test_installsteprules)

# A run step's output on the game page: split characters, "\r", the size cap.
add_executable(test_processconsole test_processconsole.cpp)
target_include_directories(test_processconsole PRIVATE
    ${MAKINE_PROJECT_ROOT}/qml/src/services)
target_link_libraries(test_processconsole PRIVATE GTest::gtest GTest::gtest_main Qt6::Core)
add_test(NAME test_processconsole COMMAND test_processconsole)

# Why a field failure happened, as a stable code instead of Turkish prose.
add_executable(test_failurereasons test_failurereasons.cpp)
target_include_directories(test_failurereasons PRIVATE
    ${MAKINE_PROJECT_ROOT}/qml/src/services)
target_link_libraries(test_failurereasons PRIVATE GTest::gtest GTest::gtest_main Qt6::Core)
add_test(NAME test_failurereasons COMMAND test_failurereasons)

# Counting-channel payload, offline queue and the Sentry quota gate.
add_executable(test_telemetryrules test_telemetryrules.cpp)
target_include_directories(test_telemetryrules PRIVATE
    ${MAKINE_PROJECT_ROOT}/qml/src/services)
target_link_libraries(test_telemetryrules PRIVATE GTest::gtest GTest::gtest_main Qt6::Core)
add_test(NAME test_telemetryrules COMMAND test_telemetryrules)

# What the user must still do once a patch is installed — the language slot the
# files landed in, and whether the catalogue note is an instruction at all.
add_executable(test_postinstallrules test_postinstallrules.cpp)
target_include_directories(test_postinstallrules PRIVATE
    ${MAKINE_PROJECT_ROOT}/qml/src/services)
target_link_libraries(test_postinstallrules PRIVATE GTest::gtest GTest::gtest_main Qt6::Core)
add_test(NAME test_postinstallrules COMMAND test_postinstallrules)

# Telemetry failure classification (built without MAKINE_HAS_SENTRY, so the
# reporting calls compile to no-ops and only the pure classifiers are exercised)
add_executable(test_crashreporter test_crashreporter.cpp
    ${MAKINE_PROJECT_ROOT}/qml/src/services/crashreporter.cpp)
target_include_directories(test_crashreporter PRIVATE
    ${MAKINE_PROJECT_ROOT}/qml/src/services)
target_link_libraries(test_crashreporter PRIVATE GTest::gtest GTest::gtest_main Qt6::Core)
add_test(NAME test_crashreporter COMMAND test_crashreporter)

# Does a downloaded package match what the catalogue published?
add_executable(test_packagechecksum test_packagechecksum.cpp)
target_include_directories(test_packagechecksum PRIVATE
    ${MAKINE_PROJECT_ROOT}/qml/src/services)
target_link_libraries(test_packagechecksum PRIVATE GTest::gtest GTest::gtest_main Qt6::Core)
add_test(NAME test_packagechecksum COMMAND test_packagechecksum)

# What keeps a user's Windows account name out of the telemetry we send.
add_executable(test_pathredaction test_pathredaction.cpp)
target_include_directories(test_pathredaction PRIVATE
    ${MAKINE_PROJECT_ROOT}/qml/src/services)
target_link_libraries(test_pathredaction PRIVATE GTest::gtest GTest::gtest_main)
add_test(NAME test_pathredaction COMMAND test_pathredaction)

# VPatch binary delta applier, against the NSIS reference project's own vectors.
add_executable(test_vpatchapply test_vpatchapply.cpp)
target_include_directories(test_vpatchapply PRIVATE
    ${MAKINE_PROJECT_ROOT}/qml/src/services)
target_link_libraries(test_vpatchapply PRIVATE GTest::gtest GTest::gtest_main Qt6::Core)
add_test(NAME test_vpatchapply COMMAND test_vpatchapply)

# Same-length find-and-replace in a game binary (the Elden Ring exe edit).
add_executable(test_bytepatch test_bytepatch.cpp)
target_include_directories(test_bytepatch PRIVATE
    ${MAKINE_PROJECT_ROOT}/qml/src/services)
target_link_libraries(test_bytepatch PRIVATE GTest::gtest GTest::gtest_main Qt6::Core)
add_test(NAME test_bytepatch COMMAND test_bytepatch)

# .forge reimport (AC Valhalla/Mirage), against QuickBMS's own output.
add_executable(test_forgereimport test_forgereimport.cpp)
target_include_directories(test_forgereimport PRIVATE
    ${MAKINE_PROJECT_ROOT}/qml/src/services)
target_link_libraries(test_forgereimport PRIVATE GTest::gtest GTest::gtest_main Qt6::Core)
add_test(NAME test_forgereimport COMMAND test_forgereimport)

# xdelta3 delta decoder (AC Odyssey), against deltas made by the legacy c1.exe.
add_executable(test_xdeltaapply test_xdeltaapply.cpp
    ${MAKINE_PROJECT_ROOT}/qml/src/services/xdeltaapply.cpp)
target_include_directories(test_xdeltaapply PRIVATE
    ${MAKINE_PROJECT_ROOT}/qml/src/services)
target_link_libraries(test_xdeltaapply PRIVATE GTest::gtest GTest::gtest_main xdelta3)
add_test(NAME test_xdeltaapply COMMAND test_xdeltaapply)

# CatalogProxyModel search-engine tests (Qt-dependent, real classes → AUTOMOC)
add_executable(test_catalogproxymodel test_catalogproxymodel.cpp
    ${MAKINE_PROJECT_ROOT}/qml/src/services/catalogproxymodel.cpp
    ${MAKINE_PROJECT_ROOT}/qml/src/services/supportedgamesmodel.cpp)
set_target_properties(test_catalogproxymodel PROPERTIES AUTOMOC ON)
target_include_directories(test_catalogproxymodel PRIVATE
    ${MAKINE_PROJECT_ROOT}/qml/src/services)
target_link_libraries(test_catalogproxymodel PRIVATE GTest::gtest Qt6::Core)
add_test(NAME test_catalogproxymodel COMMAND test_catalogproxymodel)

# MKPK tar extraction: PAX / GNU name records. mkpkformat.h includes the
# OpenSSL, zstd and fmt headers (only tar::extract is used), so this needs the
# vcpkg build.
if(NOT MAKINE_UI_ONLY)
    find_package(OpenSSL REQUIRED)
    find_package(zstd CONFIG REQUIRED)
    find_package(spdlog CONFIG REQUIRED)
    add_executable(test_mkpktar test_mkpktar.cpp)
    target_include_directories(test_mkpktar PRIVATE ${MAKINE_PROJECT_ROOT}/qml/src/services)
    target_link_libraries(test_mkpktar PRIVATE GTest::gtest GTest::gtest_main
        OpenSSL::Crypto spdlog::spdlog
        $<IF:$<TARGET_EXISTS:zstd::libzstd_shared>,zstd::libzstd_shared,zstd::libzstd_static>)
    add_test(NAME test_mkpktar COMMAND test_mkpktar)
endif()

# Plugin system v2 (docs/adr/0009-plugin-system-v2.md): manifest rules, wire
# framing, and the full launcher -> makine-plugin-host -> plugin DLL round trip.
add_executable(test_pluginmanifest test_pluginmanifest.cpp
    ${MAKINE_PROJECT_ROOT}/qml/src/services/pluginmanifest.cpp)
target_include_directories(test_pluginmanifest PRIVATE
    ${MAKINE_PROJECT_ROOT}/qml/src/services ${MAKINE_PROJECT_ROOT}/core/include)
target_link_libraries(test_pluginmanifest PRIVATE GTest::gtest GTest::gtest_main Qt6::Core)
add_test(NAME test_pluginmanifest COMMAND test_pluginmanifest)

if(NOT MAKINE_UI_ONLY)
    # Ed25519 registry signature + entry rules (OpenSSL).
    add_executable(test_pluginregistry test_pluginregistry.cpp
        ${MAKINE_PROJECT_ROOT}/qml/src/services/pluginregistry.cpp
        ${MAKINE_PROJECT_ROOT}/qml/src/services/pluginmanifest.cpp
        ${MAKINE_PROJECT_ROOT}/qml/src/services/pluginprocess.cpp)
    set_target_properties(test_pluginregistry PROPERTIES AUTOMOC ON)
    target_include_directories(test_pluginregistry PRIVATE
        ${MAKINE_PROJECT_ROOT}/qml/src/services ${MAKINE_PROJECT_ROOT}/core/include)
    find_package(Qt6 REQUIRED COMPONENTS Network)
    target_link_libraries(test_pluginregistry PRIVATE GTest::gtest GTest::gtest_main
        Qt6::Core Qt6::Network OpenSSL::Crypto)
    add_test(NAME test_pluginregistry COMMAND test_pluginregistry)

    # Staging, update, rollback and removal of plugin folders (real MKPK v2 packages).
    add_executable(test_plugininstaller test_plugininstaller.cpp
        ${MAKINE_PROJECT_ROOT}/qml/src/services/plugininstaller.cpp
        ${MAKINE_PROJECT_ROOT}/qml/src/services/pluginmanifest.cpp)
    target_include_directories(test_plugininstaller PRIVATE
        ${MAKINE_PROJECT_ROOT}/qml/src/services ${MAKINE_PROJECT_ROOT}/core/include)
    target_link_libraries(test_plugininstaller PRIVATE GTest::gtest GTest::gtest_main Qt6::Core
        OpenSSL::Crypto spdlog::spdlog
        $<IF:$<TARGET_EXISTS:zstd::libzstd_shared>,zstd::libzstd_shared,zstd::libzstd_static>)
    add_test(NAME test_plugininstaller COMMAND test_plugininstaller)
endif()

if(WIN32 AND TARGET makine-plugin-host)
    # Static C++ runtime: the host keeps the launcher folder and PATH out of the
    # plugin's DLL search, so a plugin must carry or embed what it links.
    add_library(makine-echo-plugin SHARED ${MAKINE_PROJECT_ROOT}/tests/plugins/echo/echo_plugin.cpp)
    target_include_directories(makine-echo-plugin PRIVATE ${MAKINE_PROJECT_ROOT}/core/include)
    set_target_properties(makine-echo-plugin PROPERTIES PREFIX "")
    if(MINGW)
        target_link_options(makine-echo-plugin PRIVATE -static -static-libgcc -static-libstdc++)
    endif()

    add_executable(test_pluginprocess test_pluginprocess.cpp
        ${MAKINE_PROJECT_ROOT}/qml/src/services/pluginprocess.cpp)
    set_target_properties(test_pluginprocess PROPERTIES AUTOMOC ON)
    target_include_directories(test_pluginprocess PRIVATE
        ${MAKINE_PROJECT_ROOT}/qml/src/services ${MAKINE_PROJECT_ROOT}/core/include)
    target_compile_definitions(test_pluginprocess PRIVATE
        MAKINE_PLUGIN_HOST="$<TARGET_FILE:makine-plugin-host>"
        MAKINE_ECHO_PLUGIN="$<TARGET_FILE:makine-echo-plugin>")
    target_link_libraries(test_pluginprocess PRIVATE GTest::gtest Qt6::Core)
    add_dependencies(test_pluginprocess makine-plugin-host makine-echo-plugin)
    add_test(NAME test_pluginprocess COMMAND test_pluginprocess)
endif()
