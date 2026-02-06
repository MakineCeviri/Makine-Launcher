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

# Install vcpkg dependencies
setup:
    @echo "Installing vcpkg dependencies..."
    vcpkg install --triplet x64-windows

# Install vcpkg dependencies with tests
setup-tests:
    @echo "Installing vcpkg dependencies with tests..."
    vcpkg install --triplet x64-windows
    vcpkg install gtest:x64-windows

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

# Build QML app - fast dev (MinGW, Release, UI-only)
dev:
    cmake --preset dev
    cmake --build --preset dev

# Build QML app - debug (MinGW, Debug, UI-only)
debug:
    cmake --preset debug
    cmake --build --preset debug

# Build QML app - full release (vcpkg, core integration)
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

# Build everything (core + dev)
all: core dev

# Build full release (core + release)
all-release: core release

# ============================================================================
# CLEANING
# ============================================================================

# Clean all build directories
clean:
    @echo "Cleaning build directories..."
    powershell -Command "Remove-Item -Recurse -Force -ErrorAction SilentlyContinue core/build, qml/build"

# Clean and rebuild
rebuild: clean all

# ============================================================================
# DEPLOYMENT
# ============================================================================

# Deploy QML app with Qt dependencies
deploy: release
    @echo "Deploying QML app..."
    powershell -Command "New-Item -ItemType Directory -Force -Path dist | Out-Null"
    powershell -Command "Copy-Item qml/build/release/MakineAI.exe dist/"
    windeployqt --qmldir qml/qml --release dist/MakineAI.exe

# Create release archive
package: deploy
    @echo "Creating release package..."
    powershell -Command "Compress-Archive -Path dist/* -DestinationPath MakineAI-release.zip -Force"

# ============================================================================
# DEVELOPMENT
# ============================================================================

# Run the app (dev - fast)
run: dev
    ./qml/build/dev/MakineAI.exe

# Run the app (debug)
run-debug: debug
    ./qml/build/debug/MakineAI.exe

# Run the app (release)
run-release: release
    ./qml/build/release/MakineAI.exe

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
