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
    rm -rf core/build/ qml/build/

# Clean and rebuild
rebuild: clean all

# ============================================================================
# DEPLOYMENT
# ============================================================================

# Deploy QML app with Qt dependencies
deploy: release
    @echo "Deploying QML app..."
    mkdir -p dist
    cp qml/build/release/MakineAI.exe dist/
    windeployqt --qmldir qml/qml --release dist/MakineAI.exe

# Create release archive
package: deploy
    @echo "Creating release package..."
    powershell Compress-Archive -Path dist/* -DestinationPath MakineAI-release.zip -Force

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

# Format code (requires clang-format)
format:
    @echo "Formatting C++ code..."
    find core qml -name "*.cpp" -o -name "*.hpp" -o -name "*.h" | xargs clang-format -i

# Check code format
check-format:
    @echo "Checking code format..."
    find core qml -name "*.cpp" -o -name "*.hpp" -o -name "*.h" | xargs clang-format --dry-run --Werror
