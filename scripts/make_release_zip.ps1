#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Stage a dynamically linked Makine-Launcher build and pack it as a release ZIP.

.DESCRIPTION
    Mirrors the staging that make_msix.ps1 performs for the Store package, but
    produces the plain ZIP published on GitHub Releases: the launcher, the
    elevated file helper, the Qt runtime that windeployqt resolves, and a
    SHA-256 sum file next to the archive.

    Static Qt is not available as a prebuilt kit, so the released ZIP carries
    the Qt DLLs instead of being a single EXE.

.PARAMETER Version
    Release version without the leading "v" (e.g. 0.1.5-beta). Names the archive.

.PARAMETER ExePath
    Built launcher to package. Defaults to the release-mingw preset output.

.PARAMETER QtBinDir
    Qt bin directory holding windeployqt.exe.

.PARAMETER OutDir
    Where the staging folder, the ZIP and SHA256SUMS.txt are written.

.PARAMETER Sign
    Authenticode-sign the staged EXEs before zipping (needs scripts/certs).

.EXAMPLE
    .\make_release_zip.ps1 -Version 0.1.5-beta
    .\make_release_zip.ps1 -Version 0.1.5-beta -Sign
#>

param(
  [Parameter(Mandatory)] [string]$Version,
  [string]$ExePath  = "",
  [string]$QtBinDir = "C:/Qt/6.11.1/mingw_64/bin",
  [string]$OutDir   = "",
  [switch]$Sign
)
$ErrorActionPreference = "Stop"

$repo = (& git rev-parse --show-toplevel).Trim()
if (-not $repo) { throw "Not inside a git repository" }

if (-not $ExePath) { $ExePath = Join-Path $repo "build/release-mingw/Makine-Launcher.exe" }
if (-not $OutDir)  { $OutDir  = Join-Path $repo "dist" }

if (-not (Test-Path $ExePath)) { throw "Launcher EXE not found: $ExePath  (build it first: just release-mingw)" }
if ((Get-Item $ExePath).Length -eq 0) { throw "Launcher EXE is 0 bytes: $ExePath" }

$windeployqt = Join-Path $QtBinDir "windeployqt.exe"
if (-not (Test-Path $windeployqt)) { throw "windeployqt.exe not found in -QtBinDir: $QtBinDir" }

$name  = "Makine-Launcher-v$Version-win64"
$stage = Join-Path $OutDir $name
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force $stage | Out-Null

Copy-Item $ExePath (Join-Path $stage "Makine-Launcher.exe") -Force

# Elevated file helper - installs into directories that require admin rights.
$srcDir  = Split-Path (Resolve-Path $ExePath) -Parent
$elevate = Join-Path $srcDir "makine-elevate.exe"
if (Test-Path $elevate) {
  Copy-Item $elevate (Join-Path $stage "makine-elevate.exe") -Force
} else {
  Write-Warning "makine-elevate.exe missing next to the launcher - admin-owned install paths will fail"
}

# Project DLLs first, so windeployqt sees the full dependency set.
$dlls = Get-ChildItem $srcDir -Filter *.dll -EA SilentlyContinue
if ($dlls) {
  Copy-Item $dlls.FullName -Destination $stage -Force
  Write-Host "Copied $($dlls.Count) runtime DLL(s) from $srcDir"
}

$deployArgs = @(
  "--release", "--no-translations", "--no-system-d3d-compiler",
  "--qmldir", (Join-Path $repo "qml/qml"),
  (Join-Path $stage "Makine-Launcher.exe")
)
& $windeployqt @deployArgs
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed ($LASTEXITCODE)" }

if ($Sign) {
  & (Join-Path $repo "scripts/sign_exe.ps1") -Path @(
      (Join-Path $stage "Makine-Launcher.exe"),
      (Join-Path $stage "makine-elevate.exe")
  )
  if ($LASTEXITCODE -ne 0) { throw "signing failed ($LASTEXITCODE)" }
}

$zip = Join-Path $OutDir "$name.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip -Force

# .NET hashing instead of Get-FileHash: a PowerShell 7 entry leaking into
# PSModulePath makes Windows PowerShell 5.1 fail to load Utility cmdlets.
$sha256 = [System.Security.Cryptography.SHA256]::Create()
$stream = [System.IO.File]::OpenRead($zip)
try   { $hash = [BitConverter]::ToString($sha256.ComputeHash($stream)).Replace("-", "").ToLower() }
finally { $stream.Dispose(); $sha256.Dispose() }
"$hash  $name.zip" | Out-File (Join-Path $OutDir "SHA256SUMS.txt") -Encoding ascii

$files = (Get-ChildItem $stage -Recurse -File).Count
$mb    = [math]::Round((Get-Item $zip).Length / 1MB, 1)
Write-Host ""
Write-Host "Staged $files file(s) -> $zip ($mb MB)"
Write-Host "SHA256: $hash"
Write-Host "Publish: see docs/RELEASING.md"
