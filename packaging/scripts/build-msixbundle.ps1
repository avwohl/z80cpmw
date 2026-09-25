# MSIX bundle script for z80cpmw: one .msixbundle holding the x64 and ARM64
# packages, so a single download installs the native build on either kind of PC.
#
# It builds nothing and packs no binary itself. Its inputs are the per-arch
# packages build-msix.ps1 already wrote into dist\, and it refuses to run unless
# every one of them is there and they agree with each other and with Version.h.
#
#   * Store bundle (default): bundles dist\z80cpmw-<ver>-store.msix and
#     dist\z80cpmw-<ver>-arm64-store.msix into dist\z80cpmw-<ver>-store.msixbundle,
#     unsigned - Microsoft re-signs it, as it does the single packages.
#   * Beta bundle (-Beta): bundles the two UNSIGNED beta rehearsals,
#     dist\z80cpmw-<ver>[-arm64]-beta-unsigned.msix, and signs only the bundle.
#     The rehearsals already carry the beta Publisher, so this is one Trusted
#     Signing call per release instead of three, and it never re-mints the
#     per-arch z80cpmw-<ver>-beta.msix names.
#   * Beta rehearsal (-Beta -SkipSign): the same, writing
#     dist\z80cpmw-<ver>-beta-unsigned.msixbundle and reaching neither sign.ps1
#     nor the network.
#
# Typical beta run, on the machine with the signing kit:
#   build-msix.ps1 -Platform x64   -Beta -SkipSign
#   build-msix.ps1 -Platform ARM64 -Beta -SkipSign
#   build-msixbundle.ps1 -Beta
#
# [CmdletBinding()] for the reason build-msix.ps1 gives: an imagined -WhatIf must
# be a binding error, not a silent real run.
[CmdletBinding()]
param(
    [switch]$Beta,
    [switch]$SkipSign,
    # Which architectures go in. Both by default; a subset is for rehearsing on a
    # machine that can build only one of them (the ARM64 VM cannot link x64).
    [ValidateSet("x64", "ARM64")]
    [string[]]$Platforms = @("x64", "ARM64"),
    [string]$SigningKit = $(if ($env:Z80CPMW_SIGNING_KIT) { $env:Z80CPMW_SIGNING_KIT } else { "C:\temp\in\z80cpmw-signing-kit" })
)

# See build-msix.ps1 for why every Write-Error carries -ErrorAction Continue.
$ErrorActionPreference = "Stop"
$ScriptDir = $PSScriptRoot
$RootDir = Resolve-Path (Join-Path $ScriptDir "..\..")
$OutputDir = Join-Path $RootDir "dist"

# --- The version, parsed exactly as build-msix.ps1 parses it -----------------
$versionHeader = Join-Path $RootDir "z80cpmw\Version.h"
if (!(Test-Path $versionHeader)) { Write-Error "Version header not found: $versionHeader" -ErrorAction Continue; exit 1 }
$verText = Get-Content $versionHeader -Raw
$verNums = foreach ($field in 'VERSION_MAJOR','VERSION_MINOR','VERSION_PATCH','VERSION_BUILD') {
    if ($verText -notmatch "(?m)^\s*#define\s+$field\s+(\d+)\s*$") {
        Write-Error "Could not parse $field from $versionHeader" -ErrorAction Continue; exit 1
    }
    [int]$Matches[1]
}
$pkgVersion = $verNums -join '.'
$verShort   = $verNums[0..2] -join '.'
Write-Host "Version $pkgVersion (from z80cpmw\Version.h)" -ForegroundColor Green

$Platforms = @($Platforms | Select-Object -Unique)

# Input stems, from the same naming build-msix.ps1 uses: x64 has no arch marker.
function Get-InputStem([string]$platform) {
    $archTag = if ($platform -eq "ARM64") { "-arm64" } else { "" }
    if ($Beta) { "z80cpmw-$verShort$archTag-beta-unsigned" } else { "z80cpmw-$verShort$archTag-store" }
}
# A bundle missing an architecture must not carry the release name: the first
# rehearsal of this script wrote an ARM64-only z80cpmw-<ver>-store.msixbundle,
# indistinguishable by name from the one to upload. So a subset is marked in the
# name, and is never signed.
$subsetTag = if ($Platforms.Count -lt 2) { "-$($Platforms[0].ToLower())-only" } else { "" }
if ($subsetTag -and $Beta -and !$SkipSign) {
    Write-Error "Refusing to sign a bundle holding only $($Platforms -join ', '). Add -SkipSign to rehearse, or bundle both architectures." -ErrorAction Continue
    exit 1
}
$bundleStem = if (!$Beta) { "z80cpmw-$verShort-store$subsetTag" }
              elseif ($SkipSign) { "z80cpmw-$verShort-beta-unsigned$subsetTag" }
              else { "z80cpmw-$verShort-beta" }
$bundlePath = Join-Path $OutputDir "$bundleStem.msixbundle"

Write-Host "z80cpmw MSIX Bundle Builder" -ForegroundColor Cyan
Write-Host "===========================" -ForegroundColor Cyan
Write-Host ""

# A signed beta bundle is a published artifact the moment it is uploaded, and
# signing it again re-mints it under the same name. Refuse rather than overwrite;
# deleting it by hand is the deliberate act that says this version has not shipped.
if ($Beta -and !$SkipSign -and (Test-Path $bundlePath)) {
    Write-Error "$bundlePath already exists. If this version has not shipped, delete it and its .pdb files by hand and re-run; if it has, bump Version.h." -ErrorAction Continue
    exit 1
}

# --- Step 1: check every input package ---------------------------------------
Write-Host "Step 1: Checking input packages..." -ForegroundColor Yellow
Add-Type -AssemblyName System.IO.Compression.FileSystem

function Read-PackageIdentity([string]$msixPath) {
    $zip = [System.IO.Compression.ZipFile]::OpenRead($msixPath)
    try {
        $entry = $zip.GetEntry("AppxManifest.xml")
        if (!$entry) { return $null }
        $reader = New-Object System.IO.StreamReader($entry.Open())
        try { [xml]$doc = $reader.ReadToEnd() } finally { $reader.Dispose() }
        return $doc.Package.Identity
    } finally {
        $zip.Dispose()
    }
}

$inputs = @()
foreach ($platform in $Platforms) {
    $stem = Get-InputStem $platform
    $msix = Join-Path $OutputDir "$stem.msix"
    $pdb  = Join-Path $OutputDir "$stem.pdb"
    if (!(Test-Path $msix)) {
        $how = if ($Beta) { "build-msix.ps1 -Platform $platform -Beta -SkipSign" } else { "build-msix.ps1 -Platform $platform" }
        Write-Error "Missing $msix. Build it first with: $how" -ErrorAction Continue; exit 1
    }
    # The bundle is only as debuggable as the .pdb files kept beside its inputs.
    if (!(Test-Path $pdb)) {
        Write-Error "Missing symbols $pdb for $msix. A package without its .pdb cannot be fixed by rebuilding; rebuild both and re-run." -ErrorAction Continue; exit 1
    }
    $id = Read-PackageIdentity $msix
    if (!$id) { Write-Error "$msix has no AppxManifest.xml" -ErrorAction Continue; exit 1 }
    $expectedArch = if ($platform -eq "ARM64") { "arm64" } else { "x64" }
    if ($id.ProcessorArchitecture -ne $expectedArch) {
        Write-Error "$msix says ProcessorArchitecture '$($id.ProcessorArchitecture)', expected '$expectedArch'." -ErrorAction Continue; exit 1
    }
    if ($id.Version -ne $pkgVersion) {
        Write-Error "$msix is version $($id.Version) but Version.h says $pkgVersion. Rebuild it." -ErrorAction Continue; exit 1
    }
    Write-Host ("  {0,-6} {1}  {2} {3}" -f $platform, (Split-Path $msix -Leaf), $id.Name, $id.Version) -ForegroundColor Gray
    $inputs += [pscustomobject]@{ Platform = $platform; Msix = $msix; Pdb = $pdb; Name = $id.Name; Publisher = $id.Publisher }
}

# makeappx rejects a bundle whose packages disagree on Name or Publisher, but
# only after packing; saying which file is wrong is more useful than its error.
foreach ($field in "Name", "Publisher") {
    $values = @($inputs | ForEach-Object { $_.$field } | Select-Object -Unique)
    if ($values.Count -ne 1) {
        Write-Error "Input packages disagree on $field`: $($values -join ' / '). Were they built by the same arm (Store vs -Beta)?" -ErrorAction Continue; exit 1
    }
}

# --- Step 2: stage and bundle -------------------------------------------------
Write-Host "Step 2: Creating the bundle..." -ForegroundColor Yellow
$sdkPath = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin\*\x64\makeappx.exe" |
           Sort-Object { [version]($_.Directory.Parent.Name) } -Descending |
           Select-Object -First 1
if (!$sdkPath) { Write-Error "Windows SDK not found (makeappx.exe)." -ErrorAction Continue; exit 1 }

# makeappx bundle takes a directory and bundles every package in it, so the
# staging directory holds exactly the checked inputs and nothing else.
$stagingDir = Join-Path $OutputDir "msixbundle-staging"
if (Test-Path $stagingDir) { Remove-Item -Recurse -Force $stagingDir }
New-Item -ItemType Directory -Path $stagingDir -Force | Out-Null
foreach ($i in $inputs) { Copy-Item $i.Msix $stagingDir }

if (Test-Path $bundlePath) { Remove-Item $bundlePath }
& $sdkPath.FullName bundle /d $stagingDir /p $bundlePath /bv $pkgVersion /o
$packExit = $LASTEXITCODE
Remove-Item -Recurse -Force $stagingDir
if ($packExit -ne 0) { Write-Error "Bundle creation failed." -ErrorAction Continue; exit 1 }
Write-Host "Bundle created: $bundlePath" -ForegroundColor Green

# --- Step 3: sign (beta only) ------------------------------------------------
if ($Beta -and !$SkipSign) {
    Write-Host "Step 3: Signing the bundle with Azure Trusted Signing..." -ForegroundColor Yellow
    $signPs1 = Join-Path $SigningKit "sign.ps1"
    if (!(Test-Path $signPs1)) {
        Remove-Item $bundlePath
        Write-Error "Signing kit not found at '$SigningKit'. Set `$env:Z80CPMW_SIGNING_KIT or pass -SigningKit <dir>. See docs/CODE_SIGNING.md." -ErrorAction Continue
        exit 1
    }
    & $signPs1 $bundlePath
    if ($LASTEXITCODE -ne 0) {
        Remove-Item $bundlePath
        Write-Error "Bundle signing failed." -ErrorAction Continue
        exit 1
    }
    Write-Host "Verifying signature..." -ForegroundColor Yellow
    & $signPs1 -Verify $bundlePath
    Write-Host "Beta bundle signed and verified." -ForegroundColor Green
} elseif ($Beta) {
    Write-Host "Step 3: Skipping signing (-SkipSign). Not installable, not publishable." -ForegroundColor Yellow
} else {
    Write-Host "Step 3: Skipping signing (Store bundle; Microsoft signs it)." -ForegroundColor Yellow
}

# --- Step 4: symbols under the bundle's own name ------------------------------
# The inputs' .pdb files carry "-unsigned" on the beta arm, and those files are
# rehearsal output meant to be deleted. Copy them under the name of the artifact
# that ships, one per architecture, so deleting the rehearsals loses nothing.
foreach ($i in $inputs) {
    $arch = $i.Platform.ToLower()
    $dest = Join-Path $OutputDir "$bundleStem-$arch.pdb"
    Copy-Item $i.Pdb $dest -Force
    Write-Host "Symbols kept: $dest" -ForegroundColor Green
}

Write-Host ""
Write-Host "Bundle build complete: $bundlePath" -ForegroundColor Cyan
Write-Host "Architectures: $($Platforms -join ', ')" -ForegroundColor Gray
if ($Platforms.Count -lt 2) {
    Write-Host "Only $($Platforms -join ', ') is inside. This is a rehearsal, not a release bundle." -ForegroundColor Yellow
}
