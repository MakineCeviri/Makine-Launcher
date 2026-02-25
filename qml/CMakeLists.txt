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

# vcpkg integration - only needed for standalone builds (not super-build)
if(NOT MAKINEAI_UI_ONLY AND NOT TARGET makineai_core)
    if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "Vcpkg toolchain file")
    endif()

    # Set vcpkg triplet for MinGW if using MinGW compiler
    if(MINGW AND NOT DEFINED VCPKG_TARGET_TRIPLET)
        set(VCPKG_TARGET_TRIPLET "x64-mingw-dynamic" CACHE STRING "")
    endif()

    # Find core dependencies (for standalone linking)
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

# QTP0004: Let Qt handle subdirectory qmldir generation
qt_policy(SET QTP0004 OLD)

# Core library linkage
if(NOT MAKINEAI_UI_ONLY)
    if(TARGET makineai_core)
        # Super-build: core target available from parent project
        message(STATUS "MakineAI: using makineai_core target from super-build")
    else()
        # Standalone: find prebuilt core library
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
                "Build it first or use super-build: cmake --preset release")
        endif()

    endif()
    set(CORE_SOURCES "")
endif()

# Backend service sources
set(BACKEND_SOURCES
    src/services/gameservice.cpp
    src/services/settingsmanager.cpp
    src/services/backupmanager.cpp
    src/services/processscanner.cpp
    src/services/corebridge.cpp
    src/services/systemtraymanager.cpp
    src/services/integrityservice.cpp
    src/services/batchoperationservice.cpp
    src/services/localpackagemanager.cpp
    src/services/updatedetectionservice.cpp
    src/services/updatechecker.cpp
    src/services/operationjournal.cpp
    src/services/appprotection.cpp
    src/services/imagecachemanager.cpp
    src/services/manifestsyncservice.cpp
    src/services/translationdownloader.cpp
    src/services/perfreporter.cpp
    src/services/frametimer.cpp
    src/services/sceneprofiler.cpp
    src/services/memoryprofiler.cpp
)

set(BACKEND_HEADERS
    src/services/gameservice.h
    src/services/settingsmanager.h
    src/services/backupmanager.h
    src/services/processscanner.h
    src/services/corebridge.h
    src/services/systemtraymanager.h
    src/services/integrityservice.h
    src/services/batchoperationservice.h
    src/services/localpackagemanager.h
    src/services/vdfparser.h
    src/services/updatedetectionservice.h
    src/services/updatechecker.h
    src/services/pathsecurity.h
    src/services/operationjournal.h
    src/services/appprotection.h
    src/services/obfstring.h
    src/services/encryption_key.h
    src/services/imagecachemanager.h
    src/services/manifestsyncservice.h
    src/services/translationdownloader.h
    src/services/mkpkformat.h
    src/services/apppaths.h
    src/services/profiler.h
    src/services/perfreporter.h
    src/services/frametimer.h
    src/services/sceneprofiler.h
    src/services/memoryprofiler.h
)


# QML module
qt_add_executable(MakineAI
    src/main.cpp
    ${BACKEND_SOURCES}
    ${BACKEND_HEADERS}
    ${CORE_SOURCES}
)

# Windows resource file: icon, manifest (DPI/UTF-8/visual styles), version info
if(WIN32)
    target_sources(MakineAI PRIVATE resources/app.rc)
endif()

# Mark singleton files BEFORE qt_add_qml_module
set_source_files_properties(qml/theme/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(qml/theme/Dimensions.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(qml/theme/DebugHelper.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

# Showcase images (validated game headers, embedded in binary)
file(GLOB SHOWCASE_IMAGES CONFIGURE_DEPENDS
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    "resources/showcase/*.jpg"
)

qt_add_qml_module(MakineAI
    URI MakineAI
    VERSION 1.0
    QML_FILES
        # Main Screens
        qml/Main.qml
        qml/HomeScreen.qml
        qml/SettingsScreen.qml
        qml/GameDetailScreen.qml
        qml/OnboardingWizard.qml

        # Screens - Home Pages
        qml/screens/HomePage.qml
        qml/screens/Library.qml

        # Screens - Game Detail Sections
        qml/screens/detail/HeroSection.qml
        qml/screens/detail/QuickStatsRow.qml
        qml/screens/detail/AboutSection.qml
        qml/screens/detail/ScreenshotCarousel.qml
        qml/screens/detail/ContributorsSection.qml
        qml/screens/detail/RuntimeSection.qml
        qml/screens/detail/BackupSection.qml

        # Screens - Settings Panels
        qml/screens/settings/GeneralSettings.qml
        qml/screens/settings/TranslationSettings.qml
        qml/screens/settings/BackupsSettings.qml
        qml/screens/settings/PerformanceSettings.qml
        qml/screens/settings/AboutSettings.qml
        qml/screens/settings/DeveloperSettings.qml

        # Screens - Onboarding Steps
        qml/screens/onboarding/WelcomeStep.qml
        qml/screens/onboarding/ScanStep.qml
        qml/screens/onboarding/ReadyStep.qml

        # Theme (singletons)
        qml/theme/Theme.qml
        qml/theme/Dimensions.qml
        qml/theme/DebugHelper.qml

        # ── Reusable Components (public API — used across multiple screens) ──
        #
        # Cards: Data display containers
        qml/components/GameCard.qml              # Game item in grid/list
        qml/components/GameCardSkeleton.qml      # Loading placeholder for GameCard
        qml/components/AnnouncementCard.qml       # RE Requiem hero / announcement display
        qml/components/GameDetectionCard.qml      # Game detection status + manual add
        qml/components/ProjectShowcaseCard.qml    # Showcase project display
        #
        # Navigation: Window chrome and nav
        qml/components/TitleBar.qml               # Custom frameless title bar
        qml/components/NavBar.qml                 # Bottom navigation bar
        qml/components/NavItem.qml                # Navigation tab item
        qml/components/WindowResizeHandles.qml    # Frameless window resize
        #
        # Layout: Section containers
        qml/components/SectionContainer.qml       # Glassmorphic section wrapper
        qml/components/GameSection.qml            # Section with title + game strip + empty state
        qml/components/CatalogSection.qml         # Localization library catalog (search + game strips)
        #
        # Visual: Decorative and feedback
        qml/components/AmbientGlow.qml            # Radial glow with rounded-rect clip
        qml/components/AnimatedGradientGlow.qml   # Animated glow effect
        qml/components/GradientBorderCanvas.qml   # Gradient border drawing
        qml/components/SkeletonLoader.qml         # Generic loading skeleton
        qml/components/TurkishFlagBadge.qml       # TR flag indicator
        #
        # ── Internal Components (settings/detail screens only) ──
        #
        qml/components/SettingsCard.qml           # Settings group container
        qml/components/SettingsSidebar.qml        # Settings nav sidebar
        qml/components/SettingsDivider.qml        # Settings section divider
        qml/components/ToggleSetting.qml          # Toggle switch setting
        qml/components/ThemeSetting.qml           # Theme picker setting
        qml/components/InfoSettingWithBadge.qml   # Info display with badge
        qml/components/DisabledSetting.qml        # Disabled/locked setting
        qml/components/ClickableRow.qml           # Clickable list row
        qml/components/CategoryItem.qml           # Category filter item
        qml/components/BatchOperationsPanel.qml   # Batch ops control
        qml/components/PerformanceMonitor.qml     # Dev-only perf overlay
        qml/components/EmptyState.qml             # Reusable empty/zero-data state
        qml/components/FocusRing.qml              # Keyboard focus indicator
        qml/components/StyledScrollBar.qml        # Themed scrollbar
        qml/components/HorizontalGameStrip.qml  # Horizontal scrolling game card strip
        qml/components/NotificationBanner.qml     # Dismissable notification bar
        qml/components/GameStatusCard.qml         # Game scanning status card

        # Controllers (Main.qml decomposition)
        qml/controllers/GameDataResolver.qml
        qml/controllers/InstallFlowController.qml

        # Dialogs
        qml/dialogs/BaseDialog.qml
        qml/dialogs/DialogCloseButton.qml
        qml/dialogs/AntiCheatWarningDialog.qml
        qml/dialogs/ConfirmDialog.qml
        qml/dialogs/VariantSelectionDialog.qml
        qml/dialogs/InstallNotesDialog.qml
        qml/dialogs/InstallOptionsDialog.qml
        qml/dialogs/UpdateAlertDialog.qml

    RESOURCES
        # SVG icons (8 active)
        resources/icons/arrow_right.svg
        resources/icons/discord.svg
        resources/icons/lock.svg
        resources/icons/shield-check.svg
        resources/icons/shield-check-active.svg
        resources/icons/steam.svg
        resources/icons/window-search.svg
        resources/icons/window-search-active.svg
        # PNG images
        resources/images/logo.png
        # Showcase images (validated game headers, embedded in binary)
        ${SHOWCASE_IMAGES}
        # Fonts (Inter - Google Fonts)
        resources/fonts/Inter-Regular.ttf
        resources/fonts/Inter-Medium.ttf
        resources/fonts/Inter-SemiBold.ttf
        resources/fonts/Inter-Bold.ttf
)

# ===== Binary Size Optimization =====
# GC sections: compile each function/data into separate sections, then
# discard unreferenced ones at link time. Critical for static Qt builds
# where unused Qt code would otherwise bloat the binary.
if(MSVC)
    target_compile_options(MakineAI PRIVATE
        /GS           # Buffer security check (stack cookies)
        /guard:cf     # Control Flow Guard
        /sdl          # Additional security checks
    )
    target_link_options(MakineAI PRIVATE
        /DYNAMICBASE          # ASLR
        /NXCOMPAT             # DEP (Data Execution Prevention)
        /HIGHENTROPYVA        # 64-bit ASLR
        /CETCOMPAT            # Intel CET shadow stack
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(MakineAI PRIVATE
        -ffunction-sections          # Each function in its own section
        -fdata-sections              # Each data object in its own section
        -fvisibility=hidden          # Hide all symbols by default (smaller export table)
        -fvisibility-inlines-hidden  # Hide inline functions
        -fstack-protector-strong     # Stack smashing protection
        -D_FORTIFY_SOURCE=2          # Buffer overflow detection
        -Wformat -Wformat-security   # Format string warnings
    )
    target_link_options(MakineAI PRIVATE
        -Wl,--gc-sections            # Discard unreferenced sections at link time
        -Wl,--dynamicbase            # ASLR
        -Wl,--nxcompat               # DEP
        -Wl,--high-entropy-va        # 64-bit ASLR
    )
endif()

# Include directories
target_include_directories(MakineAI PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services
)

if(NOT MAKINEAI_UI_ONLY AND NOT TARGET makineai_core)
    # Standalone: explicit include path needed
    target_include_directories(MakineAI PRIVATE
        ${MAKINEAI_CORE_DIR}/include
    )
    # Super-build: includes inherited from makineai_core target (PUBLIC)
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

# Static Qt: import QML plugins at link time (no-op for shared builds)
qt_import_qml_plugins(MakineAI)

# Link core dependencies (only for full build)
if(NOT MAKINEAI_UI_ONLY)
    if(TARGET makineai_core)
        # Super-build: link directly (transitive dependencies handled automatically)
        target_link_libraries(MakineAI PRIVATE makineai_core)
    else()
        # Standalone: link prebuilt library + explicit dependencies
        target_link_libraries(MakineAI PRIVATE
            ${MAKINEAI_CORE_LIB}
        )
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
    endif()
else()
    # Define MAKINEAI_UI_ONLY macro for stub implementations
    target_compile_definitions(MakineAI PRIVATE MAKINEAI_UI_ONLY)
endif()

# Dev tools (FrameTimer, PerfReporter, F3 overlay) — off in release builds
option(MAKINEAI_DEV_TOOLS "Enable dev-only performance tools (FrameTimer, PerfReporter, F3 overlay)" OFF)
if(MAKINEAI_DEV_TOOLS)
    target_compile_definitions(MakineAI PRIVATE MAKINEAI_DEV_TOOLS MAKINEAI_PERF_REPORT)
    message(STATUS "MakineAI: Dev tools enabled (FrameTimer + PerfReporter + F3 overlay)")
endif()

# Tracy profiler (optional, adds Tracy on top of dev tools)
if(MAKINEAI_PROFILER AND TARGET TracyClient)
    target_link_libraries(MakineAI PRIVATE TracyClient)
    target_compile_definitions(MakineAI PRIVATE TRACY_ENABLE)
    message(STATUS "MakineAI: Tracy profiler linked")
elseif(MAKINEAI_PROFILER AND NOT MAKINEAI_DEV_TOOLS)
    # Standalone profiler mode (no dev tools, just PerfReporter)
    target_compile_definitions(MakineAI PRIVATE MAKINEAI_PERF_REPORT)
    message(STATUS "MakineAI: PerfReporter enabled (standalone)")
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

    # Security hardening & AV false-positive reduction
    if(MSVC)
        target_compile_options(MakineAI PRIVATE
            /GS             # Buffer security check (stack cookies)
            /guard:cf       # Control Flow Guard
            /sdl            # Additional security checks
            /Qspectre       # Spectre mitigation
        )
        target_link_options(MakineAI PRIVATE
            /DYNAMICBASE        # ASLR
            /HIGHENTROPYVA      # High-entropy 64-bit ASLR
            /NXCOMPAT           # Data Execution Prevention (DEP)
            /INTEGRITYCHECK     # PE integrity check — AV trusts this
            /CETCOMPAT          # Intel CET shadow stack compatibility
        )
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(MakineAI PRIVATE
            -fstack-protector-strong
            -D_FORTIFY_SOURCE=2
        )
        target_link_options(MakineAI PRIVATE
            -Wl,--dynamicbase       # ASLR
            -Wl,--high-entropy-va   # High-entropy ASLR
            -Wl,--nxcompat          # DEP
        )
    endif()

    # Link Windows libraries for process scanning, system tray, and crypto
    target_link_libraries(MakineAI PRIVATE
        psapi
        Shlwapi
        Advapi32
        User32
        Crypt32
        Wintrust     # Authenticode verification
        dwmapi       # DWM acrylic/mica backdrop
    )
endif()

# ===== Internationalization (i18n) =====
find_package(Qt6 COMPONENTS LinguistTools QUIET)
if(Qt6LinguistTools_FOUND)
    set(TS_FILES
        i18n/makineai_en.ts
    )
    qt_add_translations(MakineAI
        TS_FILES ${TS_FILES}
        RESOURCE_PREFIX "/i18n"
    )
    message(STATUS "i18n: Translation files configured (${TS_FILES})")
else()
    message(STATUS "i18n: Qt6::LinguistTools not found, translations disabled")
endif()

# Strip debug symbols in release builds (all configs, not just static)
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT MSVC)
    add_custom_command(TARGET MakineAI POST_BUILD
        COMMAND ${CMAKE_STRIP} "$<TARGET_FILE:MakineAI>"
        COMMENT "Stripping release binary for minimal size"
        VERBATIM
    )
endif()

# Optional: UPX compression for even smaller static binaries (~40-50% reduction)
# Enable with: cmake -DMAKINEAI_UPX=ON
option(MAKINEAI_UPX "Compress static binary with UPX" OFF)
if(MAKINEAI_UPX AND CMAKE_EXE_LINKER_FLAGS MATCHES "-static")
    find_program(UPX_EXECUTABLE upx)
    if(UPX_EXECUTABLE)
        add_custom_command(TARGET MakineAI POST_BUILD
            COMMAND ${UPX_EXECUTABLE} --best --lzma "$<TARGET_FILE:MakineAI>"
            COMMENT "UPX compressing static binary (--best --lzma)"
            VERBATIM
        )
        message(STATUS "UPX: Enabled (${UPX_EXECUTABLE})")
    else()
        message(WARNING "UPX: Requested but upx not found in PATH")
    endif()
endif()

# Post-build: generate SHA-256 hash file for integrity verification
add_custom_command(TARGET MakineAI POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E sha256sum "$<TARGET_FILE:MakineAI>"
            > "$<TARGET_FILE:MakineAI>.sha256"
    COMMENT "Generating binary integrity hash"
    VERBATIM
)

# Install
install(TARGETS MakineAI
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
