cmake_minimum_required(VERSION 3.25)

# Integration tests for UI service layer.
# These tests link the actual service implementations against a temp
# filesystem to exercise multi-step flows (B4-01).

if(NOT DEFINED MAKINE_PROJECT_ROOT)
    set(MAKINE_PROJECT_ROOT "${CMAKE_SOURCE_DIR}")
endif()

# operationjournal.h pulls in <makine/crash_recovery.hpp> which transitively
# uses std::expected and std::unexpected from C++23. Match the core project's
# standard so we don't fall back to GCC's deprecated <exception> overload.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# OperationJournal is Q_OBJECT-derived; pull it into automoc.
set(CMAKE_AUTOMOC ON)

find_package(GTest REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core)

set(SERVICES_DIR ${MAKINE_PROJECT_ROOT}/qml/src/services)
set(CORE_INCLUDE_DIR ${MAKINE_PROJECT_ROOT}/core/include)

# OperationJournal recover integration test
# Links the actual UI-side wrapper against makine_core's CrashRecoveryJournal
# so the journal-write / journal-read / recover() round trip is exercised
# the same way the live app does it.
add_executable(test_operationjournal_recover_integration
    test_operationjournal_recover.cpp
    ${SERVICES_DIR}/operationjournal.cpp
)
target_include_directories(test_operationjournal_recover_integration PRIVATE
    ${SERVICES_DIR}
    ${CORE_INCLUDE_DIR}
)
target_link_libraries(test_operationjournal_recover_integration PRIVATE
    GTest::gtest
    GTest::gtest_main
    Qt6::Core
    makine_core
)
add_test(NAME test_operationjournal_recover_integration
         COMMAND test_operationjournal_recover_integration)

# NOTE: BackupManager / TranslationDownloader / SelfUpdater integration
# tests need the qml services compiled as a separate object library
# (their headers transitively reach QQmlEngine/QGuiApplication/etc.,
# which would drag the Qt Gui module into a headless test binary).
# Tracked under B4-01 follow-up — see docs/development/integration-coverage.md.
