# NSIS Installer Build Script for z80cpmw
# Requires NSIS (Nullsoft Scriptable Install System)

param(
    [string]$Configuration = "Release",
    [switch]$SkipBuild
)

# Stop makes every cmdlet failure terminating.  It also makes Write-Error
# terminating, which would skip the "exit 1" that follows each one and leave the
# exit code to the host, so every Write-Error here carries -ErrorAction Continue:
# the message still goes to the error stream, and the exit 1 below it actually
# runs.  Any new failure site should be written the same way.
$ErrorActionPreference = "Stop"
$ScriptDir = $PSScriptRoot
$RootDir = Resolve-Path (Join-Path $ScriptDir "..\..")
$NsisDir = Join-Path $ScriptDir "..\nsis"
$BinDir = Join-Path $RootDir "bin\$Configuration"
$OutputDir = Join-Path $RootDir "dist"

# --- The version, from the one place that holds it ---------------------------
# z80cpmw/Version.h is the single source. Parse it here rather than storing a
# copy in this repo's packaging files; the anchored pattern matches only the
# four plain-integer defines and cannot be fooled by VERSION_RC further down.
# Deliberately inline in both build scripts rather than dot-sourced: two short
# call sites, and a shared file would add an untested import path.
$versionHeader = Join-Path $RootDir "z80cpmw\Version.h"
if (!(Test-Path $versionHeader)) { Write-Error "Version header not found: $versionHeader" -ErrorAction Continue; exit 1 }
$verText = Get-Content $versionHeader -Raw
$verNums = foreach ($field in 'VERSION_MAJOR','VERSION_MINOR','VERSION_PATCH','VERSION_BUILD') {
    if ($verText -notmatch "(?m)^\s*#define\s+$field\s+(\d+)\s*$") {
        Write-Error "Could not parse $field from $versionHeader" -ErrorAction Continue; exit 1
    }
    [int]$Matches[1]
}
$pkgVersion = $verNums -join '.'         # all four fields, for the manifest
$verShort   = $verNums[0..2] -join '.'   # major.minor.patch, for file names
if ($verNums[0] -lt 1) {
    Write-Error "VERSION_MAJOR must be at least 1; the Store rejects a zero first field." -ErrorAction Continue; exit 1
}
Write-Host "Version $pkgVersion (from z80cpmw\Version.h)" -ForegroundColor Green

# Guard against packaging a stale binary: -SkipBuild over an old bin\Release
# would otherwise label the package with a version the exe does not carry.
function Assert-ExeVersion([string]$exePath, [string]$expected) {
    if (!(Test-Path $exePath)) { Write-Error "Executable not found: $exePath" -ErrorAction Continue; exit 1 }
    $actual = (Get-Item $exePath).VersionInfo.FileVersionRaw.ToString()
    if ($actual -ne $expected) {
        Write-Error "Version mismatch: Version.h says $expected but $exePath is $actual. Rebuild (drop -SkipBuild)." -ErrorAction Continue
        exit 1
    }
    Write-Host "Binary matches Version.h ($actual)" -ForegroundColor Green
}

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
        Write-Error "MSBuild not found. Install Visual Studio 18 with the C++ desktop workload (the project uses PlatformToolset v145)." -ErrorAction Continue
        exit 1
    }

    & $msbuildPath $slnPath /p:Configuration=$Configuration /p:Platform=x64 /t:Rebuild /m

    if ($LASTEXITCODE -ne 0) {
        Write-Error "Build failed." -ErrorAction Continue
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
    Write-Host "Icon file not found." -ForegroundColor Yellow

    # Nothing is created here. z80cpmw.nsi defines MUI_ICON and MUI_UNICON to this
    # path unconditionally, so a missing .ico aborts makensis rather than falling
    # back to the default NSIS icons.
    Write-Host "WARNING: No icon file found at $iconPath" -ForegroundColor Yellow
    Write-Host "z80cpmw.nsi hardcodes MUI_ICON/MUI_UNICON to this path, so makensis will FAIL at Step 4 until it exists." -ForegroundColor Yellow
    Write-Host "Run packaging\scripts\create-ico.ps1 to regenerate it; it writes to this exact path by default." -ForegroundColor Yellow
}

# Step 4: Build the installer
Write-Host "Step 4: Building installer..." -ForegroundColor Yellow

Assert-ExeVersion (Join-Path $BinDir "z80cpmw.exe") $pkgVersion

Push-Location $NsisDir
try {
    # The /D switches have to come BEFORE the script name: makensis processes
    # arguments in order and silently ignores a define that arrives after it,
    # still exiting 0. z80cpmw.nsi has no fallback defaults, so a dropped define
    # is a hard !error rather than an installer stamped with a stale version.
    & $nsisExe `
        "/DVERSIONMAJOR=$($verNums[0])" `
        "/DVERSIONMINOR=$($verNums[1])" `
        "/DVERSIONPATCH=$($verNums[2])" `
        "/DVERSIONBUILD=$($verNums[3])" `
        "z80cpmw.nsi"

    if ($LASTEXITCODE -ne 0) {
        Write-Error "NSIS build failed." -ErrorAction Continue
        exit 1
    }
} finally {
    Pop-Location
}

# Step 5: Move installer to dist folder
Write-Host "Step 5: Finalizing..." -ForegroundColor Yellow

# Look for the exact name the version implies. A newest-wins glob would happily
# pick up a leftover installer from an aborted run at a different version and
# announce it as this build's output.
$expectedName = "z80cpmw-$verShort-setup.exe"
$builtInstaller = Join-Path $NsisDir $expectedName
if (!(Test-Path $builtInstaller)) {
    Write-Error "Expected $expectedName in $NsisDir but it is not there - NSIS did not produce the version it was given." -ErrorAction Continue
    exit 1
}
$destPath = Join-Path $OutputDir $expectedName
Move-Item $builtInstaller $destPath -Force
Write-Host ""
Write-Host "Installer build complete!" -ForegroundColor Cyan
Write-Host "Output: $destPath" -ForegroundColor Green
