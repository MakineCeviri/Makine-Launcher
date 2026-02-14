# build_static_qt.ps1
# Build Qt 6.10.1 from source as fully static libraries (MinGW)
#
# Prerequisites:
#   - Qt Online Installer: install "Qt 6.10.1 > Sources" component
#   - MinGW 13.1.0 already at C:\Qt\Tools\mingw1310_64
#   - Ninja already at C:\Qt\Tools\Ninja
#   - CMake already at C:\Qt\Tools\CMake_64
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File scripts\build_static_qt.ps1
#
# Output: C:\Qt\6.10.1\mingw_64_static

param(
    [string]$QtSrcDir = "C:\Qt\6.10.1\Src",
    [string]$Prefix   = "C:\Qt\6.10.1\mingw_64_static",
    [string]$BuildDir = "C:\Qt\6.10.1\_build_static",
    [string]$MinGW    = "C:\Qt\Tools\mingw1310_64",
    [string]$CMakeDir = "C:\Qt\Tools\CMake_64\bin",
    [string]$NinjaDir = "C:\Qt\Tools\Ninja"
)

$ErrorActionPreference = "Stop"

# ------------------------------------------------------------------
# Validation
# ------------------------------------------------------------------

if (-not (Test-Path "$QtSrcDir\configure.bat")) {
    Write-Host ""
    Write-Host "ERROR: Qt 6.10.1 source not found at $QtSrcDir" -ForegroundColor Red
    Write-Host ""
    Write-Host "To install Qt sources:" -ForegroundColor Yellow
    Write-Host "  1. Open Qt Maintenance Tool (C:\Qt\MaintenanceTool.exe)"
    Write-Host "  2. Select 'Add or remove components'"
    Write-Host "  3. Expand Qt 6.10.1 > check 'Sources'"
    Write-Host "  4. Click 'Update'"
    Write-Host ""
    exit 1
}

if (-not (Test-Path "$MinGW\bin\g++.exe")) {
    Write-Host "ERROR: MinGW not found at $MinGW" -ForegroundColor Red
    exit 1
}

if (-not $env:VULKAN_SDK -or -not (Test-Path "$env:VULKAN_SDK\Include\vulkan\vulkan.h")) {
    Write-Host ""
    Write-Host "ERROR: Vulkan SDK not found!" -ForegroundColor Red
    Write-Host ""
    Write-Host "Vulkan SDK is required for the Vulkan rendering backend." -ForegroundColor Yellow
    Write-Host "  1. Download from: https://vulkan.lunarg.com/sdk/home" -ForegroundColor Yellow
    Write-Host "  2. Run the installer (sets VULKAN_SDK automatically)" -ForegroundColor Yellow
    Write-Host "  3. Re-open PowerShell and run this script again" -ForegroundColor Yellow
    Write-Host ""
    exit 1
}

if (Test-Path $Prefix) {
    Write-Host "WARNING: $Prefix already exists." -ForegroundColor Yellow
    $reply = Read-Host "Delete and rebuild? (y/N)"
    if ($reply -ne "y") {
        Write-Host "Aborted."
        exit 0
    }
    Remove-Item -Recurse -Force $Prefix
}

# ------------------------------------------------------------------
# Environment
# ------------------------------------------------------------------

$env:PATH = "$CMakeDir;$NinjaDir;$MinGW\bin;$env:PATH"

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  Qt 6.10.1 Static Build (MinGW x64)"       -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  Source:  $QtSrcDir"
Write-Host "  Prefix:  $Prefix"
Write-Host "  Build:   $BuildDir"
Write-Host "  MinGW:   $MinGW"
Write-Host ""

# Verify tools
& cmake --version | Select-Object -First 1
& ninja --version
& g++ --version | Select-Object -First 1
Write-Host ""

# ------------------------------------------------------------------
# Configure
# ------------------------------------------------------------------

# Create build directory
if (Test-Path $BuildDir) { Remove-Item -Recurse -Force $BuildDir }
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

Write-Host "Configuring Qt (this takes a few minutes)..." -ForegroundColor Green

$configArgs = @(
    "-release"
    "-static"
    "-static-runtime"
    "-platform", "win32-g++"
    "-prefix", $Prefix

    # Optimization: size-oriented
    "-optimize-size"

    # TLS: use Windows native Schannel (no OpenSSL dependency)
    "-schannel"
    "-no-openssl"

    # Use bundled third-party libraries (no external deps)
    "-qt-zlib"
    "-qt-freetype"
    "-qt-harfbuzz"
    "-qt-libpng"
    "-qt-libjpeg"
    "-qt-pcre"

    # Skip everything we don't need (saves 30-60 min build time)
    "-skip", "qt3d"
    "-skip", "qtcharts"
    "-skip", "qtcoap"
    "-skip", "qtconnectivity"
    "-skip", "qtdatavis3d"
    "-skip", "qtgraphs"
    "-skip", "qtgrpc"
    "-skip", "qthttpserver"
    "-skip", "qtlocation"
    "-skip", "qtlottie"
    "-skip", "qtmqtt"
    "-skip", "qtmultimedia"
    "-skip", "qtopcua"
    "-skip", "qtpositioning"
    "-skip", "qtquick3d"
    "-skip", "qtquick3dphysics"
    "-skip", "qtquicktimeline"
    "-skip", "qtremoteobjects"
    "-skip", "qtscxml"
    "-skip", "qtsensors"
    "-skip", "qtserialbus"
    "-skip", "qtserialport"
    "-skip", "qtspeech"
    "-skip", "qtvirtualkeyboard"
    "-skip", "qtwayland"
    "-skip", "qtwebchannel"
    "-skip", "qtwebengine"
    "-skip", "qtwebsockets"
    "-skip", "qtwebview"

    # Don't build examples or tests
    "-nomake", "examples"
    "-nomake", "tests"
    "-nomake", "benchmarks"

    # Disable features we don't use
    "-no-feature-assistant"
    "-no-feature-designer"
    "-no-feature-sql"
    "-no-feature-testlib"
    "-no-feature-printsupport"
    "-no-dbus"

    # CMake generator and compiler
    "--", "-G", "Ninja"
    "-DCMAKE_C_COMPILER=$MinGW/bin/gcc.exe"
    "-DCMAKE_CXX_COMPILER=$MinGW/bin/g++.exe"
    # Vulkan SDK (required for Vulkan RHI backend in static builds)
    "-DVulkan_INCLUDE_DIR=$env:VULKAN_SDK/Include"
    "-DVulkan_LIBRARY=$env:VULKAN_SDK/Lib/vulkan-1.lib"
)

Push-Location $BuildDir
try {
    & "$QtSrcDir\configure.bat" @configArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: Qt configure failed!" -ForegroundColor Red
        exit 1
    }
} finally {
    Pop-Location
}

# ------------------------------------------------------------------
# Build
# ------------------------------------------------------------------

Write-Host ""
Write-Host "Building Qt (this takes 1-2 hours)..." -ForegroundColor Green
Write-Host "Start time: $(Get-Date -Format 'HH:mm:ss')"
Write-Host ""

Push-Location $BuildDir
try {
    # Use all available cores
    $cores = [Environment]::ProcessorCount
    & cmake --build . --parallel $cores
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: Qt build failed!" -ForegroundColor Red
        exit 1
    }
} finally {
    Pop-Location
}

# ------------------------------------------------------------------
# Install
# ------------------------------------------------------------------

Write-Host ""
Write-Host "Installing to $Prefix..." -ForegroundColor Green

Push-Location $BuildDir
try {
    & cmake --install .
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: Qt install failed!" -ForegroundColor Red
        exit 1
    }
} finally {
    Pop-Location
}

# ------------------------------------------------------------------
# Cleanup build directory (saves ~10-20 GB)
# ------------------------------------------------------------------

Write-Host ""
$reply = Read-Host "Delete build directory to free disk space? ($BuildDir) (Y/n)"
if ($reply -ne "n") {
    Write-Host "Cleaning up build directory..."
    Remove-Item -Recurse -Force $BuildDir
}

# ------------------------------------------------------------------
# Verify
# ------------------------------------------------------------------

Write-Host ""
Write-Host "============================================" -ForegroundColor Green
Write-Host "  Qt 6.10.1 Static Build Complete!"          -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Green
Write-Host "  Installed to: $Prefix"
Write-Host ""

# Quick sanity check
if (Test-Path "$Prefix\lib\cmake\Qt6\Qt6Config.cmake") {
    Write-Host "  Qt6Config.cmake found" -ForegroundColor Green
} else {
    Write-Host "  WARNING: Qt6Config.cmake not found!" -ForegroundColor Yellow
}

if (Test-Path "$Prefix\lib\libQt6Core.a") {
    $size = (Get-Item "$Prefix\lib\libQt6Core.a").Length / 1MB
    Write-Host "  libQt6Core.a = $([math]::Round($size, 1)) MB (static)" -ForegroundColor Green
} else {
    Write-Host "  WARNING: libQt6Core.a not found!" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "Next step: just release-static" -ForegroundColor Cyan
Write-Host ""
