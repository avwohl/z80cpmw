# Create 1:1 Box Art for Microsoft Store
# Uses the Mac app icon as source

param(
    [string]$SourceIcon = "C:\temp\src\ioscpm\iOSCPM\Assets.xcassets\AppIcon.appiconset\appicon.png",
    [string]$OutputDir = "..\msix\Assets"
)

Add-Type -AssemblyName System.Drawing

$ScriptDir = $PSScriptRoot
$FullOutputDir = Join-Path $ScriptDir $OutputDir

Write-Host "Creating 1:1 Box Art..." -ForegroundColor Cyan

$sourceImage = [System.Drawing.Image]::FromFile($SourceIcon)

$sizes = @(
    @{Size=1080; Name="BoxArt_1080x1080.png"},
    @{Size=2160; Name="BoxArt_2160x2160.png"}
)

foreach ($item in $sizes) {
    $size = $item.Size
    $outputPath = Join-Path $FullOutputDir $item.Name

    $boxart = New-Object System.Drawing.Bitmap($size, $size)
    $graphics = [System.Drawing.Graphics]::FromImage($boxart)
    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit

    # Dark background
    $graphics.Clear([System.Drawing.Color]::FromArgb(30, 30, 30))

    # Draw icon centered, ~65% of size
    $iconSize = [int]($size * 0.65)
    $iconX = ($size - $iconSize) / 2
    $iconY = [int]($size * 0.08)

    $graphics.DrawImage($sourceImage, $iconX, $iconY, $iconSize, $iconSize)

    # Draw app name below icon
    $greenColor = [System.Drawing.Color]::FromArgb(0, 200, 100)
    $textBrush = New-Object System.Drawing.SolidBrush($greenColor)

    # App name - "Z80CPM"
    $titleSize = [int]($size * 0.10)
    $titleFont = New-Object System.Drawing.Font("Consolas", $titleSize, [System.Drawing.FontStyle]::Bold)
    $titleText = "Z80CPM"
    $titleMeasure = $graphics.MeasureString($titleText, $titleFont)
    $titleX = ($size - $titleMeasure.Width) / 2
    $titleY = $iconY + $iconSize + ($size * 0.03)
    $graphics.DrawString($titleText, $titleFont, $textBrush, $titleX, $titleY)

    # Tagline
    $tagSize = [int]($size * 0.032)
    $tagFont = New-Object System.Drawing.Font("Consolas", $tagSize)
    $tagText = "CP/M Emulator for Windows"
    $tagMeasure = $graphics.MeasureString($tagText, $tagFont)
    $tagX = ($size - $tagMeasure.Width) / 2
    $tagY = $titleY + $titleMeasure.Height + ($size * 0.01)
    $graphics.DrawString($tagText, $tagFont, $textBrush, $tagX, $tagY)

    # Save
    $boxart.Save($outputPath, [System.Drawing.Imaging.ImageFormat]::Png)

    # Cleanup
    $titleFont.Dispose()
    $tagFont.Dispose()
    $textBrush.Dispose()
    $graphics.Dispose()
    $boxart.Dispose()

    Write-Host "Created: $($item.Name) ($size x $size)" -ForegroundColor Green
}

$sourceImage.Dispose()
Write-Host "`nBox art complete!" -ForegroundColor Cyan
