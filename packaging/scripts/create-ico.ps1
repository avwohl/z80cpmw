# Create a Windows .ico file for z80cpmw
# This creates a placeholder icon - replace with a professional design for production

param(
    [string]$OutputPath = "..\..\z80cpmw\z80cpmw.ico"
)

Add-Type -AssemblyName System.Drawing

$ScriptDir = $PSScriptRoot
$FullOutputPath = Join-Path $ScriptDir $OutputPath

Write-Host "Creating z80cpmw.ico..." -ForegroundColor Cyan

# Icon sizes required for Windows .ico (16, 32, 48, 256)
$sizes = @(16, 32, 48, 256)

# Create bitmaps for each size
$bitmaps = @()

foreach ($size in $sizes) {
    $bitmap = New-Object System.Drawing.Bitmap($size, $size)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit

    # Dark background
    $bgBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(30, 30, 30))
    $graphics.FillRectangle($bgBrush, 0, 0, $size, $size)

    # Draw "Z80" text
    $textBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(0, 200, 100))
    $fontSize = [Math]::Max(6, $size / 4)
    $font = New-Object System.Drawing.Font("Consolas", $fontSize, [System.Drawing.FontStyle]::Bold)

    $text = "Z80"
    $textSize = $graphics.MeasureString($text, $font)
    $x = ($size - $textSize.Width) / 2
    $y = ($size - $textSize.Height) / 2

    $graphics.DrawString($text, $font, $textBrush, $x, $y)

    # Draw border
    $borderPen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(0, 200, 100), [Math]::Max(1, $size / 32))
    $graphics.DrawRectangle($borderPen, 1, 1, $size - 3, $size - 3)

    $bitmaps += $bitmap

    $font.Dispose()
    $textBrush.Dispose()
    $bgBrush.Dispose()
    $borderPen.Dispose()
    $graphics.Dispose()
}

# Write .ico file manually (since .NET doesn't support multi-size .ico creation directly)
$iconStream = New-Object System.IO.MemoryStream

# ICO header (6 bytes)
$iconStream.WriteByte(0)  # Reserved
$iconStream.WriteByte(0)
$iconStream.WriteByte(1)  # Type: 1 = ICO
$iconStream.WriteByte(0)
$iconStream.WriteByte($sizes.Count)  # Number of images
$iconStream.WriteByte(0)

# Calculate offsets
$headerSize = 6 + (16 * $sizes.Count)  # 6 byte header + 16 bytes per image entry
$offset = $headerSize

# Prepare image data
$imageData = @()
foreach ($bmp in $bitmaps) {
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $imageData += ,$ms.ToArray()
    $ms.Dispose()
}

# Write image directory entries (16 bytes each)
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $size = $sizes[$i]
    $data = $imageData[$i]

    # Width (0 = 256)
    $iconStream.WriteByte([byte]$(if ($size -eq 256) { 0 } else { $size }))
    # Height (0 = 256)
    $iconStream.WriteByte([byte]$(if ($size -eq 256) { 0 } else { $size }))
    # Color palette (0 for true color)
    $iconStream.WriteByte(0)
    # Reserved
    $iconStream.WriteByte(0)
    # Color planes
    $iconStream.WriteByte(1)
    $iconStream.WriteByte(0)
    # Bits per pixel
    $iconStream.WriteByte(32)
    $iconStream.WriteByte(0)

    # Size of image data (4 bytes little-endian)
    $dataSize = $data.Length
    $iconStream.WriteByte([byte]($dataSize -band 0xFF))
    $iconStream.WriteByte([byte](($dataSize -shr 8) -band 0xFF))
    $iconStream.WriteByte([byte](($dataSize -shr 16) -band 0xFF))
    $iconStream.WriteByte([byte](($dataSize -shr 24) -band 0xFF))

    # Offset to image data (4 bytes little-endian)
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

# Save to file
$iconBytes = $iconStream.ToArray()
[System.IO.File]::WriteAllBytes($FullOutputPath, $iconBytes)

# Cleanup
foreach ($bmp in $bitmaps) {
    $bmp.Dispose()
}
$iconStream.Dispose()

Write-Host "Icon created: $FullOutputPath" -ForegroundColor Green
Write-Host ""
Write-Host "Icon contains sizes: $($sizes -join ', ') pixels" -ForegroundColor Gray
Write-Host ""
Write-Host "NOTE: This is a placeholder icon. For Store submission," -ForegroundColor Yellow
Write-Host "      replace with a professionally designed icon." -ForegroundColor Yellow
