# build_static_qt.ps1
# Build Qt 6.10.1 from source as fully static libraries (MinGW)
#
# Builds modules individually in dependency order:
#   qtbase → qtshadertools → qtsvg → qtdeclarative → qttools → qttranslations
#
# Prerequisites:
#   - Qt Online Installer: install "Qt 6.10.1 > Sources" component
#   - MinGW 13.1.0 already at C:\Qt\Tools\mingw1310_64
#   - Ninja already at C:\Qt\Tools\Ninja
#   - CMake already at C:\Qt\Tools\CMake_64
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File scripts\build_static_qt.ps1
#   powershell -ExecutionPolicy Bypass -File scripts\build_static_qt.ps1 -NoCleanup
#
# Output: C:\Qt\6.10.1\mingw_64_static

param(
    [string]$QtSrcDir = "C:\Qt\6.10.1\Src",
    [string]$Prefix   = "C:\Qt\6.10.1\mingw_64_static",
    [string]$BuildDir = "C:\Qt\6.10.1\_build_static",
    [string]$MinGW    = "C:\Qt\Tools\mingw1310_64",
    [string]$CMakeDir = "C:\Qt\Tools\CMake_64\bin",
    [string]$NinjaDir = "C:\Qt\Tools\Ninja",
    [switch]$NoCleanup
)

$ErrorActionPreference = "Stop"
$startTime = Get-Date

# ------------------------------------------------------------------
# Validation
# ------------------------------------------------------------------

if (-not (Test-Path "$QtSrcDir\qtbase\configure.bat")) {
    Write-Host ""
    Write-Host "ERROR: Qt 6.10.1 qtbase source not found at $QtSrcDir\qtbase" -ForegroundColor Red
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
    Write-Host "Removing existing static Qt at $Prefix..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $Prefix
}

# ------------------------------------------------------------------
# Environment
# ------------------------------------------------------------------

$env:PATH = "$CMakeDir;$NinjaDir;$MinGW\bin;$env:PATH"

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  Qt 6.10.1 Static Build (MinGW x64)"       -ForegroundColor Cyan
Write-Host "  Module-by-module build"                    -ForegroundColor Cyan
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

$cores = [Environment]::ProcessorCount
Write-Host "Using $cores parallel jobs" -ForegroundColor Cyan

# Build order: dependencies must come first
$modules = @(
    "qtbase",
    "qtshadertools",
    "qtsvg",
    "qtdeclarative",
    "qttools",
    "qttranslations"
)

# Check all module sources exist
foreach ($mod in $modules) {
    if (-not (Test-Path "$QtSrcDir\$mod")) {
        Write-Host "ERROR: Module source not found: $QtSrcDir\$mod" -ForegroundColor Red
        exit 1
    }
}

# ------------------------------------------------------------------
# Step 1: Build qtbase (has its own configure)
# ------------------------------------------------------------------

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "  [1/$($modules.Count)] Building qtbase"  -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host "  Start: $(Get-Date -Format 'HH:mm:ss')"

$qtbaseBuild = "$BuildDir\qtbase"
if (Test-Path $qtbaseBuild) { Remove-Item -Recurse -Force $qtbaseBuild }
New-Item -ItemType Directory -Force -Path $qtbaseBuild | Out-Null

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

    # Don't build examples or tests
    "-nomake", "examples"
    "-nomake", "tests"
    "-nomake", "benchmarks"

    # Disable features we don't use (qtbase only)
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

Write-Host "Configuring qtbase..." -ForegroundColor Green
Push-Location $qtbaseBuild
try {
    & "$QtSrcDir\qtbase\configure.bat" @configArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: qtbase configure failed!" -ForegroundColor Red
        exit 1
    }
} finally {
    Pop-Location
}

Write-Host "Building qtbase..." -ForegroundColor Green
Push-Location $qtbaseBuild
try {
    & cmake --build . --parallel $cores
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: qtbase build failed!" -ForegroundColor Red
        exit 1
    }
} finally {
    Pop-Location
}

Write-Host "Installing qtbase..." -ForegroundColor Green
Push-Location $qtbaseBuild
try {
    & cmake --install .
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: qtbase install failed!" -ForegroundColor Red
        exit 1
    }
} finally {
    Pop-Location
}

Write-Host "  qtbase done at $(Get-Date -Format 'HH:mm:ss')" -ForegroundColor Green

# ------------------------------------------------------------------
# Steps 2-N: Build remaining modules with qt-configure-module
# ------------------------------------------------------------------

$qtConfigModule = "$Prefix\bin\qt-configure-module.bat"
if (-not (Test-Path $qtConfigModule)) {
    Write-Host "ERROR: qt-configure-module.bat not found at $qtConfigModule" -ForegroundColor Red
    Write-Host "qtbase install may have failed." -ForegroundColor Red
    exit 1
}

$remaining = $modules | Select-Object -Skip 1
$step = 2

foreach ($mod in $remaining) {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "  [$step/$($modules.Count)] Building $mod" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "  Start: $(Get-Date -Format 'HH:mm:ss')"

    $modBuild = "$BuildDir\$mod"
    if (Test-Path $modBuild) { Remove-Item -Recurse -Force $modBuild }
    New-Item -ItemType Directory -Force -Path $modBuild | Out-Null

    Write-Host "Configuring $mod..." -ForegroundColor Green
    Push-Location $modBuild
    try {
        & $qtConfigModule "$QtSrcDir\$mod"
        if ($LASTEXITCODE -ne 0) {
            Write-Host "ERROR: $mod configure failed!" -ForegroundColor Red
            exit 1
        }
    } finally {
        Pop-Location
    }

    Write-Host "Building $mod..." -ForegroundColor Green
    Push-Location $modBuild
    try {
        & cmake --build . --parallel $cores
        if ($LASTEXITCODE -ne 0) {
            Write-Host "ERROR: $mod build failed!" -ForegroundColor Red
            exit 1
        }
    } finally {
        Pop-Location
    }

    Write-Host "Installing $mod..." -ForegroundColor Green
    Push-Location $modBuild
    try {
        & cmake --install .
        if ($LASTEXITCODE -ne 0) {
            Write-Host "ERROR: $mod install failed!" -ForegroundColor Red
            exit 1
        }
    } finally {
        Pop-Location
    }

    Write-Host "  $mod done at $(Get-Date -Format 'HH:mm:ss')" -ForegroundColor Green
    $step++
}

# ------------------------------------------------------------------
# Cleanup build directory (saves ~10-20 GB)
# ------------------------------------------------------------------

if (-not $NoCleanup) {
    Write-Host ""
    Write-Host "Cleaning up build directory ($BuildDir)..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
    Write-Host "Build directory removed."
}

# ------------------------------------------------------------------
# Verify
# ------------------------------------------------------------------

$elapsed = (Get-Date) - $startTime
Write-Host ""
Write-Host "============================================" -ForegroundColor Green
Write-Host "  Qt 6.10.1 Static Build Complete!"          -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Green
Write-Host "  Installed to: $Prefix"
Write-Host "  Total time:   $([math]::Round($elapsed.TotalMinutes, 1)) minutes"
Write-Host ""

# Quick sanity check
$checks = @(
    @{ Path = "$Prefix\lib\cmake\Qt6\Qt6Config.cmake"; Name = "Qt6Config.cmake" },
    @{ Path = "$Prefix\lib\libQt6Core.a";              Name = "libQt6Core.a" },
    @{ Path = "$Prefix\lib\libQt6Quick.a";             Name = "libQt6Quick.a" },
    @{ Path = "$Prefix\lib\libQt6Qml.a";               Name = "libQt6Qml.a" },
    @{ Path = "$Prefix\bin\qt-configure-module.bat";    Name = "qt-configure-module" }
)

$allOk = $true
foreach ($check in $checks) {
    if (Test-Path $check.Path) {
        if ($check.Path -match "\.a$") {
            $size = (Get-Item $check.Path).Length / 1MB
            Write-Host "  OK  $($check.Name) ($([math]::Round($size, 1)) MB)" -ForegroundColor Green
        } else {
            Write-Host "  OK  $($check.Name)" -ForegroundColor Green
        }
    } else {
        Write-Host "  MISSING  $($check.Name)" -ForegroundColor Red
        $allOk = $false
    }
}

Write-Host ""
if ($allOk) {
    Write-Host "All checks passed! Next step: just release-static" -ForegroundColor Cyan
} else {
    Write-Host "Some checks failed. Review the output above." -ForegroundColor Yellow
}
Write-Host ""
