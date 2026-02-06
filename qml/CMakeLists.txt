cmake_minimum_required(VERSION 3.16)

project(MakineAI VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Option to build UI-only (without core library dependencies)
option(MAKINEAI_UI_ONLY "Build UI shell only without core library" OFF)

# Core library path
set(MAKINEAI_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../core)

# vcpkg integration (auto-detect toolchain) - only for full build
if(NOT MAKINEAI_UI_ONLY)
    if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "Vcpkg toolchain file")
    endif()

    # Set vcpkg triplet for MinGW if using MinGW compiler
    if(MINGW AND NOT DEFINED VCPKG_TARGET_TRIPLET)
        set(VCPKG_TARGET_TRIPLET "x64-mingw-dynamic" CACHE STRING "")
    endif()

    # Find core dependencies
    find_package(Boost REQUIRED COMPONENTS filesystem)
    find_package(OpenSSL REQUIRED)
    find_package(CURL REQUIRED)
    find_package(nlohmann_json CONFIG REQUIRED)
    find_package(lz4 CONFIG REQUIRED)
    find_package(ZLIB REQUIRED)
    find_package(zstd CONFIG REQUIRED)
    find_package(unofficial-sqlite3 CONFIG REQUIRED)
    find_package(spdlog CONFIG REQUIRED)
endif()

# Find Qt packages
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Quick
    QuickControls2
    QuickEffects
    Network
    Svg
    Concurrent
)

qt_standard_project_setup(REQUIRES 6.5)

# Core library sources (embedded) - only for full build
if(NOT MAKINEAI_UI_ONLY)
    set(CORE_SOURCES
        ${MAKINEAI_CORE_DIR}/src/game_detector/game_detector.cpp
        ${MAKINEAI_CORE_DIR}/src/game_detector/steam_scanner.cpp
        ${MAKINEAI_CORE_DIR}/src/game_detector/epic_scanner.cpp
        ${MAKINEAI_CORE_DIR}/src/game_detector/gog_scanner.cpp
        ${MAKINEAI_CORE_DIR}/src/translation/translation_memory.cpp
        ${MAKINEAI_CORE_DIR}/src/translation/glossary_service.cpp
        ${MAKINEAI_CORE_DIR}/src/translation/qa_service.cpp
        ${MAKINEAI_CORE_DIR}/src/handlers/unity_handler.cpp
        ${MAKINEAI_CORE_DIR}/src/handlers/unreal_handler.cpp
        ${MAKINEAI_CORE_DIR}/src/handlers/renpy_handler.cpp
        ${MAKINEAI_CORE_DIR}/src/handlers/rpgmaker_handler.cpp
        ${MAKINEAI_CORE_DIR}/src/handlers/gamemaker_handler.cpp
        ${MAKINEAI_CORE_DIR}/src/database/database.cpp
        ${MAKINEAI_CORE_DIR}/src/platform/windows/registry.cpp
        ${MAKINEAI_CORE_DIR}/src/platform/windows/filesystem.cpp
    )
endif()

# Backend service sources
set(BACKEND_SOURCES
    src/services/gameservice.cpp
    src/services/translationservice.cpp
    src/services/settingsmanager.cpp
    src/services/backupmanager.cpp
    src/services/processscanner.cpp
    src/services/corebridge.cpp
)

set(BACKEND_HEADERS
    src/services/gameservice.h
    src/services/translationservice.h
    src/services/settingsmanager.h
    src/services/backupmanager.h
    src/services/processscanner.h
    src/services/corebridge.h
)

# Model sources
set(MODEL_SOURCES
    src/models/gamelistmodel.cpp
    src/models/translationmodel.cpp
)

set(MODEL_HEADERS
    src/models/gamelistmodel.h
    src/models/translationmodel.h
    src/makineai_metatypes.h
)

# QML module
qt_add_executable(MakineAI
    src/main.cpp
    ${BACKEND_SOURCES}
    ${BACKEND_HEADERS}
    ${MODEL_SOURCES}
    ${MODEL_HEADERS}
    ${CORE_SOURCES}
)

# Mark singleton files BEFORE qt_add_qml_module
set_source_files_properties(qml/theme/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(qml/theme/Dimensions.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(qml/theme/DebugHelper.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(MakineAI
    URI MakineAI
    VERSION 1.0
    QML_FILES
        # Main
        qml/Main.qml
        qml/HomeScreen.qml
        qml/SettingsScreen.qml
        qml/SplashScreen.qml
        qml/SplashWindow.qml
        qml/GameDetailScreen.qml
        qml/TranslationWorkflowScreen.qml

        # Theme (singletons)
        qml/theme/Theme.qml
        qml/theme/Dimensions.qml
        qml/theme/DebugHelper.qml

        # Components
        qml/components/GlassCard.qml
        qml/components/StopButton.qml
        qml/components/AnimatedButton.qml
        qml/components/GradientButton.qml
        qml/components/MakineLogo.qml
        qml/components/RotatingHourglass.qml
        qml/components/TranslationPhaseBadge.qml
        qml/components/WaitingForGameCard.qml
        qml/components/GameDetectedCard.qml
        qml/components/AnnouncementCard.qml
        qml/components/DiscordButton.qml
        qml/components/CedraCard.qml
        qml/components/BackgroundBlur.qml
        qml/components/GlassPanel.qml
        qml/components/GradientText.qml
        qml/components/MinimalTitleBar.qml
        qml/components/NavBar.qml
        qml/components/NavBarItem.qml
        qml/components/CedraInteractiveCard.qml
        qml/components/DonateButton.qml
        qml/components/GameCard.qml
        qml/components/HoverBuilder.qml
        qml/components/ViewAllCard.qml
        qml/components/WindowButton.qml
        qml/components/TranslationProgressBar.qml
        qml/components/HoverGlow.qml
        qml/components/SkeletonLoader.qml
        qml/components/GameCardSkeleton.qml
        qml/components/PerformanceMonitor.qml

        # Dialogs
        qml/dialogs/GameDetectorDialog.qml
        qml/dialogs/AllGamesDialog.qml
        qml/dialogs/QAResultsDialog.qml
        qml/dialogs/AntiCheatWarningDialog.qml

    RESOURCES
        # SVG icons
        resources/icons/logo.svg
        resources/icons/home.svg
        resources/icons/settings.svg
        resources/icons/close.svg
        resources/icons/minimize.svg
        resources/icons/maximize.svg
        resources/icons/search.svg
        resources/icons/check.svg
        resources/icons/arrow_back.svg
        resources/icons/verified.svg
        resources/icons/stop.svg
        resources/icons/donate.svg
        resources/icons/arrow_right.svg
        resources/icons/backup.svg
        resources/icons/folder.svg
        resources/icons/delete.svg
        resources/icons/info.svg
        resources/icons/refresh.svg
        # PNG images
        resources/images/logo.png
)

# Include directories
target_include_directories(MakineAI PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services
    ${CMAKE_CURRENT_SOURCE_DIR}/src/models
)

if(NOT MAKINEAI_UI_ONLY)
    target_include_directories(MakineAI PRIVATE
        ${MAKINEAI_CORE_DIR}/include
    )
endif()

# Link Qt libraries (always required)
target_link_libraries(MakineAI PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickEffects
    Qt6::Network
    Qt6::Svg
    Qt6::Concurrent
)

# Link core dependencies (only for full build)
if(NOT MAKINEAI_UI_ONLY)
    target_link_libraries(MakineAI PRIVATE
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
else()
    # Define MAKINEAI_UI_ONLY macro for stub implementations
    target_compile_definitions(MakineAI PRIVATE MAKINEAI_UI_ONLY)
endif()

# Windows specific
if(WIN32)
    set_target_properties(MakineAI PROPERTIES
        WIN32_EXECUTABLE TRUE
    )

    # Windows definitions
    target_compile_definitions(MakineAI PRIVATE
        UNICODE
        _UNICODE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )

    # Link Windows libraries for process scanning and core
    target_link_libraries(MakineAI PRIVATE
        psapi
        Shlwapi
        Advapi32
        User32
    )
endif()

# Install
install(TARGETS MakineAI
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
