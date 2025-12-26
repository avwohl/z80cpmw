# Convert Mac app icon to Windows Store assets
# Requires the source icon from ioscpm project

param(
    [string]$SourceIcon = "C:\temp\src\ioscpm\iOSCPM\Assets.xcassets\AppIcon.appiconset\appicon.png",
    [string]$OutputDir = "..\msix\Assets",
    [string]$IcoOutputDir = "..\..\z80cpmw"
)

Add-Type -AssemblyName System.Drawing

$ScriptDir = $PSScriptRoot
$FullOutputDir = Join-Path $ScriptDir $OutputDir
$FullIcoDir = Join-Path $ScriptDir $IcoOutputDir

Write-Host "Converting Mac icon to Windows Store assets..." -ForegroundColor Cyan

# Load source image
$sourceImage = [System.Drawing.Image]::FromFile($SourceIcon)
Write-Host "Source image: $($sourceImage.Width)x$($sourceImage.Height)" -ForegroundColor Green

# Store icon sizes (square)
$squareSizes = @{
    "Square44x44Logo" = 44
    "Square71x71Logo" = 71
    "Square150x150Logo" = 150
    "Square310x310Logo" = 310
    "StoreLogo" = 50
}

# Scaled versions
$scaledSizes = @{
    "Square44x44Logo" = @(44, 55, 66, 88, 176)
    "Square150x150Logo" = @(150, 188, 225, 300, 600)
}

# Wide/splash sizes
$rectSizes = @{
    "Wide310x150Logo" = @{Width=310; Height=150}
    "SplashScreen" = @{Width=620; Height=300}
}

function Resize-Image {
    param($source, $width, $height)

    $destImage = New-Object System.Drawing.Bitmap($width, $height)
    $graphics = [System.Drawing.Graphics]::FromImage($destImage)
    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality

    # Fill with dark background first
    $graphics.Clear([System.Drawing.Color]::FromArgb(30, 30, 30))

    # Calculate scaling to fit
    $scale = [Math]::Min($width / $source.Width, $height / $source.Height)
    $newWidth = [int]($source.Width * $scale)
    $newHeight = [int]($source.Height * $scale)
    $x = ($width - $newWidth) / 2
    $y = ($height - $newHeight) / 2

    $graphics.DrawImage($source, $x, $y, $newWidth, $newHeight)
    $graphics.Dispose()

    return $destImage
}

# Generate square icons
foreach ($name in $squareSizes.Keys) {
    $size = $squareSizes[$name]
    $outputPath = Join-Path $FullOutputDir "$name.png"

    $resized = Resize-Image $sourceImage $size $size
    $resized.Save($outputPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $resized.Dispose()

    Write-Host "Created: $name.png (${size}x${size})" -ForegroundColor Green
}

# Generate scaled versions
foreach ($baseName in $scaledSizes.Keys) {
    $scales = $scaledSizes[$baseName]
    $scaleFactors = @(100, 125, 150, 200, 400)

    for ($i = 0; $i -lt $scales.Count; $i++) {
        $size = $scales[$i]
        $scaleFactor = $scaleFactors[$i]
        $outputPath = Join-Path $FullOutputDir "$baseName.scale-$scaleFactor.png"

        $resized = Resize-Image $sourceImage $size $size
        $resized.Save($outputPath, [System.Drawing.Imaging.ImageFormat]::Png)
        $resized.Dispose()

        Write-Host "Created: $baseName.scale-$scaleFactor.png (${size}x${size})" -ForegroundColor Green
    }
}

# Generate rectangular icons (wide tile, splash)
foreach ($name in $rectSizes.Keys) {
    $dims = $rectSizes[$name]
    $outputPath = Join-Path $FullOutputDir "$name.png"

    $resized = Resize-Image $sourceImage $dims.Width $dims.Height
    $resized.Save($outputPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $resized.Dispose()

    Write-Host "Created: $name.png ($($dims.Width)x$($dims.Height))" -ForegroundColor Green
}

# Generate Windows .ico file
Write-Host "`nCreating Windows .ico file..." -ForegroundColor Cyan

$icoSizes = @(16, 32, 48, 256)
$bitmaps = @()

foreach ($size in $icoSizes) {
    $bitmaps += (Resize-Image $sourceImage $size $size)
}

# Write .ico file
$icoPath = Join-Path $FullIcoDir "z80cpmw.ico"
$iconStream = New-Object System.IO.MemoryStream

# ICO header
$iconStream.WriteByte(0); $iconStream.WriteByte(0)  # Reserved
$iconStream.WriteByte(1); $iconStream.WriteByte(0)  # Type: ICO
$iconStream.WriteByte($icoSizes.Count); $iconStream.WriteByte(0)  # Count

# Prepare PNG data for each size
$imageData = @()
foreach ($bmp in $bitmaps) {
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $imageData += ,$ms.ToArray()
    $ms.Dispose()
}

# Calculate header size
$headerSize = 6 + (16 * $icoSizes.Count)
$offset = $headerSize

# Write directory entries
for ($i = 0; $i -lt $icoSizes.Count; $i++) {
    $size = $icoSizes[$i]
    $data = $imageData[$i]

    $iconStream.WriteByte([byte]$(if ($size -eq 256) { 0 } else { $size }))  # Width
    $iconStream.WriteByte([byte]$(if ($size -eq 256) { 0 } else { $size }))  # Height
    $iconStream.WriteByte(0)  # Color palette
    $iconStream.WriteByte(0)  # Reserved
    $iconStream.WriteByte(1); $iconStream.WriteByte(0)  # Color planes
    $iconStream.WriteByte(32); $iconStream.WriteByte(0)  # Bits per pixel

    # Data size (4 bytes LE)
    $iconStream.WriteByte([byte]($data.Length -band 0xFF))
    $iconStream.WriteByte([byte](($data.Length -shr 8) -band 0xFF))
    $iconStream.WriteByte([byte](($data.Length -shr 16) -band 0xFF))
    $iconStream.WriteByte([byte](($data.Length -shr 24) -band 0xFF))

    # Offset (4 bytes LE)
    $iconStream.WriteByte([byte]($offset -band 0xFF))
    $iconStream.WriteByte([byte](($offset -shr 8) -band 0xFF))
    $iconStream.WriteByte([byte](($offset -shr 16) -band 0xFF))
    $iconStream.WriteByte([byte](($offset -shr 24) -band 0xFF))

    $offset += $data.Length
}

# Write image data
foreach ($data in $imageData) {
    $iconStream.Write($data, 0, $data.Length)
}

[System.IO.File]::WriteAllBytes($icoPath, $iconStream.ToArray())
$iconStream.Dispose()

Write-Host "Created: z80cpmw.ico" -ForegroundColor Green

# Cleanup
foreach ($bmp in $bitmaps) { $bmp.Dispose() }
$sourceImage.Dispose()

Write-Host "`nIcon conversion complete!" -ForegroundColor Cyan
