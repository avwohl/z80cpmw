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

param(
    [string]$Configuration = "Release",
    [string]$CertificatePath = "",
    [string]$CertificatePassword = "",
    [switch]$SkipBuild,
    [switch]$SkipSign,
    # Build a signed beta/sideload package instead of an (unsigned) Store package.
    [switch]$Beta,
    # Folder holding the Azure Trusted Signing kit (sign.ps1 + dlib + credentials).
    [string]$SigningKit = $(if ($env:Z80CPMW_SIGNING_KIT) { $env:Z80CPMW_SIGNING_KIT } else { "C:\temp\in\z80cpmw-signing-kit" }),
    # Must exactly match the signing cert subject (see docs/CODE_SIGNING.md).
    [string]$PublisherSubject = "CN=Aaron Wohl, O=Aaron Wohl, L=Gainesville, S=fl, C=US"
)

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
if (!(Test-Path $versionHeader)) { Write-Error "Version header not found: $versionHeader"; exit 1 }
$verText = Get-Content $versionHeader -Raw
$verNums = foreach ($field in 'VERSION_MAJOR','VERSION_MINOR','VERSION_PATCH','VERSION_BUILD') {
    if ($verText -notmatch "(?m)^\s*#define\s+$field\s+(\d+)\s*$") {
        Write-Error "Could not parse $field from $versionHeader"; exit 1
    }
    [int]$Matches[1]
}
$pkgVersion = $verNums -join '.'         # all four fields, for the manifest
$verShort   = $verNums[0..2] -join '.'   # major.minor.patch, for file names
if ($verNums[0] -lt 1) {
    Write-Error "VERSION_MAJOR must be at least 1; the Store rejects a zero first field."; exit 1
}
Write-Host "Version $pkgVersion (from z80cpmw\Version.h)" -ForegroundColor Green

# Guard against packaging a stale binary: -SkipBuild over an old bin\Release
# would otherwise label the package with a version the exe does not carry.
function Assert-ExeVersion([string]$exePath, [string]$expected) {
    if (!(Test-Path $exePath)) { Write-Error "Executable not found: $exePath"; exit 1 }
    $actual = (Get-Item $exePath).VersionInfo.FileVersionRaw.ToString()
    if ($actual -ne $expected) {
        Write-Error "Version mismatch: Version.h says $expected but $exePath is $actual. Rebuild (drop -SkipBuild)."
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
New-Item -ItemType Directory -Path (Join-Path $stagingDir "disks") -Force | Out-Null

# Copy application files
Copy-Item (Join-Path $BinDir "z80cpmw.exe") $stagingDir
Copy-Item (Join-Path $BinDir "*.dll") $stagingDir
Copy-Item (Join-Path $BinDir "roms\*") (Join-Path $stagingDir "roms")
Copy-Item (Join-Path $BinDir "disks\*") (Join-Path $stagingDir "disks")

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
    Write-Error "Expected exactly one Identity/@Version in AppxManifest.xml"; exit 1
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
        Write-Error "Expected exactly one Identity/@Publisher in AppxManifest.xml"; exit 1
    }
    $pubEsc = $PublisherSubject -replace '\$','$$$$'
    $manifestText = $manifestText -replace $pubPattern, "`${1}$pubEsc`$2"
}

[System.IO.File]::WriteAllText($stagedManifest, $manifestText, (New-Object System.Text.UTF8Encoding($false)))

# Read it back: a silently failed injection would otherwise ship 0.0.0.0.
[xml]$check = Get-Content $stagedManifest -Raw
if ($check.Package.Identity.Version -ne $pkgVersion) {
    Write-Error "Manifest injection failed: staged version is '$($check.Package.Identity.Version)', expected '$pkgVersion'"
    exit 1
}

# Step 4: Create MSIX package
Write-Host "Step 4: Creating MSIX package..." -ForegroundColor Yellow

# Find makeappx.exe
$sdkPath = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin\*\x64\makeappx.exe" |
           Sort-Object { [version]($_.Directory.Parent.Name) } -Descending |
           Select-Object -First 1

if (!$sdkPath) {
    Write-Error "Windows SDK not found. Please install Windows 10 SDK."
    exit 1
}

$makeAppxPath = $sdkPath.FullName
$signToolPath = Join-Path $sdkPath.Directory "signtool.exe"

$msixName = if ($Beta) { "z80cpmw-$verShort-beta.msix" } else { "z80cpmw.msix" }
$msixPath = Join-Path $OutputDir $msixName

# Remove existing package
if (Test-Path $msixPath) {
    Remove-Item $msixPath
}

# Create package
& $makeAppxPath pack /d $stagingDir /p $msixPath /o

if ($LASTEXITCODE -ne 0) {
    Write-Error "Package creation failed."
    exit 1
}

Write-Host "MSIX package created: $msixPath" -ForegroundColor Green

# Step 5: Sign the package
if ($Beta) {
    Write-Host "Step 5: Signing beta package with Azure Trusted Signing..." -ForegroundColor Yellow

    $signPs1 = Join-Path $SigningKit "sign.ps1"
    if (!(Test-Path $signPs1)) {
        Remove-Item -Recurse -Force $stagingDir
        Write-Error "Signing kit not found at '$SigningKit'. Set `$env:Z80CPMW_SIGNING_KIT or pass -SigningKit <dir> (the kit holds sign.ps1 + the Trusted Signing dlib + credentials). See docs/CODE_SIGNING.md."
        exit 1
    }

    & $signPs1 $msixPath
    if ($LASTEXITCODE -ne 0) {
        Remove-Item -Recurse -Force $stagingDir
        Write-Error "Beta package signing failed."
        exit 1
    }

    Write-Host "Verifying signature..." -ForegroundColor Yellow
    & $signPs1 -Verify $msixPath
    Write-Host "Beta package signed and verified." -ForegroundColor Green
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

Write-Host ""
Write-Host "Package build complete!" -ForegroundColor Cyan
Write-Host "Output: $msixPath" -ForegroundColor Green
Write-Host ""
if ($Beta) {
    Write-Host "Beta sideload package. Testers install via:" -ForegroundColor Yellow
    Write-Host "  double-click the .msix (App Installer), or  Add-AppxPackage `"$msixPath`"" -ForegroundColor Gray
    Write-Host "The Trusted Signing cert chains to a Microsoft public root, so no dev cert import is needed." -ForegroundColor Gray
} else {
    Write-Host "Next steps for Microsoft Store submission:" -ForegroundColor Yellow
    Write-Host "1. Create a developer account at https://partner.microsoft.com" -ForegroundColor Gray
    Write-Host "2. Reserve your app name in Partner Center" -ForegroundColor Gray
    Write-Host "3. Update AppxManifest.xml with your Store identity" -ForegroundColor Gray
    Write-Host "4. Upload the .msix package to Partner Center" -ForegroundColor Gray
}
