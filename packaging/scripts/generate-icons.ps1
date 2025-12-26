# Icon Generation Script for z80cpmw
# This script generates placeholder icons for development/testing
# For production, replace with professionally designed icons

param(
    [string]$SourceIcon = "..\..\z80cpmw\z80cpmw.ico",
    [string]$OutputDir = "..\msix\Assets"
)

# Required icon sizes for Microsoft Store
$iconSizes = @{
    "Square44x44Logo" = 44
    "Square71x71Logo" = 71
    "Square150x150Logo" = 150
    "Square310x310Logo" = 310
    "Wide310x150Logo" = @{Width=310; Height=150}
    "StoreLogo" = 50
    "SplashScreen" = @{Width=620; Height=300}
}

# Create output directory if it doesn't exist
$fullOutputPath = Join-Path $PSScriptRoot $OutputDir
if (!(Test-Path $fullOutputPath)) {
    New-Item -ItemType Directory -Path $fullOutputPath -Force | Out-Null
}

Write-Host "Icon Generation for z80cpmw Microsoft Store Package" -ForegroundColor Cyan
Write-Host "======================================================" -ForegroundColor Cyan
Write-Host ""

# Check if source icon exists
$sourceIconPath = Join-Path $PSScriptRoot $SourceIcon
if (Test-Path $sourceIconPath) {
    Write-Host "Source icon found: $sourceIconPath" -ForegroundColor Green
} else {
    Write-Host "Source icon not found. Creating placeholder icons..." -ForegroundColor Yellow
}

# Generate placeholder icons using .NET
Add-Type -AssemblyName System.Drawing

foreach ($name in $iconSizes.Keys) {
    $size = $iconSizes[$name]

    if ($size -is [hashtable]) {
        $width = $size.Width
        $height = $size.Height
    } else {
        $width = $size
        $height = $size
    }

    $outputPath = Join-Path $fullOutputPath "$name.png"

    # Create a placeholder image
    $bitmap = New-Object System.Drawing.Bitmap($width, $height)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)

    # Dark background
    $bgBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(30, 30, 30))
    $graphics.FillRectangle($bgBrush, 0, 0, $width, $height)

    # Draw Z80 text
    $textBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(0, 200, 100))
    $fontSize = [Math]::Max(8, [Math]::Min($width, $height) / 4)
    $font = New-Object System.Drawing.Font("Consolas", $fontSize, [System.Drawing.FontStyle]::Bold)

    $text = "Z80"
    $textSize = $graphics.MeasureString($text, $font)
    $x = ($width - $textSize.Width) / 2
    $y = ($height - $textSize.Height) / 2

    $graphics.DrawString($text, $font, $textBrush, $x, $y)

    # Draw border
    $borderPen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(0, 200, 100), 2)
    $graphics.DrawRectangle($borderPen, 1, 1, $width - 3, $height - 3)

    # Save
    $bitmap.Save($outputPath, [System.Drawing.Imaging.ImageFormat]::Png)

    # Cleanup
    $font.Dispose()
    $textBrush.Dispose()
    $bgBrush.Dispose()
    $borderPen.Dispose()
    $graphics.Dispose()
    $bitmap.Dispose()

    Write-Host "Created: $name.png (${width}x${height})" -ForegroundColor Green
}

# Also generate scaled versions for high DPI
$scaledSizes = @{
    "Square44x44Logo" = @(44, 55, 66, 88, 176)  # scale-100, 125, 150, 200, 400
    "Square150x150Logo" = @(150, 188, 225, 300, 600)
}

foreach ($baseName in $scaledSizes.Keys) {
    $scales = $scaledSizes[$baseName]
    for ($i = 0; $i -lt $scales.Count; $i++) {
        $size = $scales[$i]
        $scaleFactor = @(100, 125, 150, 200, 400)[$i]
        $outputPath = Join-Path $fullOutputPath "$baseName.scale-$scaleFactor.png"

        $bitmap = New-Object System.Drawing.Bitmap($size, $size)
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)

        $bgBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(30, 30, 30))
        $graphics.FillRectangle($bgBrush, 0, 0, $size, $size)

        $textBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(0, 200, 100))
        $fontSize = [Math]::Max(8, $size / 4)
        $font = New-Object System.Drawing.Font("Consolas", $fontSize, [System.Drawing.FontStyle]::Bold)

        $text = "Z80"
        $textSize = $graphics.MeasureString($text, $font)
        $x = ($size - $textSize.Width) / 2
        $y = ($size - $textSize.Height) / 2

        $graphics.DrawString($text, $font, $textBrush, $x, $y)

        $borderPen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(0, 200, 100), 2)
        $graphics.DrawRectangle($borderPen, 1, 1, $size - 3, $size - 3)

        $bitmap.Save($outputPath, [System.Drawing.Imaging.ImageFormat]::Png)

        $font.Dispose()
        $textBrush.Dispose()
        $bgBrush.Dispose()
        $borderPen.Dispose()
        $graphics.Dispose()
        $bitmap.Dispose()

        Write-Host "Created: $baseName.scale-$scaleFactor.png (${size}x${size})" -ForegroundColor Green
    }
}

Write-Host ""
Write-Host "Icon generation complete!" -ForegroundColor Cyan
Write-Host "NOTE: These are placeholder icons. Replace with professionally designed icons for Store submission." -ForegroundColor Yellow
