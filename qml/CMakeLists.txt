cmake_minimum_required(VERSION 3.20)

# Version inherited from parent CMakeLists.txt (MAKINE_VERSION_FULL)
# Fallback for standalone builds
if(NOT DEFINED MAKINE_VERSION_FULL)
    project(MakineLauncher VERSION 0.1.0 LANGUAGES CXX)
    set(MAKINE_VERSION_SUFFIX "pre-alpha")
    set(MAKINE_VERSION_FULL "${PROJECT_VERSION}-${MAKINE_VERSION_SUFFIX}")
else()
    project(MakineLauncher VERSION ${PROJECT_VERSION} LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Option to build UI-only (without core library dependencies)
option(MAKINE_UI_ONLY "Build UI shell only without core library" OFF)


# Core library path
set(MAKINE_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../core)

# vcpkg integration - only needed for standalone builds (not super-build)
if(NOT MAKINE_UI_ONLY AND NOT TARGET makine_core)
    if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "Vcpkg toolchain file")
    endif()

    # Set vcpkg triplet for MinGW if using MinGW compiler
    if(MINGW AND NOT DEFINED VCPKG_TARGET_TRIPLET)
        set(VCPKG_TARGET_TRIPLET "x64-mingw-dynamic" CACHE STRING "")
    endif()

    # Find core dependencies (for standalone linking)
    find_package(OpenSSL REQUIRED)
    find_package(nlohmann_json CONFIG REQUIRED)
    find_package(spdlog CONFIG REQUIRED)
endif()

# Package decompression (mkpkformat.h) — the launcher's own dependency
if(NOT MAKINE_UI_ONLY)
    find_package(zstd CONFIG REQUIRED)
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
if(NOT MAKINE_UI_ONLY)
    if(TARGET makine_core)
        # Super-build: core target available from parent project
        message(STATUS "MakineLauncher: using makine_core target from super-build")
    else()
        # Standalone: find prebuilt core library
        find_library(MAKINE_CORE_LIB
            NAMES makine_core
            PATHS
                ${MAKINE_CORE_DIR}/build
                ${MAKINE_CORE_DIR}/build/core
                ${MAKINE_CORE_DIR}/build/release
                ${MAKINE_CORE_DIR}/build_msvc
                ${CMAKE_BINARY_DIR}/../core
            NO_DEFAULT_PATH
        )

        if(NOT MAKINE_CORE_LIB)
            message(FATAL_ERROR "Core library not found. "
                "Build it first or use super-build: cmake --preset release")
        endif()

    endif()
    set(CORE_SOURCES "")
endif()

# Backend service sources
set(BACKEND_SOURCES
    src/services/gameservice.cpp
    src/services/steamdetailsservice.cpp
    src/services/communityservice.cpp
    src/services/settingsmanager.cpp
    src/services/backupmanager.cpp
    src/services/processscanner.cpp
    src/services/corebridge.cpp
    src/services/systemtraymanager.cpp
    src/services/integrityservice.cpp
    src/services/batchoperationservice.cpp
    src/services/localpackagemanager.cpp
    src/services/updateservice.cpp
    src/services/selfupdater.cpp
    src/services/operationjournal.cpp
    src/services/appprotection.cpp
    src/services/imagecachemanager.cpp
    src/services/manifestsyncservice.cpp
    src/services/catalogstore.cpp
    src/services/detailfetcher.cpp
    src/services/telemetryservice.cpp
    src/services/translationdownloader.cpp
    src/services/supportedgamesmodel.cpp
    src/services/catalogproxymodel.cpp
    src/services/crashreporter.cpp
    src/services/networksecurity.cpp
    src/services/pluginmanager.cpp
    src/services/pluginmanifest.cpp
    src/services/plugininstaller.cpp
    src/services/pluginprocess.cpp
    src/services/pluginregistry.cpp
    src/services/elevatedops.cpp
    src/services/translationstatemanager.cpp
    src/services/installflowservice.cpp
    src/services/rendergovernor.cpp
    src/services/xdeltaapply.cpp
    src/services/companionservice.cpp
)

set(BACKEND_HEADERS
    src/services/gameservice.h
    src/services/steamdetailsservice.h
    src/services/communityservice.h
    src/services/settingsmanager.h
    src/services/backupmanager.h
    src/services/processscanner.h
    src/services/corebridge.h
    src/services/systemtraymanager.h
    src/services/integrityservice.h
    src/services/batchoperationservice.h
    src/services/localpackagemanager.h
    src/services/vdfparser.h
    src/services/updateservice.h
    src/services/selfupdater.h
    src/services/pathsecurity.h
    src/services/operationjournal.h
    src/services/appprotection.h
    src/services/obfstring.h
    src/services/imagecachemanager.h
    src/services/manifestsyncservice.h
    src/services/catalogstore.h
    src/services/detailfetcher.h
    src/services/telemetryservice.h
    src/services/translationdownloader.h
    src/services/supportedgamesmodel.h
    src/services/catalogproxymodel.h
    src/services/crashreporter.h
    src/services/mkpkformat.h
    src/services/apppaths.h
    src/services/profiler.h
    src/services/pluginmanager.h
    src/services/pluginmanifest.h
    src/services/plugininstaller.h
    src/services/pluginprocess.h
    src/services/pluginregistry.h
    src/services/pluginwire.h
    src/services/translationstatemanager.h
    src/services/installflowservice.h
    src/services/xdeltaapply.h
    src/services/companionservice.h
)

# encryption_key.h is generated at build time (not in repo) — only needed for full builds
if(NOT MAKINE_UI_ONLY)
    list(APPEND BACKEND_HEADERS src/services/encryption_key.h)
endif()

# QML module
qt_add_executable(MakineLauncher
    src/main.cpp
    ${BACKEND_SOURCES}
    ${BACKEND_HEADERS}
    ${CORE_SOURCES}
)
set_target_properties(MakineLauncher PROPERTIES OUTPUT_NAME "Makine-Launcher")

# Windows resource file: icon, manifest (DPI/UTF-8/visual styles), version info
# Generated from app.rc.in — version strings come from CMake
if(WIN32)
    configure_file(resources/app.rc.in "${CMAKE_CURRENT_BINARY_DIR}/app.rc" @ONLY)
    target_sources(MakineLauncher PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/app.rc")
endif()

# Mark singleton files BEFORE qt_add_qml_module
set_source_files_properties(qml/theme/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(qml/theme/Dimensions.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(qml/theme/Icons.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(MakineLauncher
    URI MakineLauncher
    VERSION 1.0
    QML_FILES
        # Main Screens
        qml/Main.qml
        qml/HomeScreen.qml
        qml/SettingsScreen.qml
        qml/GameDetailScreen.qml
        qml/OnboardingWizard.qml
        qml/screens/onboarding/WelcomeStep.qml
        qml/screens/onboarding/GamesStep.qml
        qml/screens/onboarding/PreferencesStep.qml

        # Screens - Home Pages
        qml/screens/HomePage.qml
        qml/screens/home/HomeCover.qml
        qml/screens/home/Shelf.qml
        qml/screens/home/FeaturedHero.qml
        qml/screens/home/RunningGameCard.qml
        qml/screens/home/MakineCorner.qml
        qml/screens/Library.qml
        qml/screens/AllGames.qml
        qml/screens/HelpScreen.qml
        qml/screens/WhatsNewScreen.qml
        qml/screens/CommunityScreen.qml

        # Screens - Game Detail Sections
        qml/screens/detail/HeroSection.qml
        qml/screens/detail/TranslationActionButton.qml
        qml/screens/detail/AboutSection.qml
        qml/screens/detail/UpdateBanner.qml
        qml/screens/detail/GameFolderBar.qml
        qml/screens/detail/ProgramConsole.qml
        qml/screens/detail/ContributorsSection.qml
        qml/screens/detail/RuntimeSection.qml
        qml/screens/detail/InfoTilesBar.qml

        # Screens - Settings Panels
        qml/screens/settings/GeneralSettings.qml
        qml/screens/settings/DisplaySettings.qml
        qml/screens/settings/PluginsSettings.qml
        qml/screens/settings/PluginSettingsPage.qml
        qml/screens/settings/AboutSettings.qml

        # Screens - Onboarding Steps

        # Theme (singletons)
        qml/theme/Theme.qml
        qml/theme/Dimensions.qml
        qml/theme/Icons.qml                       # Generated by scripts/build_ui_fonts.py

        # ── Reusable Components (public API — used across multiple screens) ──
        #
        # Cards: Data display containers
        qml/components/GameToast.qml               # Overlay notification for detected games
        qml/components/FolderDropOverlay.qml       # Drop a game folder from Explorer onto a page
        #
        # Navigation: Window chrome and nav
        qml/components/Sidebar.qml                # App navigation (macOS source list)
        qml/components/SidebarStatus.qml          # Sidebar foot: launcher update, scan
        qml/components/WindowControls.qml         # Tray / minimise / close
        qml/components/AppNotice.qml              # Floating app-wide notice
        qml/components/StyledToolTip.qml          # Modern minimal tooltip
        #
        # Layout: Section containers
        qml/components/SectionContainer.qml       # Glassmorphic section wrapper
        #
        # Visual: Decorative and feedback
        qml/components/TurkishFlagBadge.qml       # TR flag indicator
        qml/components/RegionSelector.qml         # Full-screen rubber-band OCR region selector
        qml/components/TranslationOverlay.qml     # Frameless overlay window for OCR results
        qml/components/RefreshIndicator.qml       # SWR catalog refresh corner badge
        #
        # ── Internal Components (settings/detail screens only) ──
        #
        qml/components/SettingsCard.qml           # Settings group container
        qml/components/SettingsDivider.qml        # Settings section divider
        qml/components/ToggleSetting.qml          # Toggle switch setting
        qml/components/ClickableRow.qml           # Clickable list row
        qml/components/BatchOperationsPanel.qml   # Batch ops control
        qml/components/PerformanceMonitor.qml     # Dev-only perf overlay
        qml/components/FocusRing.qml              # Keyboard focus indicator
        qml/components/StyledScrollBar.qml        # Themed scrollbar
        # Design system (docs/design.md)
        qml/components/Icon.qml
        qml/components/Spinner.qml
        qml/components/AppBackground.qml
        qml/components/AppButton.qml
        qml/components/AppSwitch.qml
        qml/components/SegmentedControl.qml
        qml/components/MenuButton.qml
        qml/components/AppLogo.qml
        qml/components/SearchField.qml
        qml/components/Card.qml
        qml/components/EdgeFade.qml
        qml/components/FadeLine.qml
        qml/components/TopDissolve.qml
        qml/components/ListSection.qml
        qml/components/ListRow.qml
        qml/components/IconTile.qml
        qml/components/NavIcon.qml
        qml/components/PagerButton.qml
        qml/components/MotionIcon.qml
        qml/components/GlassButton.qml
        qml/components/StatusBadge.qml
        qml/components/PageHeader.qml

        # Controllers (Main.qml decomposition)
        qml/controllers/GameDataResolver.qml
        qml/controllers/GameDetailViewModel.qml
        qml/controllers/InstallFlowController.qml

        # Dialogs
        qml/dialogs/BaseDialog.qml
        qml/dialogs/SheetHeader.qml
        qml/dialogs/DialogCloseButton.qml
        qml/dialogs/AntiCheatWarningDialog.qml
        qml/dialogs/ConfirmDialog.qml
        qml/dialogs/VariantSelectionDialog.qml
        qml/dialogs/InstallOptionsDialog.qml
        qml/dialogs/UpdateAlertDialog.qml
        qml/dialogs/InstallNotesDialog.qml
        qml/dialogs/GameFolderDialog.qml

    RESOURCES
        # SVG icons
        resources/icons/arrow_right.svg
        resources/icons/discord.svg
        resources/icons/discord-white.svg
        resources/icons/lock.svg
        resources/icons/shield-check.svg
        resources/icons/steam.svg
        # PNG images
        resources/images/logo.png
        resources/images/logo_white.png
        resources/images/logo_anim.webp
        # Fonts (Inter - Google Fonts)
        resources/fonts/Inter-Regular.ttf
        resources/fonts/Inter-Medium.ttf
        resources/fonts/Inter-SemiBold.ttf
        resources/fonts/Inter-Bold.ttf
        resources/fonts/InterDisplay-Regular.ttf
        resources/fonts/InterDisplay-Medium.ttf
        resources/fonts/InterDisplay-SemiBold.ttf
        resources/fonts/InterDisplay-Bold.ttf
        # Icons (Lucide subset + Phosphor fill for tiles, scripts/build_ui_fonts.py)
        resources/fonts/MakineIcons.ttf
        resources/fonts/MakineSymbols.ttf
        # The launcher's release notes and plan (releasenotes.h)
        resources/whatsnew.json
)

# ===== Binary Size Optimization =====
# GC sections: compile each function/data into separate sections, then
# discard unreferenced ones at link time. Critical for static Qt builds
# where unused Qt code would otherwise bloat the binary.
if(MSVC)
    target_compile_options(MakineLauncher PRIVATE
        /GS           # Buffer security check (stack cookies)
        /guard:cf     # Control Flow Guard
        /sdl          # Additional security checks
    )
    target_link_options(MakineLauncher PRIVATE
        /DYNAMICBASE          # ASLR
        /NXCOMPAT             # DEP (Data Execution Prevention)
        /HIGHENTROPYVA        # 64-bit ASLR
        /CETCOMPAT            # Intel CET shadow stack
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(MakineLauncher PRIVATE
        -ffunction-sections          # Each function in its own section
        -fdata-sections              # Each data object in its own section
        -fvisibility=hidden          # Hide all symbols by default (smaller export table)
        -fvisibility-inlines-hidden  # Hide inline functions
        -fstack-protector-strong     # Stack smashing protection
        -D_FORTIFY_SOURCE=2          # Buffer overflow detection
        -Wformat -Wformat-security   # Format string warnings
    )
    target_link_options(MakineLauncher PRIVATE
        -Wl,--gc-sections            # Discard unreferenced sections at link time
        -Wl,--dynamicbase            # ASLR
        -Wl,--nxcompat               # DEP
        -Wl,--high-entropy-va        # 64-bit ASLR
    )
endif()

# Include directories
target_include_directories(MakineLauncher PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services
)

if(NOT MAKINE_UI_ONLY AND NOT TARGET makine_core)
    # Standalone: explicit include path needed
    target_include_directories(MakineLauncher PRIVATE
        ${MAKINE_CORE_DIR}/include
    )
    # Super-build: includes inherited from makine_core target (PUBLIC)
endif()

# Link Qt libraries (always required)
target_link_libraries(MakineLauncher PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickEffects
    Qt6::Network
    Qt6::Svg
    Qt6::Concurrent
    wintrust   # SelfUpdater: Authenticode check of downloaded updates
    crypt32    # DPAPI for plugin secret settings
    xdelta3    # VCDIFF decoder for the "xdelta" install step (third_party/)
)

# Makine, the companion, is closed source (MakineCeviri/Makine-Companion, private). He is built
# in when that checkout sits next to this repo (or MAKINE_COMPANION_DIR names one); without it the
# launcher builds as before, minus him and his settings. A release never goes out without him.
set(MAKINE_COMPANION_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../Makine-Companion" CACHE PATH
    "Makine-Companion checkout (private repo)")
if(EXISTS "${MAKINE_COMPANION_DIR}/CMakeLists.txt")
    set(MAKINE_COMPANION_TESTS ${MAKINE_BUILD_TESTS})
    add_subdirectory("${MAKINE_COMPANION_DIR}" "${CMAKE_BINARY_DIR}/companion")
    target_link_libraries(MakineLauncher PRIVATE makine_companion)
    target_compile_definitions(MakineLauncher PRIVATE MAKINE_HAS_COMPANION)
    message(STATUS "Makine:   built in (${MAKINE_COMPANION_DIR})")
elseif(MAKINE_RELEASE_VERIFIED)
    message(FATAL_ERROR "A release without Makine: clone MakineCeviri/Makine-Companion next to "
        "this repo, or set MAKINE_COMPANION_DIR")
else()
    message(STATUS "Makine:   not here (${MAKINE_COMPANION_DIR}), building without him")
endif()

# Static Qt: import QML plugins at link time (no-op for shared builds)
qt_import_qml_plugins(MakineLauncher)

# Link core dependencies (only for full build)
if(NOT MAKINE_UI_ONLY)
    if(TARGET makine_core)
        # Super-build: link directly (transitive dependencies handled automatically)
        target_link_libraries(MakineLauncher PRIVATE makine_core)
        target_link_libraries(MakineLauncher PRIVATE
            $<IF:$<TARGET_EXISTS:zstd::libzstd_shared>,zstd::libzstd_shared,zstd::libzstd_static>)
    else()
        # Standalone: link prebuilt library + explicit dependencies
        target_link_libraries(MakineLauncher PRIVATE
            ${MAKINE_CORE_LIB}
        )
        target_link_libraries(MakineLauncher PRIVATE
            OpenSSL::Crypto
            nlohmann_json::nlohmann_json
            $<IF:$<TARGET_EXISTS:zstd::libzstd_shared>,zstd::libzstd_shared,zstd::libzstd_static>
            spdlog::spdlog
        )
    endif()
else()
    # Define MAKINE_UI_ONLY macro for stub implementations
    target_compile_definitions(MakineLauncher PRIVATE MAKINE_UI_ONLY)
    # Core headers still needed for type definitions (plugin API, package catalog)
    target_include_directories(MakineLauncher PRIVATE ${MAKINE_CORE_DIR}/include)
endif()

# Version string — single source from top-level CMakeLists.txt
set(MAKINE_SENTRY_RELEASE "makine-launcher@${MAKINE_VERSION_FULL}")
target_compile_definitions(MakineLauncher PRIVATE
    MAKINE_APP_VERSION="${MAKINE_VERSION_FULL}"
    MAKINE_SENTRY_RELEASE="${MAKINE_SENTRY_RELEASE}"
)

# Dev tools (FrameTimer, PerfReporter, F3 overlay) — off in release builds
# These sources are conditionally compiled to keep release binary clean:
#   - PerfReporter: compiled when DEV_TOOLS or PROFILER is ON
#   - FrameTimer/SceneProfiler/MemoryProfiler: compiled only when DEV_TOOLS is ON
option(MAKINE_DEV_TOOLS "Enable dev-only performance tools (FrameTimer, PerfReporter, F3 overlay)" OFF)
if(MAKINE_DEV_TOOLS)
    target_compile_definitions(MakineLauncher PRIVATE MAKINE_DEV_TOOLS MAKINE_PERF_REPORT)
    target_sources(MakineLauncher PRIVATE
        src/services/perfreporter.cpp src/services/perfreporter.h
        src/services/frametimer.cpp   src/services/frametimer.h
        src/services/sceneprofiler.cpp src/services/sceneprofiler.h
        src/services/memoryprofiler.cpp src/services/memoryprofiler.h
    )
    message(STATUS "MakineLauncher: Dev tools enabled (FrameTimer + PerfReporter + F3 overlay)")
endif()

# Tracy profiler (optional, adds Tracy on top of dev tools)
if(MAKINE_PROFILER AND TARGET TracyClient)
    target_link_libraries(MakineLauncher PRIVATE TracyClient)
    target_compile_definitions(MakineLauncher PRIVATE TRACY_ENABLE)
    if(NOT MAKINE_DEV_TOOLS)
        # PerfReporter needed for Tracy zones but dev tools not enabled
        target_compile_definitions(MakineLauncher PRIVATE MAKINE_PERF_REPORT)
        target_sources(MakineLauncher PRIVATE
            src/services/perfreporter.cpp src/services/perfreporter.h
        )
    endif()
    message(STATUS "MakineLauncher: Tracy profiler linked")
elseif(MAKINE_PROFILER AND NOT MAKINE_DEV_TOOLS)
    # Standalone profiler mode (no dev tools, just PerfReporter)
    target_compile_definitions(MakineLauncher PRIVATE MAKINE_PERF_REPORT)
    target_sources(MakineLauncher PRIVATE
        src/services/perfreporter.cpp src/services/perfreporter.h
    )
    message(STATUS "MakineLauncher: PerfReporter enabled (standalone)")
endif()

# Sentry crash reporting (optional)
if(MAKINE_CRASH_REPORTING AND TARGET sentry)
    target_link_libraries(MakineLauncher PRIVATE sentry)

    # Read DSN from environment — never hardcode in source (SEC-13)
    if(DEFINED ENV{MAKINE_SENTRY_DSN} AND NOT "$ENV{MAKINE_SENTRY_DSN}" STREQUAL "")
        set(_sentry_dsn "$ENV{MAKINE_SENTRY_DSN}")
    else()
        # Fall back to .env. Only `just` loads that file, so building with a
        # bare `cmake --preset dev` silently produced SENTRY_DSN="" — the app
        # started, sentry_init succeeded, and every report went nowhere. The
        # build looked perfectly healthy while telemetry was dead, which is the
        # worst possible failure mode for a diagnostics feature. Reading the
        # file here makes the DSN independent of how the build was invoked.
        set(_dotenv "${CMAKE_CURRENT_SOURCE_DIR}/../.env")
        if(EXISTS "${_dotenv}")
            file(STRINGS "${_dotenv}" _dsn_lines REGEX "^MAKINE_SENTRY_DSN=")
            if(_dsn_lines)
                list(GET _dsn_lines 0 _dsn_line)
                string(REGEX REPLACE "^MAKINE_SENTRY_DSN=" "" _sentry_dsn "${_dsn_line}")
                string(REGEX REPLACE "^[\"']|[\"']$" "" _sentry_dsn "${_sentry_dsn}")
                string(STRIP "${_sentry_dsn}" _sentry_dsn)
            endif()
        endif()
    endif()

    if(_sentry_dsn STREQUAL "")
        # Loud, because a quiet warning here is exactly what got missed before.
        message(WARNING
            "================================================================\n"
            "  SENTRY DSN BULUNAMADI — telemetri CALISMAYACAK.\n"
            "  Kod derlenir, uygulama acilir, raporlar hicbir yere gitmez.\n"
            "  Cozum: .env dosyasina MAKINE_SENTRY_DSN=... ekleyin\n"
            "         veya ortam degiskeni olarak tanimlayin.\n"
            "  Dogrulama: Makine-Launcher.exe --selftest-telemetry\n"
            "================================================================")
    else()
        message(STATUS "MakineLauncher: Sentry DSN configured (telemetry active)")
    endif()

    target_compile_definitions(MakineLauncher PRIVATE
        MAKINE_HAS_SENTRY
        SENTRY_DSN="${_sentry_dsn}"
    )
    message(STATUS "MakineLauncher: Sentry crash reporting linked")

    # Copy crashpad_handler.exe next to binary (crashpad backend only)
    if(NOT MINGW AND TARGET crashpad_handler)
        add_custom_command(TARGET MakineLauncher POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:crashpad_handler>
                $<TARGET_FILE_DIR:MakineLauncher>/crashpad_handler.exe
            COMMENT "Copying crashpad_handler.exe"
        )
    endif()
endif()

# Windows specific
if(WIN32)
    set_target_properties(MakineLauncher PROPERTIES
        WIN32_EXECUTABLE TRUE
    )

    # Windows definitions
    target_compile_definitions(MakineLauncher PRIVATE
        UNICODE
        _UNICODE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )

    # Security hardening & AV false-positive reduction
    if(MSVC)
        target_compile_options(MakineLauncher PRIVATE
            /GS             # Buffer security check (stack cookies)
            /guard:cf       # Control Flow Guard
            /sdl            # Additional security checks
            /Qspectre       # Spectre mitigation
        )
        target_link_options(MakineLauncher PRIVATE
            /DYNAMICBASE        # ASLR
            /HIGHENTROPYVA      # High-entropy 64-bit ASLR
            /NXCOMPAT           # Data Execution Prevention (DEP)
            /INTEGRITYCHECK     # PE integrity check — AV trusts this
            /CETCOMPAT          # Intel CET shadow stack compatibility
        )
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(MakineLauncher PRIVATE
            -fstack-protector-strong
            -D_FORTIFY_SOURCE=2
        )
        target_link_options(MakineLauncher PRIVATE
            -Wl,--dynamicbase       # ASLR
            -Wl,--high-entropy-va   # High-entropy ASLR
            -Wl,--nxcompat          # DEP
        )
    endif()

    # Link Windows libraries for process scanning, system tray, and crypto
    target_link_libraries(MakineLauncher PRIVATE
        psapi
        Shlwapi
        Advapi32
        User32
        Crypt32
        Wintrust     # Authenticode verification
        dwmapi       # DWM acrylic/mica backdrop
        Shell32      # ShellExecuteEx — elevated ("runas") install steps
        Ole32        # CoInitializeEx, required by ShellExecuteEx
    )
endif()

# ===== Internationalization (i18n) =====
find_package(Qt6 COMPONENTS LinguistTools QUIET)
if(Qt6LinguistTools_FOUND)
    set(TS_FILES
        i18n/makine_en.ts
        i18n/makine_tr.ts
    )
    qt_add_translations(MakineLauncher
        TS_FILES ${TS_FILES}
        RESOURCE_PREFIX "/i18n"
    )
    message(STATUS "i18n: Translation files configured (${TS_FILES})")
else()
    message(STATUS "i18n: Qt6::LinguistTools not found, translations disabled")
endif()

# Keep an unstripped copy for Sentry, then strip the one we ship.
#
# Stripping in place destroyed the only copy of the symbols, so
# `sentry-cli debug-files upload` (scripts/deploy.py) had nothing to find and
# every crash from a shipped build came back with our own frames as "?". Twelve
# fatal issues across ~28 users are undiagnosable for this reason alone — the
# only readable frames are Windows' own, symbolicated from Microsoft's servers.
#
# Measured rather than assumed (2026-09-19, sentry-cli 3.2.2):
#   stripped .exe           → Debug ID + unwind, but `nm` finds 0 symbols;
#                             stacks walk, every frame is "?"
#   objcopy --only-keep-debug → 180k symbols but "missing debug identifier",
#                             so Sentry cannot pair it with the crashing module
#   unstripped copy         → both. This is the file to upload.
#
# --build-id is what gives the module a Debug ID at all; the stripped binary
# keeps that section, so the shipped file and the .sym copy identify as the same
# module. Packaging (scripts/make_msix.ps1) names the files it stages, so the
# .sym copy never reaches the installer and the shipped size is unchanged.
#
# The `nm` count above was the wrong test: Sentry reads DWARF, not the COFF
# symbol table, so 0.1.5's .sym (no -g) still put every frame of ours on the
# exe's only export, qml_register_types_MakineLauncher. -g1 adds function names
# and line tables — enough for a stack, and stripped from the shipped file.
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT MSVC)
    target_compile_options(MakineLauncher PRIVATE -g1)
    if(TARGET makine_core)
        target_compile_options(makine_core PRIVATE -g1)
    endif()
    target_link_options(MakineLauncher PRIVATE "-Wl,--build-id=sha1")

    add_custom_command(TARGET MakineLauncher POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                "$<TARGET_FILE:MakineLauncher>" "$<TARGET_FILE:MakineLauncher>.sym"
        COMMAND ${CMAKE_STRIP} "$<TARGET_FILE:MakineLauncher>"
        COMMENT "Keeping Makine-Launcher.exe.sym for Sentry, stripping the shipped binary"
        VERBATIM
    )
endif()

# Optional: UPX compression for even smaller static binaries (~40-50% reduction)
# Enable with: cmake -DMAKINE_UPX=ON
option(MAKINE_UPX "Compress static binary with UPX" OFF)
if(MAKINE_UPX AND CMAKE_EXE_LINKER_FLAGS MATCHES "-static")
    find_program(UPX_EXECUTABLE upx)
    if(UPX_EXECUTABLE)
        add_custom_command(TARGET MakineLauncher POST_BUILD
            COMMAND ${UPX_EXECUTABLE} --best --lzma "$<TARGET_FILE:MakineLauncher>"
            COMMENT "UPX compressing static binary (--best --lzma)"
            VERBATIM
        )
        message(STATUS "UPX: Enabled (${UPX_EXECUTABLE})")
    else()
        message(WARNING "UPX: Requested but upx not found in PATH")
    endif()
endif()

# Post-build: generate SHA-256 hash file for integrity verification
add_custom_command(TARGET MakineLauncher POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E sha256sum "$<TARGET_FILE:MakineLauncher>"
            > "$<TARGET_FILE:MakineLauncher>.sha256"
    COMMENT "Generating binary integrity hash"
    VERBATIM
)

# Install
install(TARGETS MakineLauncher
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
