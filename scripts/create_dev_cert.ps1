#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Create a self-signed code signing certificate for Makine-Launcher development.
    Uses certreq.exe + certutil.exe — fully automated, no GUI dialogs.

.DESCRIPTION
    Creates a code signing certificate via certreq INF template:
    1. Generates certificate request INF file
    2. Creates self-signed cert and installs to CurrentUser\My store
    3. Exports PFX for use with signtool
    4. Saves thumbprint for automated signing

.NOTES
    Run ONCE per machine. After this: `just sign` to sign EXEs.
    For production releases, use SignPath.io (free for open source).
#>

param(
    [string]$CertDir = "$PSScriptRoot\certs",
    [Parameter(Mandatory=$true)][string]$Password,
    [switch]$Force
)

$ErrorActionPreference = "Stop"

# ── Setup output dir ─────────────────────────────────────────────────────────
New-Item -ItemType Directory -Force -Path $CertDir | Out-Null

$csPfx     = Join-Path $CertDir "Makine-CodeSign.pfx"
$thumbFile = Join-Path $CertDir "thumbprint.txt"
$infFile   = Join-Path $CertDir "certreq.inf"
$cerFile   = Join-Path $CertDir "Makine-CodeSign.cer"

if ((Test-Path $csPfx) -and -not $Force) {
    Write-Host "[OK] Certificate already exists: $csPfx" -ForegroundColor Green
    Write-Host "     Use -Force to recreate." -ForegroundColor DarkGray
    exit 0
}

Write-Host ""
Write-Host "=== Makine-Launcher Code Signing Certificate Setup ===" -ForegroundColor Cyan
Write-Host ""

# ── Step 1: Create INF template ──────────────────────────────────────────────
Write-Host "[1/4] Creating certificate request template..." -ForegroundColor Yellow

$infContent = @"
[Version]
Signature="`$Windows NT`$"

[NewRequest]
Subject = "CN=MakineCeviri Team, O=MakineCeviri, L=Istanbul, C=TR"
KeyLength = 4096
KeyAlgorithm = RSA
HashAlgorithm = SHA256
KeySpec = 2
KeyUsage = 0x80
MachineKeySet = FALSE
ProviderName = "Microsoft Enhanced RSA and AES Cryptographic Provider"
RequestType = Cert
ValidityPeriod = Years
ValidityPeriodUnits = 3
Exportable = TRUE

[EnhancedKeyUsageExtension]
OID = 1.3.6.1.5.5.7.3.3
"@

$infContent | Out-File -FilePath $infFile -Encoding ascii
Write-Host "  INF template: $infFile" -ForegroundColor DarkGray

# ── Step 2: Create self-signed certificate ───────────────────────────────────
Write-Host "[2/4] Creating self-signed code signing certificate..." -ForegroundColor Yellow

# Remove old cert file
Remove-Item $cerFile -ErrorAction SilentlyContinue

$result = certreq -new -f $infFile $cerFile 2>&1
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $cerFile)) {
    Write-Host "[ERROR] Certificate creation failed:" -ForegroundColor Red
    Write-Host $result -ForegroundColor Red
    exit 1
}
Write-Host "  Certificate created and installed to store" -ForegroundColor DarkGray

# ── Step 3: Find thumbprint ──────────────────────────────────────────────────
Write-Host "[3/4] Extracting thumbprint..." -ForegroundColor Yellow

$thumbOutput = certutil -dump $cerFile 2>&1
$thumbLine = ($thumbOutput | Select-String "Cert Hash\(sha1\)" | Select-Object -First 1)

if ($thumbLine) {
    $thumbprint = ($thumbLine.ToString() -replace '.*:\s*','').Trim() -replace '\s',''
} else {
    Write-Host "[ERROR] Could not extract thumbprint from certificate" -ForegroundColor Red
    Write-Host "Output:" -ForegroundColor DarkGray
    $thumbOutput | ForEach-Object { Write-Host "  $_" -ForegroundColor DarkGray }
    exit 1
}

if ($thumbprint.Length -lt 40) {
    Write-Host "[ERROR] Invalid thumbprint: $thumbprint" -ForegroundColor Red
    exit 1
}

$thumbprint | Out-File -FilePath $thumbFile -NoNewline -Encoding ascii
Write-Host "  Thumbprint: $thumbprint" -ForegroundColor Green

# ── Step 4: Export PFX ────────────────────────────────────────────────────────
Write-Host "[4/4] Exporting PFX (private key bundle)..." -ForegroundColor Yellow

# Remove old PFX
Remove-Item $csPfx -ErrorAction SilentlyContinue

$exportResult = certutil -user -exportpfx -p $Password My $thumbprint $csPfx 2>&1
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $csPfx)) {
    Write-Host "[ERROR] PFX export failed:" -ForegroundColor Red
    Write-Host $exportResult -ForegroundColor Red
    Write-Host "" -ForegroundColor DarkGray
    Write-Host "Try manually:" -ForegroundColor Yellow
    Write-Host "  certutil -user -exportpfx -p `"$Password`" My $thumbprint `"$csPfx`"" -ForegroundColor DarkGray
    exit 1
}
Write-Host "  PFX: $csPfx" -ForegroundColor DarkGray

# ── Cleanup temp files ────────────────────────────────────────────────────────
Remove-Item $infFile -ErrorAction SilentlyContinue
Remove-Item $cerFile -ErrorAction SilentlyContinue

Write-Host ""
Write-Host "=== Setup Complete ===" -ForegroundColor Green
Write-Host ""
Write-Host "Certificate Details:" -ForegroundColor Cyan
Write-Host "  Subject:     CN=MakineCeviri Team, O=MakineCeviri, L=Istanbul, C=TR"
Write-Host "  Thumbprint:  $thumbprint"
Write-Host "  PFX:         $csPfx"
Write-Host "  Password:    $Password"
Write-Host ""
Write-Host "Usage:" -ForegroundColor Cyan
Write-Host "  just sign              # Sign all built EXEs"
Write-Host "  just release-signed    # Build static + sign"
Write-Host "  just sign-file <path>  # Sign a specific file"
Write-Host ""
Write-Host "Signed EXEs will be trusted on THIS machine." -ForegroundColor DarkGray
Write-Host "For other users: install the cert or use SignPath.io for production." -ForegroundColor DarkGray
