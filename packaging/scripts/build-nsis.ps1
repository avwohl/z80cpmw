# NSIS Installer Build Script for z80cpmw
# Requires NSIS (Nullsoft Scriptable Install System)

param(
    [string]$Configuration = "Release",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$ScriptDir = $PSScriptRoot
$RootDir = Resolve-Path (Join-Path $ScriptDir "..\..")
$NsisDir = Join-Path $ScriptDir "..\nsis"
$OutputDir = Join-Path $RootDir "dist"

Write-Host "z80cpmw NSIS Installer Builder" -ForegroundColor Cyan
Write-Host "===============================" -ForegroundColor Cyan
Write-Host ""

# Ensure output directory exists
if (!(Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

# Step 1: Build the application
if (!$SkipBuild) {
    Write-Host "Step 1: Building application..." -ForegroundColor Yellow

    $slnPath = Join-Path $RootDir "z80cpmw.sln"
    $msbuildPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1

    if (!$msbuildPath) {
        Write-Error "MSBuild not found. Please install Visual Studio 2022."
        exit 1
    }

    & $msbuildPath $slnPath /p:Configuration=$Configuration /p:Platform=x64 /t:Rebuild /m

    if ($LASTEXITCODE -ne 0) {
        Write-Error "Build failed."
        exit 1
    }

    Write-Host "Build completed successfully." -ForegroundColor Green
} else {
    Write-Host "Step 1: Skipping build (using existing binaries)..." -ForegroundColor Yellow
}

# Step 2: Find NSIS
Write-Host "Step 2: Locating NSIS..." -ForegroundColor Yellow

$nsisExe = $null

# Common NSIS installation paths
$nsisPaths = @(
    "${env:ProgramFiles(x86)}\NSIS\makensis.exe",
    "${env:ProgramFiles}\NSIS\makensis.exe",
    "C:\Program Files (x86)\NSIS\makensis.exe",
    "C:\Program Files\NSIS\makensis.exe"
)

foreach ($path in $nsisPaths) {
    if (Test-Path $path) {
        $nsisExe = $path
        break
    }
}

# Try to find via PATH
if (!$nsisExe) {
    $nsisExe = Get-Command "makensis.exe" -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source
}

if (!$nsisExe) {
    Write-Host ""
    Write-Host "NSIS not found. Please install NSIS from https://nsis.sourceforge.io/" -ForegroundColor Red
    Write-Host ""
    Write-Host "After installing NSIS, run this script again." -ForegroundColor Yellow
    exit 1
}

Write-Host "NSIS found: $nsisExe" -ForegroundColor Green

# Step 3: Check for icon file
Write-Host "Step 3: Checking icon file..." -ForegroundColor Yellow

$iconPath = Join-Path $RootDir "z80cpmw\z80cpmw.ico"
if (!(Test-Path $iconPath)) {
    Write-Host "Icon file not found. Creating placeholder..." -ForegroundColor Yellow

    # Create a simple placeholder .ico (we'll use an empty placeholder - NSIS will still work)
    # In production, this should be a proper .ico file
    Write-Host "WARNING: No icon file found at $iconPath" -ForegroundColor Yellow
    Write-Host "The installer will use default NSIS icons." -ForegroundColor Yellow
    Write-Host "To add a custom icon, create z80cpmw.ico in the z80cpmw folder." -ForegroundColor Yellow
}

# Step 4: Build the installer
Write-Host "Step 4: Building installer..." -ForegroundColor Yellow

Push-Location $NsisDir
try {
    & $nsisExe "z80cpmw.nsi"

    if ($LASTEXITCODE -ne 0) {
        Write-Error "NSIS build failed."
        exit 1
    }
} finally {
    Pop-Location
}

# Step 5: Move installer to dist folder
Write-Host "Step 5: Finalizing..." -ForegroundColor Yellow

$installerName = Get-ChildItem $NsisDir -Filter "z80cpmw-*-setup.exe" | Select-Object -First 1
if ($installerName) {
    $destPath = Join-Path $OutputDir $installerName.Name
    Move-Item $installerName.FullName $destPath -Force
    Write-Host ""
    Write-Host "Installer build complete!" -ForegroundColor Cyan
    Write-Host "Output: $destPath" -ForegroundColor Green
} else {
    Write-Warning "Installer file not found in output directory."
}
