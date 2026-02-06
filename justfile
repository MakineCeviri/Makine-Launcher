# MakineAI Build System
# Usage: just <recipe>
# Install just: cargo install just (or winget install just)

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
# CORE LIBRARY
# ============================================================================

# Configure core library (debug)
configure-core-debug:
    cmake --preset core-debug

# Configure core library (release)
configure-core-release:
    cmake --preset core-release

# Build core library (debug)
build-core-debug: configure-core-debug
    cmake --build --preset core-debug

# Build core library (release)
build-core-release: configure-core-release
    cmake --build --preset core-release

# Build core (alias for release)
core: build-core-release

# ============================================================================
# QML APPLICATION
# ============================================================================

# Configure QML app (debug)
configure-qml-debug:
    cmake --preset qml-debug

# Configure QML app (release)
configure-qml-release:
    cmake --preset qml-release

# Build QML app (debug)
build-qml-debug: configure-qml-debug
    cmake --build --preset qml-debug

# Build QML app (release)
build-qml-release: configure-qml-release
    cmake --build --preset qml-release

# Build QML (alias for release)
qml: build-qml-release

# ============================================================================
# TESTING
# ============================================================================

# Run core tests
test: build-core-debug
    ctest --preset core-tests

# Run tests with verbose output
test-verbose: build-core-debug
    ctest --preset core-tests --verbose

# ============================================================================
# ALL BUILDS
# ============================================================================

# Build everything (release)
all: core qml

# Build everything (debug)
all-debug: build-core-debug build-qml-debug

# ============================================================================
# CLEANING
# ============================================================================

# Clean build directories
clean:
    @echo "Cleaning build directories..."
    rm -rf build/

# Clean and rebuild
rebuild: clean all

# ============================================================================
# DEPLOYMENT
# ============================================================================

# Deploy QML app with Qt dependencies
deploy: qml
    @echo "Deploying QML app..."
    mkdir -p dist
    cp build/qml-release/MakineAI.exe dist/
    windeployqt --qmldir qml/qml --release dist/MakineAI.exe

# Create release archive
package: deploy
    @echo "Creating release package..."
    powershell Compress-Archive -Path dist/* -DestinationPath MakineAI-release.zip -Force

# ============================================================================
# DEVELOPMENT
# ============================================================================

# Run the app (debug)
run: build-qml-debug
    ./build/qml-debug/MakineAI.exe

# Run the app (release)
run-release: qml
    ./build/qml-release/MakineAI.exe

# Format code (requires clang-format)
format:
    @echo "Formatting C++ code..."
    find core qml -name "*.cpp" -o -name "*.hpp" -o -name "*.h" | xargs clang-format -i

# Check code format
check-format:
    @echo "Checking code format..."
    find core qml -name "*.cpp" -o -name "*.hpp" -o -name "*.h" | xargs clang-format --dry-run --Werror

# ============================================================================
# VISUAL STUDIO
# ============================================================================

# Generate VS2022 solution for core
vs-core:
    cmake --preset vs2022-core

# Generate VS2022 solution for QML
vs-qml:
    cmake --preset vs2022-qml

# Open VS2022 solution (core)
open-vs-core: vs-core
    start build/vs2022-core/MakineAI.sln

# Open VS2022 solution (QML)
open-vs-qml: vs-qml
    start build/vs2022-qml/MakineAI.sln
