<#
.SYNOPSIS
  Build a Makine Launcher MSIX from the static EXE + generated assets.

.DESCRIPTION
  Reproduces, locally, the Store-packaging step (GitHub Actions removed
  2026-05-19). Substitutes the Partner Center identity into the manifest
  template, stages exe + Assets + manifest, and runs makeappx.

  Identity/Publisher MUST come from Microsoft Partner Center (App identity
  page). Publisher MUST equal the subject (CN=...) of the code-signing
  certificate. An MSIX whose Identity/Publisher do not match the Partner
  Center reservation is rejected on submission.

  Signing is a SEPARATE owner step (needs scripts/certs/ — not done here):
    signtool sign /fd SHA256 /f <cert.pfx> /p <pwd> dist\Makine-Launcher-v<ver>.msix

.EXAMPLE
  pwsh scripts/make_msix.ps1 -Version 0.1.0.0 `
       -IdentityName MakineCeviri.MakineLauncher `
       -Publisher "CN=ABCD1234-..."
#>
[CmdletBinding()]
param(
  [Parameter(Mandatory)] [string]$Version,        # 4-part: X.Y.Z.0
  [Parameter(Mandatory)] [string]$IdentityName,   # Partner Center Identity Name
  [Parameter(Mandatory)] [string]$Publisher,      # Partner Center Publisher == cert CN
  [string]$ExePath = "build/release-static/Makine-Launcher.exe",
  [string]$OutDir  = "dist",
  # Set for a dynamically linked build: runs windeployqt against the staged EXE
  # and copies the runtime DLLs sitting next to it. Static Qt is not available
  # as a prebuilt kit (it has to be compiled from source), and an MSIX is a
  # container anyway — it may carry DLLs, so static linking buys nothing here.
  [string]$QtBinDir = ""
)
$ErrorActionPreference = "Stop"
$repo = Split-Path $PSScriptRoot -Parent
Set-Location $repo

# --- validate inputs ---------------------------------------------------
if ($Version -notmatch '^\d+\.\d+\.\d+\.\d+$') {
  throw "Version must be 4-part (e.g. 0.1.0.0); Store requires the revision .0 — got '$Version'"
}
if ($IdentityName -match '@MSIX|<.*>|^$' -or $Publisher -match '@MSIX|<.*>|^$') {
  throw "IdentityName/Publisher look like placeholders. Use the EXACT values from Microsoft Partner Center > App identity."
}
if ($Publisher -notmatch '^CN=') {
  throw "Publisher must be an X.500 subject starting with 'CN=' (matches the signing cert subject) — got '$Publisher'"
}
if (-not (Test-Path $ExePath) -or (Get-Item $ExePath).Length -eq 0) {
  throw "EXE missing/0 bytes: $ExePath  (build it first, or pass -ExePath)"
}
if ($QtBinDir -and -not (Test-Path (Join-Path $QtBinDir "windeployqt.exe"))) {
  throw "windeployqt.exe not found in -QtBinDir: $QtBinDir"
}

$tpl    = Join-Path $repo "packaging/msix/AppxManifest.xml.in"
$assets = Join-Path $repo "packaging/msix/Assets"
if (-not (Test-Path $tpl))    { throw "Manifest template missing: $tpl" }
if (-not (Test-Path $assets)) { throw "Assets missing — run: python scripts/gen_msix_assets.py" }
$need = @("StoreLogo.png","Square44x44Logo.png","Square71x71Logo.png",
          "Square150x150Logo.png","Square310x310Logo.png","Wide310x150Logo.png","SplashScreen.png")
$miss = $need | Where-Object { -not (Test-Path (Join-Path $assets $_)) }
if ($miss) { throw "Missing assets: $($miss -join ', ') — run: python scripts/gen_msix_assets.py" }

# --- stage -------------------------------------------------------------
$stage = Join-Path $repo "packaging/msix/_stage"
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force $stage | Out-Null
New-Item -ItemType Directory -Force (Join-Path $stage "Assets") | Out-Null

(Get-Content $tpl -Raw).
  Replace('@MSIX_IDENTITY_NAME@', $IdentityName).
  Replace('@MSIX_PUBLISHER@',     $Publisher).
  Replace('@MSIX_VERSION@',       $Version) |
  Set-Content (Join-Path $stage "AppxManifest.xml") -Encoding UTF8

Copy-Item (Join-Path $assets "*.png") (Join-Path $stage "Assets") -Force
Copy-Item $ExePath (Join-Path $stage "Makine-Launcher.exe") -Force

# --- runtime dependencies (dynamic builds only) ------------------------
if ($QtBinDir) {
  $stagedExe = Join-Path $stage "Makine-Launcher.exe"

  # Runtime DLLs already sitting next to the built EXE (vcpkg applocal output).
  # Copied before windeployqt so it can see the full dependency set.
  $srcDir = Split-Path (Resolve-Path $ExePath) -Parent
  $dlls = Get-ChildItem $srcDir -Filter *.dll -EA SilentlyContinue
  if ($dlls) {
    Copy-Item $dlls.FullName -Destination $stage -Force
    Write-Host "Copied $($dlls.Count) runtime DLL(s) from $srcDir"
  }

  & (Join-Path $QtBinDir "windeployqt.exe") `
      --release --no-translations --no-system-d3d-compiler `
      --qmldir (Join-Path $repo "qml/qml") $stagedExe
  if ($LASTEXITCODE -ne 0) { throw "windeployqt failed ($LASTEXITCODE)" }

  $n = (Get-ChildItem $stage -Recurse -File).Count
  Write-Host "Staged $n file(s) after windeployqt"
}

# --- locate makeappx ---------------------------------------------------
$makeappx = (Get-Command makeappx -EA SilentlyContinue).Source
if (-not $makeappx) {
  $makeappx = Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\bin\*\x64\makeappx.exe" -EA SilentlyContinue |
              Sort-Object FullName | Select-Object -Last 1 -Expand FullName
}
if (-not $makeappx) { throw "makeappx.exe not found — install the Windows 10/11 SDK" }

# --- pack --------------------------------------------------------------
New-Item -ItemType Directory -Force (Join-Path $repo $OutDir) | Out-Null
$msix = Join-Path $repo "$OutDir/Makine-Launcher-v$Version.msix"
if (Test-Path $msix) { Remove-Item $msix -Force }
& $makeappx pack /d $stage /p $msix /overwrite
if ($LASTEXITCODE -ne 0) { throw "makeappx failed ($LASTEXITCODE)" }

$mb = [math]::Round((Get-Item $msix).Length / 1MB, 2)
Write-Host ""
Write-Host "MSIX built: $msix ($mb MB)" -ForegroundColor Green
Write-Host "UNSIGNED — owner must sign before Store submission:" -ForegroundColor Yellow
Write-Host "  signtool sign /fd SHA256 /f <cert.pfx> /p <pwd> `"$msix`""
Write-Host "  (the cert subject MUST equal the manifest Publisher: $Publisher)"
