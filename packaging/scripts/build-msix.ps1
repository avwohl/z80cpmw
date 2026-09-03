# MSIX Package Build Script for z80cpmw
# Requires Windows SDK with makeappx.exe and signtool.exe
#
# Two release vehicles:
#   * Store release (default): keeps the Store identity Publisher GUID and is left
#     UNSIGNED here -- Microsoft re-signs it at submission.
#   * Beta / sideload build (-Beta): Authenticode-signed with our Azure Trusted
#     Signing cert (subject "CN=Aaron Wohl, ..."). Because signtool requires the
#     package Publisher to equal the cert subject, -Beta rewrites the manifest
#     Publisher accordingly and emits dist\z80cpmw-<ver>-beta.msix.
#   * Beta rehearsal (-Beta -SkipSign): every step of the beta vehicle except the
#     Trusted Signing call, writing dist\z80cpmw-<ver>-beta-unsigned.msix. This is
#     how you check the beta path without spending a signing call or touching a
#     name that has already shipped. See docs/CODE_SIGNING.md.

# [CmdletBinding()] is here for what it REFUSES, not for the common parameters it
# adds. A param() block without it is a simple script, and PowerShell quietly
# collects unmatched arguments into $args instead of failing: "-Beta -SkipBuild
# -SkipSign -WhatIf" was run against the old script expecting -WhatIf to be
# rejected, and instead -WhatIf fell into $args, the run proceeded for real, and it
# re-minted and re-signed the already-published dist\z80cpmw-1.0.22-beta.msix. This
# script's whole job is irreversible side effects - a network signing call and
# writes into dist\ - so a mistyped or imagined safety switch must be a binding
# error. Note that this deliberately does NOT declare SupportsShouldProcess: with
# plain [CmdletBinding()], -WhatIf is an unknown parameter and now fails outright,
# which is the honest answer, whereas a token -WhatIf that skipped only some steps
# would be a worse lie than none.
[CmdletBinding()]
param(
    [string]$Configuration = "Release",
    [string]$CertificatePath = "",
    [string]$CertificatePassword = "",
    [switch]$SkipBuild,
    # Suppresses every signing route, -Beta's included. It used to be honoured only
    # on the Store arm, which is the one arm that never signs anyway.
    [switch]$SkipSign,
    # Build a signed beta/sideload package instead of an (unsigned) Store package.
    # Combine with -SkipSign for the unsigned rehearsal described in the header.
    [switch]$Beta,
    # Folder holding the Azure Trusted Signing kit (sign.ps1 + dlib + credentials).
    [string]$SigningKit = $(if ($env:Z80CPMW_SIGNING_KIT) { $env:Z80CPMW_SIGNING_KIT } else { "C:\temp\in\z80cpmw-signing-kit" }),
    # Must exactly match the signing cert subject (see docs/CODE_SIGNING.md).
    [string]$PublisherSubject = "CN=Aaron Wohl, O=Aaron Wohl, L=Gainesville, S=fl, C=US"
)

# Stop makes every cmdlet failure terminating.  It also makes Write-Error
# terminating, which would skip the "exit 1" that follows each one and leave the
# exit code to the host, so every Write-Error here carries -ErrorAction Continue:
# the message still goes to the error stream, and the exit 1 below it actually
# runs.  Any new failure site should be written the same way.
$ErrorActionPreference = "Stop"
$ScriptDir = $PSScriptRoot
$RootDir = Resolve-Path (Join-Path $ScriptDir "..\..")
$BinDir = Join-Path $RootDir "bin\$Configuration"
$MsixDir = Join-Path $ScriptDir "..\msix"
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

# One stem for the whole beta run, so the .msix and the .pdb beside it cannot be
# named by two independent expressions and drift apart. The "-unsigned" marker is
# in the file name rather than only in the console output because the file outlives
# the console: a beta package is identified by its name when it is attached to a
# release, and an unsigned rehearsal that is named like a shippable one can be
# uploaded by mistake or can overwrite the artifact already published under that
# name. Only read when -Beta; the Store package name is fixed.
$betaStem = if ($SkipSign) { "z80cpmw-$verShort-beta-unsigned" } else { "z80cpmw-$verShort-beta" }

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

Write-Host "z80cpmw MSIX Package Builder" -ForegroundColor Cyan
Write-Host "=============================" -ForegroundColor Cyan
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

# Step 2: Generate icons if they don't exist
Write-Host "Step 2: Checking icons..." -ForegroundColor Yellow
$assetsDir = Join-Path $MsixDir "Assets"
if (!(Test-Path (Join-Path $assetsDir "StoreLogo.png"))) {
    Write-Host "Generating placeholder icons..." -ForegroundColor Yellow
    & (Join-Path $ScriptDir "generate-icons.ps1")
}

# Step 3: Prepare staging directory
Write-Host "Step 3: Preparing package contents..." -ForegroundColor Yellow
$stagingDir = Join-Path $OutputDir "msix-staging"
if (Test-Path $stagingDir) {
    Remove-Item -Recurse -Force $stagingDir
}
New-Item -ItemType Directory -Path $stagingDir -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $stagingDir "Assets") -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $stagingDir "roms") -Force | Out-Null

# No disks\ directory, and that is the design rather than an omission: every
# port gets its disk images from the ioscpm release area, through the catalog
# pinned in DiskCatalog.cpp's RELEASE_TAG. Nothing is bundled, so nothing can
# go stale in a package or disagree with what the catalog serves.

# Copy application files
Copy-Item (Join-Path $BinDir "z80cpmw.exe") $stagingDir
Copy-Item (Join-Path $BinDir "*.dll") $stagingDir
Copy-Item (Join-Path $BinDir "roms\*") (Join-Path $stagingDir "roms")

# Copy assets
Copy-Item (Join-Path $assetsDir "*") (Join-Path $stagingDir "Assets")

# Stage the manifest with the version - and for -Beta the Publisher - injected.
# Only the staged copy is written; the committed AppxManifest.xml keeps its
# 0.0.0.0 placeholder and is never modified. Targeted regex rather than
# [xml].Save(), which prepends a BOM and reflows the whole document.
Assert-ExeVersion (Join-Path $BinDir "z80cpmw.exe") $pkgVersion

$stagedManifest = Join-Path $stagingDir "AppxManifest.xml"
$manifestText = Get-Content (Join-Path $MsixDir "AppxManifest.xml") -Raw

$verPattern = '(<Identity\b[^>]*?\sVersion=")[^"]*(")'
if (([regex]$verPattern).Matches($manifestText).Count -ne 1) {
    Write-Error "Expected exactly one Identity/@Version in AppxManifest.xml" -ErrorAction Continue; exit 1
}
Write-Host "Injecting version $pkgVersion into the staged manifest" -ForegroundColor Yellow
$manifestText = $manifestText -replace $verPattern, "`${1}$pkgVersion`$2"

if ($Beta) {
    # Beta/sideload builds are signed with our Trusted Signing cert, so the package
    # Publisher MUST equal the cert subject. (Store releases keep the Store identity
    # GUID and are re-signed by Microsoft.)
    Write-Host "Beta build: setting manifest Publisher to '$PublisherSubject'" -ForegroundColor Yellow
    $pubPattern = '(<Identity\b[^>]*?\sPublisher=")[^"]*(")'
    if (([regex]$pubPattern).Matches($manifestText).Count -ne 1) {
        Write-Error "Expected exactly one Identity/@Publisher in AppxManifest.xml" -ErrorAction Continue; exit 1
    }
    $pubEsc = $PublisherSubject -replace '\$','$$$$'
    $manifestText = $manifestText -replace $pubPattern, "`${1}$pubEsc`$2"
}

[System.IO.File]::WriteAllText($stagedManifest, $manifestText, (New-Object System.Text.UTF8Encoding($false)))

# Read it back: a silently failed injection would otherwise ship 0.0.0.0.
[xml]$check = Get-Content $stagedManifest -Raw
if ($check.Package.Identity.Version -ne $pkgVersion) {
    Write-Error "Manifest injection failed: staged version is '$($check.Package.Identity.Version)', expected '$pkgVersion'" -ErrorAction Continue
    exit 1
}

# Step 4: Create MSIX package
Write-Host "Step 4: Creating MSIX package..." -ForegroundColor Yellow

# Find makeappx.exe
$sdkPath = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin\*\x64\makeappx.exe" |
           Sort-Object { [version]($_.Directory.Parent.Name) } -Descending |
           Select-Object -First 1

if (!$sdkPath) {
    Write-Error "Windows SDK not found. Please install Windows 10 SDK." -ErrorAction Continue
    exit 1
}

$makeAppxPath = $sdkPath.FullName
$signToolPath = Join-Path $sdkPath.Directory "signtool.exe"

$msixName = if ($Beta) { "$betaStem.msix" } else { "z80cpmw.msix" }
$msixPath = Join-Path $OutputDir $msixName

# Remove existing package. This is the destructive step that -Beta -SkipSign has to
# be kept away from a shipped name: under the rehearsal $betaStem carries
# "-unsigned", so the only file this can delete is a previous rehearsal's output.
if (Test-Path $msixPath) {
    Remove-Item $msixPath
}

# Create package
& $makeAppxPath pack /d $stagingDir /p $msixPath /o

if ($LASTEXITCODE -ne 0) {
    Write-Error "Package creation failed." -ErrorAction Continue
    exit 1
}

Write-Host "MSIX package created: $msixPath" -ForegroundColor Green

# Step 5: Sign the package.
# The test used to be "if ($Beta)" ahead of any look at $SkipSign, so -Beta ignored
# -SkipSign completely: the one switch whose entire purpose is to keep a run off the
# network was overridden by the one branch that goes to the network. That left the
# beta vehicle with no rehearsal at all - the only way to exercise the .pdb rule in
# step 6 was to spend a real Trusted Signing call and overwrite whatever dist\ held
# under the shipped name. -SkipSign is now checked first on every arm.
if ($Beta -and !$SkipSign) {
    Write-Host "Step 5: Signing beta package with Azure Trusted Signing..." -ForegroundColor Yellow

    $signPs1 = Join-Path $SigningKit "sign.ps1"
    if (!(Test-Path $signPs1)) {
        Remove-Item -Recurse -Force $stagingDir
        Write-Error "Signing kit not found at '$SigningKit'. Set `$env:Z80CPMW_SIGNING_KIT or pass -SigningKit <dir> (the kit holds sign.ps1 + the Trusted Signing dlib + credentials). See docs/CODE_SIGNING.md." -ErrorAction Continue
        exit 1
    }

    & $signPs1 $msixPath
    if ($LASTEXITCODE -ne 0) {
        Remove-Item -Recurse -Force $stagingDir
        Write-Error "Beta package signing failed." -ErrorAction Continue
        exit 1
    }

    Write-Host "Verifying signature..." -ForegroundColor Yellow
    & $signPs1 -Verify $msixPath
    Write-Host "Beta package signed and verified." -ForegroundColor Green
}
elseif ($Beta) {
    # The rehearsal arm. It reaches neither sign.ps1 nor the network, which is the
    # property the whole restructure exists to provide, so nothing here may consult
    # $SigningKit even to report on it - a run that touches the kit is a run someone
    # will eventually let sign. Step 6 below still runs, because the symbol copy is
    # the thing this rehearsal is for.
    Write-Host "Step 5: Skipping signing (-SkipSign)." -ForegroundColor Yellow
    Write-Host "  This is the unsigned beta rehearsal. The package cannot be sideloaded" -ForegroundColor Gray
    Write-Host "  and must not be published; re-run without -SkipSign to sign for real." -ForegroundColor Gray
}
elseif (!$SkipSign -and $CertificatePath) {
    Write-Host "Step 5: Signing package..." -ForegroundColor Yellow

    $signArgs = @("sign", "/fd", "SHA256", "/f", $CertificatePath)
    if ($CertificatePassword) {
        $signArgs += "/p"
        $signArgs += $CertificatePassword
    }
    $signArgs += "/t"
    $signArgs += "http://timestamp.digicert.com"
    $signArgs += $msixPath

    & $signToolPath $signArgs

    if ($LASTEXITCODE -ne 0) {
        Write-Warning "Package signing failed. The package can still be used for testing."
    } else {
        Write-Host "Package signed successfully." -ForegroundColor Green
    }
} else {
    Write-Host "Step 5: Skipping signing (Store package)." -ForegroundColor Yellow
    Write-Host "  For Store submission, the package will be signed by Microsoft." -ForegroundColor Gray
    Write-Host "  For a signed beta you can sideload, re-run with -Beta (Azure Trusted Signing)." -ForegroundColor Gray
}

# Cleanup
Remove-Item -Recurse -Force $stagingDir

# Step 6: Keep the beta build's symbols.
# The Store package is re-signed and served by Microsoft, but the sideload beta
# is the binary testers actually run, and a crash dump from it is unreadable
# without the .pdb built alongside that exact exe. It cannot be recovered later:
# a rebuild - against a different vcpkg wxWidgets, say - is a different binary
# with a different debug GUID, so its symbols will not load against the one
# that shipped. 1.0.22 shipped with no .pdb on either channel for
# exactly that reason. Named from $betaStem so the pair stays obvious.
#
# This runs on BOTH beta arms, signed and rehearsal alike, which is the point of
# having a rehearsal: the copy and its Write-Error are what a dry run is checking,
# and an arm that skipped them would prove nothing about the run that ships.
if ($Beta) {
    $pdbSource = Join-Path $BinDir "z80cpmw.pdb"
    $pdbPath = Join-Path $OutputDir "$betaStem.pdb"
    if (Test-Path $pdbSource) {
        Copy-Item $pdbSource $pdbPath -Force
        Write-Host "Symbols kept: $pdbPath" -ForegroundColor Green
    } else {
        Write-Error "No symbols at $pdbSource. The package at $msixPath is good, but its .pdb cannot be produced by rebuilding later. Rebuild this configuration and re-run." -ErrorAction Continue
        exit 1
    }
}

Write-Host ""
Write-Host "Package build complete!" -ForegroundColor Cyan
Write-Host "Output: $msixPath" -ForegroundColor Green
Write-Host ""
if ($Beta -and $SkipSign) {
    # Do not print sideload instructions for a package that cannot be sideloaded:
    # an unsigned MSIX is refused by Add-AppxPackage on a normal machine, and the
    # closing lines of a build log are what someone copies.
    Write-Host "Unsigned beta rehearsal - not installable and not publishable." -ForegroundColor Yellow
    Write-Host "It confirms the beta path end to end, symbol copy included. Delete both" -ForegroundColor Gray
    Write-Host "-unsigned files when you are done; they are build output." -ForegroundColor Gray
    Write-Host "For a real beta, re-run without -SkipSign - and only on a version that has" -ForegroundColor Gray
    Write-Host "not already shipped, since that run writes dist\z80cpmw-$verShort-beta.msix." -ForegroundColor Gray
} elseif ($Beta) {
    Write-Host "Beta sideload package. Testers install via:" -ForegroundColor Yellow
    Write-Host "  double-click the .msix (App Installer), or  Add-AppxPackage `"$msixPath`"" -ForegroundColor Gray
    Write-Host "The Trusted Signing cert chains to a Microsoft public root, so no dev cert import is needed." -ForegroundColor Gray
} else {
    Write-Host "Next step for Microsoft Store submission:" -ForegroundColor Yellow
    Write-Host "Upload dist\z80cpmw.msix to Partner Center - the Z80CPM product is already reserved." -ForegroundColor Gray
    Write-Host "The Identity in AppxManifest.xml is the one Partner Center assigned and must not be" -ForegroundColor Gray
    Write-Host "edited; this build injected version $pkgVersion into the staged copy from z80cpmw\Version.h." -ForegroundColor Gray
}
