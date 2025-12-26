# MSIX Package Build Script for z80cpmw
# Requires Windows SDK with makeappx.exe and signtool.exe

param(
    [string]$Configuration = "Release",
    [string]$CertificatePath = "",
    [string]$CertificatePassword = "",
    [switch]$SkipBuild,
    [switch]$SkipSign
)

$ErrorActionPreference = "Stop"
$ScriptDir = $PSScriptRoot
$RootDir = Resolve-Path (Join-Path $ScriptDir "..\..")
$BinDir = Join-Path $RootDir "bin\$Configuration"
$MsixDir = Join-Path $ScriptDir "..\msix"
$OutputDir = Join-Path $RootDir "dist"

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

# Copy and update manifest
Copy-Item (Join-Path $MsixDir "AppxManifest.xml") $stagingDir

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

$msixPath = Join-Path $OutputDir "z80cpmw.msix"

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

# Step 5: Sign the package (optional)
if (!$SkipSign -and $CertificatePath) {
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
    Write-Host "Step 5: Skipping signing (no certificate provided)." -ForegroundColor Yellow
    Write-Host "  For Store submission, the package will be signed by Microsoft." -ForegroundColor Gray
    Write-Host "  For sideloading, create a self-signed certificate:" -ForegroundColor Gray
    Write-Host '  New-SelfSignedCertificate -Type Custom -Subject "CN=avwohl" -KeyUsage DigitalSignature -FriendlyName "z80cpmw Dev Cert" -CertStoreLocation "Cert:\CurrentUser\My"' -ForegroundColor Gray
}

# Cleanup
Remove-Item -Recurse -Force $stagingDir

Write-Host ""
Write-Host "Package build complete!" -ForegroundColor Cyan
Write-Host "Output: $msixPath" -ForegroundColor Green
Write-Host ""
Write-Host "Next steps for Microsoft Store submission:" -ForegroundColor Yellow
Write-Host "1. Create a developer account at https://partner.microsoft.com" -ForegroundColor Gray
Write-Host "2. Reserve your app name in Partner Center" -ForegroundColor Gray
Write-Host "3. Update AppxManifest.xml with your Store identity" -ForegroundColor Gray
Write-Host "4. Upload the .msix package to Partner Center" -ForegroundColor Gray
