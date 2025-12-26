# Create 9:16 Poster Art for Microsoft Store
# Uses the Mac app icon as source

param(
    [string]$SourceIcon = "C:\temp\src\ioscpm\iOSCPM\Assets.xcassets\AppIcon.appiconset\appicon.png",
    [string]$OutputDir = "..\msix\Assets"
)

Add-Type -AssemblyName System.Drawing

$ScriptDir = $PSScriptRoot
$FullOutputDir = Join-Path $ScriptDir $OutputDir

Write-Host "Creating 9:16 Poster Art..." -ForegroundColor Cyan

$sourceImage = [System.Drawing.Image]::FromFile($SourceIcon)

$sizes = @(
    @{Width=720; Height=1080; Name="Poster_720x1080.png"},
    @{Width=1440; Height=2160; Name="Poster_1440x2160.png"}
)

foreach ($size in $sizes) {
    $width = $size.Width
    $height = $size.Height
    $outputPath = Join-Path $FullOutputDir $size.Name

    $poster = New-Object System.Drawing.Bitmap($width, $height)
    $graphics = [System.Drawing.Graphics]::FromImage($poster)
    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit

    # Dark background matching the icon
    $graphics.Clear([System.Drawing.Color]::FromArgb(30, 30, 30))

    # Draw icon in upper portion (centered, ~60% of width)
    $iconSize = [int]($width * 0.6)
    $iconX = ($width - $iconSize) / 2
    $iconY = [int]($height * 0.15)

    $graphics.DrawImage($sourceImage, $iconX, $iconY, $iconSize, $iconSize)

    # Draw app name below icon
    $greenColor = [System.Drawing.Color]::FromArgb(0, 200, 100)
    $textBrush = New-Object System.Drawing.SolidBrush($greenColor)

    # App name - "Z80CPM"
    $titleSize = [int]($width * 0.12)
    $titleFont = New-Object System.Drawing.Font("Consolas", $titleSize, [System.Drawing.FontStyle]::Bold)
    $titleText = "Z80CPM"
    $titleMeasure = $graphics.MeasureString($titleText, $titleFont)
    $titleX = ($width - $titleMeasure.Width) / 2
    $titleY = $iconY + $iconSize + ($height * 0.05)
    $graphics.DrawString($titleText, $titleFont, $textBrush, $titleX, $titleY)

    # Tagline
    $tagSize = [int]($width * 0.035)
    $tagFont = New-Object System.Drawing.Font("Consolas", $tagSize)
    $tagText = "CP/M Emulator for Windows"
    $tagMeasure = $graphics.MeasureString($tagText, $tagFont)
    $tagX = ($width - $tagMeasure.Width) / 2
    $tagY = $titleY + $titleMeasure.Height + ($height * 0.02)
    $graphics.DrawString($tagText, $tagFont, $textBrush, $tagX, $tagY)

    # Save
    $poster.Save($outputPath, [System.Drawing.Imaging.ImageFormat]::Png)

    # Cleanup
    $titleFont.Dispose()
    $tagFont.Dispose()
    $textBrush.Dispose()
    $graphics.Dispose()
    $poster.Dispose()

    Write-Host "Created: $($size.Name) ($width x $height)" -ForegroundColor Green
}

$sourceImage.Dispose()
Write-Host "`nPoster art complete!" -ForegroundColor Cyan
