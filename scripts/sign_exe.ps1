#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Sign MakineAI executables and installer with Authenticode.

.DESCRIPTION
    Signs EXE/DLL/MSI files using the MakineAI code signing certificate.
    Supports both development (self-signed) and production (SignPath/OV) certs.

.PARAMETER Path
    File(s) to sign. Accepts wildcards.

.PARAMETER Thumbprint
    Certificate thumbprint. Auto-detected from scripts/certs/thumbprint.txt if omitted.

.EXAMPLE
    .\sign_exe.ps1 -Path "dist\MakineAI.exe"
    .\sign_exe.ps1 -Path "installer\output\*.exe"
    .\sign_exe.ps1 -Path "dist\*.exe","dist\*.dll"
#>

param(
    [Parameter(Position=0)]
    [string[]]$Path,

    [string]$Thumbprint,
    [string]$TimestampServer = "http://timestamp.digicert.com",
    [switch]$SkipTimestamp
)

$ErrorActionPreference = "Stop"

# ── Find signtool.exe ────────────────────────────────────────────────────────
function Find-SignTool {
    # Check PATH first
    $inPath = Get-Command signtool.exe -ErrorAction SilentlyContinue
    if ($inPath) { return $inPath.Source }

    # Search Windows SDK locations
    $sdkPaths = @(
        "${env:ProgramFiles(x86)}\Windows Kits\10\bin",
        "$env:ProgramFiles\Windows Kits\10\bin"
    )
    foreach ($sdk in $sdkPaths) {
        if (Test-Path $sdk) {
            $found = Get-ChildItem -Path $sdk -Recurse -Filter "signtool.exe" -ErrorAction SilentlyContinue |
                Where-Object { $_.FullName -match "x64" } |
                Sort-Object FullName -Descending |
                Select-Object -First 1
            if ($found) { return $found.FullName }
        }
    }

    return $null
}

$signTool = Find-SignTool
if (-not $signTool) {
    Write-Host "[ERROR] signtool.exe not found." -ForegroundColor Red
    Write-Host "Install Windows SDK: winget install Microsoft.WindowsSDK.10.0.26100" -ForegroundColor Yellow
    Write-Host "Or: Visual Studio Installer > Individual Components > Windows SDK" -ForegroundColor Yellow
    exit 1
}
Write-Host "[OK] signtool: $signTool" -ForegroundColor DarkGray

# ── Resolve certificate thumbprint ───────────────────────────────────────────
if (-not $Thumbprint) {
    $thumbFile = Join-Path $PSScriptRoot "certs\thumbprint.txt"
    if (Test-Path $thumbFile) {
        $Thumbprint = (Get-Content $thumbFile -Raw).Trim()
        Write-Host "[OK] Using thumbprint from certs/thumbprint.txt" -ForegroundColor DarkGray
    } else {
        # Try to find MakineAI cert via certutil
        $certOutput = certutil -user -store My 2>&1 | Out-String
        if ($certOutput -match "MakineAI" -and $certOutput -match "Cert Hash\(sha1\):\s*([0-9a-fA-F\s]+)") {
            $Thumbprint = ($Matches[1]).Trim() -replace '\s',''
            Write-Host "[OK] Auto-detected MakineAI cert: $Thumbprint" -ForegroundColor DarkGray
        } else {
            Write-Host "[ERROR] No certificate found. Run: just setup-cert" -ForegroundColor Red
            exit 1
        }
    }
}

# Verify cert exists in store via certutil
$verifyOutput = certutil -user -store My $Thumbprint 2>&1 | Out-String
if ($verifyOutput -notmatch "Cert Hash\(sha1\)") {
    Write-Host "[ERROR] Certificate not found in store: $Thumbprint" -ForegroundColor Red
    Write-Host "Run: just setup-cert" -ForegroundColor Yellow
    exit 1
}
# Extract subject and expiry from certutil output
$subjectMatch = [regex]::Match($verifyOutput, "Subject:\s*(.+)")
$notAfterMatch = [regex]::Match($verifyOutput, "NotAfter:\s*(.+)")
$certSubject = if ($subjectMatch.Success) { $subjectMatch.Groups[1].Value.Trim() } else { "Unknown" }
$certExpiry = if ($notAfterMatch.Success) { $notAfterMatch.Groups[1].Value.Trim() } else { "Unknown" }
Write-Host "[OK] Certificate: $certSubject (expires $certExpiry)" -ForegroundColor Green

# ── Resolve files to sign ────────────────────────────────────────────────────
if (-not $Path) {
    # Default: sign all EXEs in known build output locations
    $Path = @(
        "dist\MakineAI.exe",
        "dist-static\MakineAI.exe",
        "build\dev\MakineAI.exe",
        "build\release\MakineAI.exe",
        "qml\build\release-static\MakineAI.exe"
    )
}

$filesToSign = @()
foreach ($p in $Path) {
    $resolved = Resolve-Path $p -ErrorAction SilentlyContinue
    if ($resolved) {
        $filesToSign += $resolved.Path
    }
}

if ($filesToSign.Count -eq 0) {
    Write-Host "[WARN] No files found to sign." -ForegroundColor Yellow
    Write-Host "Build first: just release" -ForegroundColor DarkGray
    exit 0
}

# ── Sign each file ───────────────────────────────────────────────────────────
$signed = 0
$failed = 0

foreach ($file in $filesToSign) {
    Write-Host "Signing: $file" -ForegroundColor Cyan

    $args = @(
        "sign",
        "/sha1", $Thumbprint,
        "/fd", "SHA256",
        "/td", "SHA256",
        "/d", "MakineAI - Turkish Game Translation Platform",
        "/du", "https://github.com/jlceaser/MakineAI"
    )

    if (-not $SkipTimestamp) {
        $args += "/tr", $TimestampServer
    }

    $args += $file

    $result = & $signTool @args 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  [OK] Signed successfully" -ForegroundColor Green
        $signed++
    } else {
        Write-Host "  [FAIL] $result" -ForegroundColor Red
        $failed++
    }
}

Write-Host ""
Write-Host "=== Signing Complete ===" -ForegroundColor Cyan
Write-Host "  Signed: $signed, Failed: $failed"

if ($failed -gt 0) { exit 1 }

# ── Verify signature ─────────────────────────────────────────────────────────
Write-Host ""
Write-Host "Verifying signatures..." -ForegroundColor Yellow
foreach ($file in $filesToSign) {
    $verify = & $signTool verify /pa /v $file 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  [OK] $file" -ForegroundColor Green
    } else {
        Write-Host "  [WARN] $file - verification failed (expected for self-signed until Root CA is trusted)" -ForegroundColor Yellow
    }
}
