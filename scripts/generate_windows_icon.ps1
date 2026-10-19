# Generates packaging/windows/game-face.ico from assets/icon.png.
# The .ico holds PNG-compressed images at the sizes Windows asks for (taskbar,
# Explorer, Start menu, Add/Remove Programs). Run it again when icon.png changes:
#
#   powershell -ExecutionPolicy Bypass -File scripts\generate_windows_icon.ps1

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'assets\icon.png'
$target = Join-Path $root 'packaging\windows\game-face.ico'
$sizes = 16, 20, 24, 32, 40, 48, 64, 128, 256

$original = [System.Drawing.Image]::FromFile($source)
$images = foreach ($size in $sizes) {
    $bitmap = New-Object System.Drawing.Bitmap $size, $size
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $graphics.DrawImage($original, 0, 0, $size, $size)
    $graphics.Dispose()
    $stream = New-Object System.IO.MemoryStream
    $bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
    $bitmap.Dispose()
    , $stream.ToArray()
}
$original.Dispose()

# ICONDIR, then one ICONDIRENTRY per image, then the PNG data.
$out = New-Object System.IO.MemoryStream
$writer = New-Object System.IO.BinaryWriter $out
$writer.Write([UInt16]0)               # reserved
$writer.Write([UInt16]1)               # type: icon
$writer.Write([UInt16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $dimension = if ($sizes[$i] -ge 256) { 0 } else { $sizes[$i] }  # 0 means 256
    $writer.Write([byte]$dimension)    # width
    $writer.Write([byte]$dimension)    # height
    $writer.Write([byte]0)             # palette colors
    $writer.Write([byte]0)             # reserved
    $writer.Write([UInt16]1)           # color planes
    $writer.Write([UInt16]32)          # bits per pixel
    $writer.Write([UInt32]$images[$i].Length)
    $writer.Write([UInt32]$offset)
    $offset += $images[$i].Length
}
foreach ($image in $images) { $writer.Write($image) }
$writer.Flush()
[System.IO.File]::WriteAllBytes($target, $out.ToArray())
Write-Output "Wrote $target ($($out.Length) bytes, sizes $($sizes -join ', '))"
