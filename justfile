# MakineAI Build System
# Usage: just <recipe>
# Install just: cargo install just (or winget install just)
#
# Presets (CMakePresets.json):
#   dev     = MinGW, Release, UI-only (fast iteration)
#   debug   = MinGW, Debug, UI-only
#   release = vcpkg, Release, full core integration
#   core    = vcpkg, Release, core library only

# Default recipe - show help
default:
    @just --list

# ============================================================================
# SETUP
# ============================================================================

# Install vcpkg dependencies (MinGW)
setup:
    @echo "Installing vcpkg dependencies (MinGW)..."
    vcpkg install boost-filesystem:x64-mingw-dynamic openssl:x64-mingw-dynamic curl:x64-mingw-dynamic nlohmann-json:x64-mingw-dynamic lz4:x64-mingw-dynamic zlib:x64-mingw-dynamic zstd:x64-mingw-dynamic sqlite3:x64-mingw-dynamic spdlog:x64-mingw-dynamic simdjson:x64-mingw-dynamic mio:x64-mingw-dynamic taskflow:x64-mingw-dynamic concurrentqueue:x64-mingw-dynamic simdutf:x64-mingw-dynamic sqlitecpp:x64-mingw-dynamic libsodium:x64-mingw-dynamic libarchive:x64-mingw-dynamic bit7z:x64-mingw-dynamic efsw:x64-mingw-dynamic

# Install vcpkg dependencies with tests
setup-tests: setup
    @echo "Installing test dependencies..."
    vcpkg install gtest:x64-mingw-dynamic

# ============================================================================
# CORE LIBRARY (vcpkg, Release)
# ============================================================================

# Build core library
core:
    cmake --preset core
    cmake --build --preset core

# ============================================================================
# QML APPLICATION
# ============================================================================

# Build full app (Core + UI, MinGW, Release)
dev:
    cmake --preset dev
    cmake --build --preset dev

# Build UI-only (fast, no vcpkg deps)
dev-ui:
    cmake --preset dev-ui
    cmake --build --preset dev-ui

# Build full app debug (Core + UI, MinGW, Debug)
debug:
    cmake --preset debug
    cmake --build --preset debug

# Build full release - super-build (Core + QML in single pipeline)
release:
    cmake --preset release
    cmake --build --preset release

# ============================================================================
# TESTING
# ============================================================================

# Run core tests
test: core
    ctest --preset core-tests

# Run tests with verbose output
test-verbose: core
    ctest --preset core-tests --verbose

# ============================================================================
# ALL BUILDS
# ============================================================================

# Build everything (core + UI)
all: dev

# Build full release (super-build handles both core + qml)
all-release: release

# ============================================================================
# CLEANING
# ============================================================================

# Clean all build directories
clean:
    @echo "Cleaning build directories..."
    powershell -Command "Remove-Item -Recurse -Force -ErrorAction SilentlyContinue build, core/build, qml/build"

# Clean and rebuild
rebuild: clean all

# ============================================================================
# STATIC BUILD (Single EXE — no DLLs)
# ============================================================================

# Build static Qt from source (one-time setup, ~1-2 hours)
# Prerequisite: Install "Qt 6.10.1 > Sources" via Qt Online Installer
setup-static-qt:
    powershell -ExecutionPolicy Bypass -File scripts/build_static_qt.ps1

# Build single-file static EXE (no Qt/MinGW DLLs needed)
release-static:
    cmake --preset release-static
    cmake --build --preset release-static

# Run static release build
run-static: release-static
    ./build/release-static/MakineAI.exe

# ============================================================================
# TRANSLATION PACKAGE PUBLISHING
# ============================================================================

# Publish a single translation package (compress + encrypt + upload + manifest)
publish app-id:
    python scripts/deploy.py --app-id {{app-id}}

# Publish all translation packages
publish-all:
    python scripts/deploy.py

# Preview publish (no changes made)
publish-dry app-id:
    python scripts/deploy.py --app-id {{app-id}} --dry-run

# ============================================================================
# SENTRY (one-time setup)
# ============================================================================

# Configure Sentry GitHub integration + alert rules
sentry-setup:
    python scripts/sentry_setup.py

# Preview Sentry setup (no changes)
sentry-setup-dry:
    python scripts/sentry_setup.py --dry-run

# ============================================================================
# DEPLOYMENT
# ============================================================================

# Deploy QML app with Qt dependencies
deploy: release
    @echo "Deploying QML app..."
    powershell -Command "New-Item -ItemType Directory -Force -Path dist | Out-Null"
    powershell -Command "Copy-Item build/release/MakineAI.exe dist/"
    windeployqt --qmldir qml/qml --release dist/MakineAI.exe

# Create release archive (shared Qt build — includes DLLs)
package: deploy
    @echo "Creating release package..."
    powershell -Command "Compress-Archive -Path dist/* -DestinationPath MakineAI-release.zip -Force"

# Create release archive (static build — single EXE)
package-static: release-static
    @echo "Creating static release package..."
    powershell -Command "New-Item -ItemType Directory -Force -Path dist-static | Out-Null"
    powershell -Command "Copy-Item qml/build/release-static/MakineAI.exe dist-static/"
    powershell -Command "Compress-Archive -Path dist-static/* -DestinationPath MakineAI-static.zip -Force"
    @echo "Done: MakineAI-static.zip (single EXE)"

# ============================================================================
# CODE SIGNING (Antivirus false-positive prevention)
# ============================================================================

# One-time: Create development code signing certificate (run as admin)
setup-cert:
    @echo "Creating MakineAI development code signing certificate..."
    powershell -ExecutionPolicy Bypass -File scripts/create_dev_cert.ps1

# Sign all built EXEs (auto-finds cert from scripts/certs/)
sign:
    @echo "Signing MakineAI executables..."
    powershell -ExecutionPolicy Bypass -File scripts/sign_exe.ps1

# Sign a specific file
sign-file path:
    powershell -ExecutionPolicy Bypass -File scripts/sign_exe.ps1 -Path "{{path}}"

# Build static + sign (release pipeline)
release-signed: release-static
    @echo "Signing release..."
    powershell -ExecutionPolicy Bypass -File scripts/sign_exe.ps1 -Path "qml/build/release-static/MakineAI.exe"
    @echo "Done: signed single EXE ready for distribution"

# ============================================================================
# PROFILING (Tracy)
# ============================================================================

# Build with Tracy profiler + PerfReporter instrumentation
profile:
    cmake --preset dev-profile
    cmake --build --preset dev-profile

# Run profiler build (connect with Tracy GUI)
run-profile:
    ./build/dev-profile/MakineAI.exe

# Automated profile: build, run for N seconds, generate + show report
profile-auto duration="15":
    cmake --preset dev-profile
    cmake --build --preset dev-profile
    @echo "Running MakineAI for {{duration}} seconds..."
    ./build/dev-profile/MakineAI.exe --profile-duration={{duration}}
    @echo ""
    python scripts/perf_report.py

# Tracy trace capture: build, capture trace, export CSV for analysis
profile-tracy duration="12":
    cmake --preset dev-profile
    cmake --build --preset dev-profile
    @echo "Starting Tracy capture + MakineAI ({{duration}}s)..."
    C:/cedra/tools/tracy-0.13.1/tracy-capture.exe -o C:/cedra/tools/trace.tracy -s {{duration}} &
    sleep 2 && ./build/dev-profile/MakineAI.exe --profile-duration={{duration}}
    @echo ""
    @echo "=== Zone Statistics ==="
    C:/cedra/tools/tracy-0.13.1/tracy-csvexport.exe C:/cedra/tools/trace.tracy
    @echo ""
    @echo "Trace saved: C:/cedra/tools/trace.tracy"
    @echo "Open in Tracy GUI: C:/cedra/tools/tracy-0.13.1/tracy-profiler.exe"

# Show latest performance report (markdown formatted)
perf-report:
    python scripts/perf_report.py

# ============================================================================
# DEVELOPMENT
# ============================================================================

# Run the app (dev)
run: dev
    ./build/dev/MakineAI.exe

# Run the app (debug)
run-debug: debug
    ./qml/build/debug/MakineAI.exe

# Run the app (release)
run-release: release
    ./build/release/MakineAI.exe

# ============================================================================
# CODE QUALITY
# ============================================================================

# Format C++ code (requires clang-format)
format:
    @echo "Formatting C++ code..."
    powershell -Command "Get-ChildItem -Recurse -Include *.cpp,*.hpp,*.h -Path core,qml/src | ForEach-Object { clang-format -i $_.FullName; Write-Host ('  ' + $_.Name) }"
    @echo "Done."

# Check code format without modifying
check-format:
    @echo "Checking code format..."
    powershell -Command "Get-ChildItem -Recurse -Include *.cpp,*.hpp,*.h -Path core,qml/src | ForEach-Object { clang-format --dry-run --Werror $_.FullName }"

# Run clang-tidy static analysis
lint:
    @echo "Running clang-tidy analysis..."
    powershell -Command "Get-ChildItem -Recurse -Include *.cpp -Path core/src | ForEach-Object { Write-Host ('Analyzing: ' + $_.Name); clang-tidy -p core/build $_.FullName 2>&1 | Select-String -Pattern 'warning:|error:' }"

# Run all quality checks
check: check-format lint
    @echo "All quality checks completed."

# ============================================================================
# UTILITIES
# ============================================================================

# Show project statistics
stats:
    @echo "=== MakineAI Project Statistics ==="
    @echo ""
    @echo "C++ Source Files:"
    powershell -Command "$cpp = Get-ChildItem -Recurse -Include *.cpp,*.hpp,*.h -Path core/src,core/include,qml/src; Write-Host ('  Files: ' + $cpp.Count); $lines = ($cpp | Get-Content | Measure-Object -Line).Lines; Write-Host ('  Lines: ' + $lines)"
    @echo ""
    @echo "QML Files:"
    powershell -Command "$qml = Get-ChildItem -Recurse -Include *.qml -Path qml; Write-Host ('  Files: ' + $qml.Count); $lines = ($qml | Get-Content | Measure-Object -Line).Lines; Write-Host ('  Lines: ' + $lines)"
    @echo ""
    @echo "Test Files:"
    powershell -Command "$tests = Get-ChildItem -Recurse -Include test_*.cpp,*_test.cpp -Path core/tests; Write-Host ('  Files: ' + $tests.Count); $lines = ($tests | Get-Content | Measure-Object -Line).Lines; Write-Host ('  Lines: ' + $lines)"
    @echo ""
    @echo "Documentation:"
    powershell -Command "$docs = Get-ChildItem -Recurse -Include *.md -Path docs; Write-Host ('  Files: ' + $docs.Count)"

# Show tool versions and system info
info:
    @echo "=== MakineAI Development Environment ==="
    @echo ""
    -cmake --version 2>NUL | powershell -Command "$input | Select-Object -First 1"
    -ninja --version 2>NUL
    -vcpkg version 2>NUL | powershell -Command "$input | Select-Object -First 1"
    -git --version
    -clang-format --version 2>NUL
    @echo ""

# Generate documentation (requires Doxygen)
docs:
    @echo "Generating documentation..."
    cd core && doxygen Doxyfile
    @echo "Documentation generated in core/docs/html/"

# Pre-push quality check
ci-check: check-format core test
    @echo "All CI checks passed!"

# ============================================================================
# SHOWCASE IMAGES
# ============================================================================

# Validate Steam App IDs and download showcase images
download-images:
    @echo "Validating Steam App IDs and downloading images..."
    python scripts/download_showcase_images.py

# Force re-download all images (ignore cache)
download-images-force:
    @echo "Force re-downloading all showcase images..."
    python scripts/download_showcase_images.py --force
