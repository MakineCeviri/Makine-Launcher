cmake_minimum_required(VERSION 3.20)

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
    Widgets
    PrintSupport
)

qt_standard_project_setup(REQUIRES 6.5)

# Core library - link to prebuilt static library
if(NOT MAKINEAI_UI_ONLY)
    # Search for core library in possible build directories
    find_library(MAKINEAI_CORE_LIB
        NAMES makineai_core
        PATHS
            ${MAKINEAI_CORE_DIR}/build
            ${MAKINEAI_CORE_DIR}/build/core
            ${MAKINEAI_CORE_DIR}/build/release
            ${MAKINEAI_CORE_DIR}/build_msvc
            ${CMAKE_BINARY_DIR}/../core
        NO_DEFAULT_PATH
    )

    if(NOT MAKINEAI_CORE_LIB)
        message(FATAL_ERROR "Core library not found. "
                           "Build it first: cmake --preset=core && cmake --build build/core")
    endif()

    get_filename_component(MAKINEAI_CORE_LIB_DIR ${MAKINEAI_CORE_LIB} DIRECTORY)
    set(CORE_SOURCES "")
endif()

# Backend service sources
set(BACKEND_SOURCES
    src/services/gameservice.cpp
    src/services/translationservice.cpp
    src/services/settingsmanager.cpp
    src/services/backupmanager.cpp
    src/services/processscanner.cpp
    src/services/corebridge.cpp
    src/services/systemtraymanager.cpp
)

set(BACKEND_HEADERS
    src/services/gameservice.h
    src/services/translationservice.h
    src/services/settingsmanager.h
    src/services/backupmanager.h
    src/services/processscanner.h
    src/services/corebridge.h
    src/services/systemtraymanager.h
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

# Link pre-compiled Windows icon resource
if(WIN32 AND EXISTS "${CMAKE_CURRENT_BINARY_DIR}/app_icon.obj")
    target_link_libraries(MakineAI PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/app_icon.obj")
endif()

# Mark singleton files BEFORE qt_add_qml_module
set_source_files_properties(qml/theme/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(qml/theme/Dimensions.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(qml/theme/DebugHelper.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(MakineAI
    URI MakineAI
    VERSION 1.0
    QML_FILES
        # Main Screens
        qml/Main.qml
        qml/HomeScreen.qml
        qml/SettingsScreen.qml
        qml/GameDetailScreen.qml
        qml/TranslationWorkflowScreen.qml

        # Theme (singletons)
        qml/theme/Theme.qml
        qml/theme/Dimensions.qml
        qml/theme/DebugHelper.qml

        # Settings Pages
        qml/settings/GeneralSettings.qml
        qml/settings/TranslationSettings.qml
        qml/settings/ProjectsSettings.qml
        qml/settings/PerformanceSettings.qml
        qml/settings/AboutSettings.qml
        qml/settings/DeveloperSettings.qml

        # Components - Cards
        qml/components/GlassCard.qml
        qml/components/CedraCard.qml
        qml/components/CedraInteractiveCard.qml
        qml/components/AnimatedGradientGlow.qml
        qml/components/GameCard.qml
        qml/components/GameCardSkeleton.qml
        qml/components/GameDetectedCard.qml
        qml/components/GameStatusCard.qml
        qml/components/WaitingForGameCard.qml
        qml/components/AnnouncementCard.qml
        qml/components/ProjectCard.qml
        qml/components/SettingsCard.qml
        qml/components/ViewAllCard.qml

        # Components - Buttons
        qml/components/StopButton.qml
        qml/components/AnimatedButton.qml
        qml/components/GradientButton.qml
        qml/components/ActionButton.qml
        qml/components/IconButton.qml
        qml/components/DevButton.qml
        qml/components/DiscordButton.qml
        qml/components/DonateButton.qml
        qml/components/WindowButton.qml

        # Components - Navigation
        qml/components/NavBar.qml
        qml/components/NavBarItem.qml
        qml/components/NavItem.qml
        qml/components/MinimalTitleBar.qml
        qml/components/TitleBar.qml

        # Components - Visual
        qml/components/MakineLogo.qml
        qml/components/RotatingHourglass.qml
        qml/components/TranslationPhaseBadge.qml
        qml/components/TranslationProgressBar.qml
        qml/components/BackgroundBlur.qml
        qml/components/GlassPanel.qml
        qml/components/GradientText.qml
        qml/components/HoverGlow.qml
        qml/components/HoverBuilder.qml
        qml/components/SkeletonLoader.qml
        qml/components/PerformanceMonitor.qml
        qml/components/Badge.qml
        qml/components/MetacriticScoreBadge.qml
        qml/components/PhaseIndicator.qml
        qml/components/PlatformIcons.qml

        # Components - Settings UI
        qml/components/ToggleSetting.qml
        qml/components/ThemeSetting.qml
        qml/components/InfoSettingWithBadge.qml
        qml/components/DisabledSetting.qml
        qml/components/SettingsDivider.qml
        qml/components/ClickableRow.qml

        # Components - List Items
        qml/components/CategoryItem.qml
        qml/components/ProjectCategory.qml
        qml/components/BackupListItem.qml
        qml/components/ActivityListItem.qml
        qml/components/InfoRow.qml
        qml/components/SectionHeader.qml
        qml/components/EmptyStateMessage.qml
        qml/components/CustomScrollBar.qml
        qml/components/AppFooter.qml

        # Dialogs
        qml/dialogs/GameDetectorDialog.qml
        qml/dialogs/AllGamesDialog.qml
        qml/dialogs/QAResultsDialog.qml
        qml/dialogs/AntiCheatWarningDialog.qml
        qml/dialogs/CancelConfirmationDialog.qml

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
        resources/icons/hourglass.svg
        resources/icons/flag-tr.svg
        resources/icons/box.svg
        resources/icons/globe.svg
        resources/icons/gamepad.svg
        resources/icons/rocket.svg
        resources/icons/shield.svg
        resources/icons/clock.svg
        resources/icons/megaphone.svg
        resources/icons/plus.svg
        resources/icons/star.svg
        resources/icons/chevron-right.svg
        resources/icons/lock.svg
        resources/icons/trash.svg
        resources/icons/rotate-ccw.svg
        resources/icons/settings.svg
        resources/icons/heart.svg
        resources/icons/coffee.svg
        resources/icons/shield-check.svg
        resources/icons/shield-check-active.svg
        resources/icons/pin.svg
        resources/icons/discord.svg
        resources/icons/gamepad-search.svg
        resources/icons/window-search.svg
        # PNG images
        resources/images/logo.png
        # Fonts (Inter - Google Fonts, Flutter style)
        resources/fonts/Inter-Regular.ttf
        resources/fonts/Inter-Medium.ttf
        resources/fonts/Inter-SemiBold.ttf
        resources/fonts/Inter-Bold.ttf
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
    Qt6::Widgets
    Qt6::PrintSupport
)

# Link core dependencies (only for full build)
if(NOT MAKINEAI_UI_ONLY)
    # Link to prebuilt MakineAI Core library
    target_link_libraries(MakineAI PRIVATE
        ${MAKINEAI_CORE_LIB}
    )

    # Link vcpkg dependencies (required by Core)
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
        Version      # For GetFileVersionInfo APIs
    )
endif()

# Install
install(TARGETS MakineAI
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
