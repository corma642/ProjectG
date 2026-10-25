[CmdletBinding()]
param(
    [string]$SourcePath,
    [string]$LogoPath,
    [string]$ProjectRoot
)

# Generates every Android branding image from the sources in Resources/Branding:
#   AndroidAppIcon.png -> legacy launcher icons (drawable-*/icon.png, ProjectG.png)
#                      -> adaptive icon layers (drawable-*/icon_foreground.png, icon_background.png; Android 8+ reads
#                         Build/Android/res/drawable-anydpi-v26/icon.xml, a checked-in file that references these layers)
#   Logo.png           -> launch image shown until the engine renders its first frame
#                         (Build/Android/res/drawable/splashscreen_portrait.png, bShowLaunchImage=True in DefaultEngine.ini)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Join-Path $PSScriptRoot '..'
}

$ProjectRoot = [System.IO.Path]::GetFullPath($ProjectRoot)

if ([string]::IsNullOrWhiteSpace($SourcePath)) {
    $SourcePath = Join-Path $ProjectRoot 'Resources\Branding\AndroidAppIcon.png'
}
if ([string]::IsNullOrWhiteSpace($LogoPath)) {
    $LogoPath = Join-Path $ProjectRoot 'Resources\Branding\Logo.png'
}

$SourcePath = [System.IO.Path]::GetFullPath($SourcePath)
$LogoPath = [System.IO.Path]::GetFullPath($LogoPath)

if (-not (Test-Path -LiteralPath $SourcePath -PathType Leaf)) {
    throw "Android icon source was not found: $SourcePath"
}

if ([System.IO.Path]::GetExtension($SourcePath) -ine '.png') {
    throw 'Android icon source must be a PNG file.'
}

Add-Type -AssemblyName System.Drawing

# Adaptive icon geometry (dp): 108 canvas, inner 72 visible, launcher masks cut the rest.
$AdaptiveCanvasDp = 108
$AdaptiveVisibleDp = 72
# Launch image: portrait canvas large enough for QHD phones; the engine center-crops it to the screen.
$SplashWidth = 1440
$SplashHeight = 3120
# Home screen sky color (sRGB) so the launch image, the loading screen and Home read as one background.
$SplashBackground = [System.Drawing.Color]::FromArgb(255, 124, 162, 191)
# Logo width as a fraction of the screen width. Matches the loading screen (640px of 1080px).
$SplashLogoWidthRatio = 0.593
# Logo center as a fraction of the screen height (slightly above center, like the Home layout).
$SplashLogoCenterRatio = 0.44

function New-Graphics {
    param([Parameter(Mandatory)][System.Drawing.Bitmap]$Bitmap)

    $graphics = [System.Drawing.Graphics]::FromImage($Bitmap)
    $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    return $graphics
}

function Save-Png {
    param(
        [Parameter(Mandatory)][System.Drawing.Bitmap]$Bitmap,
        [Parameter(Mandatory)][string]$DestinationPath
    )

    $directory = [System.IO.Path]::GetDirectoryName($DestinationPath)
    [System.IO.Directory]::CreateDirectory($directory) | Out-Null
    $Bitmap.Save($DestinationPath, [System.Drawing.Imaging.ImageFormat]::Png)
}

function New-ResizedPng {
    param(
        [Parameter(Mandatory)]
        [System.Drawing.Image]$SourceImage,

        [Parameter(Mandatory)]
        [int]$Size,

        [Parameter(Mandatory)]
        [string]$DestinationPath
    )

    $bitmap = New-Object System.Drawing.Bitmap(
        $Size,
        $Size,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)

    try {
        $graphics = New-Graphics -Bitmap $bitmap
        try {
            $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
            $graphics.DrawImage(
                $SourceImage,
                (New-Object System.Drawing.Rectangle(0, 0, $Size, $Size)),
                0,
                0,
                $SourceImage.Width,
                $SourceImage.Height,
                [System.Drawing.GraphicsUnit]::Pixel)
        }
        finally {
            $graphics.Dispose()
        }

        Save-Png -Bitmap $bitmap -DestinationPath $DestinationPath
    }
    finally {
        $bitmap.Dispose()
    }
}

function New-AdaptiveForegroundPng {
    # The whole artwork scaled to the visible 72dp square, centered on a transparent 108dp canvas.
    param(
        [Parameter(Mandatory)][System.Drawing.Image]$SourceImage,
        [Parameter(Mandatory)][int]$CanvasSize,
        [Parameter(Mandatory)][int]$VisibleSize,
        [Parameter(Mandatory)][string]$DestinationPath
    )

    $bitmap = New-Object System.Drawing.Bitmap($CanvasSize, $CanvasSize, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $graphics = New-Graphics -Bitmap $bitmap
        try {
            $graphics.Clear([System.Drawing.Color]::Transparent)
            $offset = [int](($CanvasSize - $VisibleSize) / 2)
            $graphics.DrawImage(
                $SourceImage,
                (New-Object System.Drawing.Rectangle($offset, $offset, $VisibleSize, $VisibleSize)),
                0, 0, $SourceImage.Width, $SourceImage.Height,
                [System.Drawing.GraphicsUnit]::Pixel)
        }
        finally {
            $graphics.Dispose()
        }
        Save-Png -Bitmap $bitmap -DestinationPath $DestinationPath
    }
    finally {
        $bitmap.Dispose()
    }
}

function New-AdaptiveBackgroundPng {
    # A soft color field made from the artwork (downscale to 8x8, then upscale) so parallax and mask edges
    # show matching colors instead of a flat fill. System.Drawing has no blur, this is the equivalent.
    param(
        [Parameter(Mandatory)][System.Drawing.Image]$SourceImage,
        [Parameter(Mandatory)][int]$CanvasSize,
        [Parameter(Mandatory)][string]$DestinationPath
    )

    $tiny = New-Object System.Drawing.Bitmap(8, 8, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $bitmap = New-Object System.Drawing.Bitmap($CanvasSize, $CanvasSize, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $attributes = New-Object System.Drawing.Imaging.ImageAttributes
    try {
        $graphics = New-Graphics -Bitmap $tiny
        try {
            $graphics.DrawImage($SourceImage, (New-Object System.Drawing.Rectangle(0, 0, 8, 8)), 0, 0, $SourceImage.Width, $SourceImage.Height, [System.Drawing.GraphicsUnit]::Pixel)
        }
        finally {
            $graphics.Dispose()
        }

        # TileFlipXY keeps the bicubic upscale from fading to transparent at the borders.
        $attributes.SetWrapMode([System.Drawing.Drawing2D.WrapMode]::TileFlipXY)
        $graphics = New-Graphics -Bitmap $bitmap
        try {
            $graphics.Clear([System.Drawing.Color]::Black)
            $graphics.DrawImage($tiny, (New-Object System.Drawing.Rectangle(0, 0, $CanvasSize, $CanvasSize)), 0, 0, 8, 8, [System.Drawing.GraphicsUnit]::Pixel, $attributes)
        }
        finally {
            $graphics.Dispose()
        }
        Save-Png -Bitmap $bitmap -DestinationPath $DestinationPath
    }
    finally {
        $attributes.Dispose()
        $bitmap.Dispose()
        $tiny.Dispose()
    }
}

function New-SplashPng {
    param(
        [Parameter(Mandatory)][System.Drawing.Image]$LogoImage,
        [Parameter(Mandatory)][string]$DestinationPath
    )

    $bitmap = New-Object System.Drawing.Bitmap($SplashWidth, $SplashHeight, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
    try {
        $graphics = New-Graphics -Bitmap $bitmap
        try {
            $graphics.Clear($SplashBackground)
            $logoWidth = [int]($SplashWidth * $SplashLogoWidthRatio)
            $logoHeight = [int]($logoWidth * $LogoImage.Height / $LogoImage.Width)
            $left = [int](($SplashWidth - $logoWidth) / 2)
            $top = [int]($SplashHeight * $SplashLogoCenterRatio - $logoHeight / 2)
            $graphics.DrawImage(
                $LogoImage,
                (New-Object System.Drawing.Rectangle($left, $top, $logoWidth, $logoHeight)),
                0, 0, $LogoImage.Width, $LogoImage.Height,
                [System.Drawing.GraphicsUnit]::Pixel)
        }
        finally {
            $graphics.Dispose()
        }
        Save-Png -Bitmap $bitmap -DestinationPath $DestinationPath
    }
    finally {
        $bitmap.Dispose()
    }
}

function Install-GeneratedFile {
    param(
        [Parameter(Mandatory)]
        [string]$GeneratedPath,

        [Parameter(Mandatory)]
        [string]$DestinationPath
    )

    $destinationDirectory = [System.IO.Path]::GetDirectoryName($DestinationPath)
    [System.IO.Directory]::CreateDirectory($destinationDirectory) | Out-Null

    $pendingPath = "$DestinationPath.pending"
    $backupPath = "$DestinationPath.backup"
    try {
        [System.IO.File]::Copy($GeneratedPath, $pendingPath, $true)

        if ([System.IO.File]::Exists($DestinationPath)) {
            if ([System.IO.File]::Exists($backupPath)) {
                [System.IO.File]::Delete($backupPath)
            }

            [System.IO.File]::Replace($pendingPath, $DestinationPath, $backupPath)
        }
        else {
            [System.IO.File]::Move($pendingPath, $DestinationPath)
        }
    }
    finally {
        if ([System.IO.File]::Exists($pendingPath)) {
            [System.IO.File]::Delete($pendingPath)
        }

        if ([System.IO.File]::Exists($backupPath)) {
            [System.IO.File]::Delete($backupPath)
        }
    }
}

function Test-GeneratedSize {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][int]$Width,
        [Parameter(Mandatory)][int]$Height
    )

    $image = [System.Drawing.Image]::FromFile($Path)
    try {
        if ($image.Width -ne $Width -or $image.Height -ne $Height) {
            throw "Generated image has an invalid size: $Path ($($image.Width)x$($image.Height), expected ${Width}x${Height})"
        }
    }
    finally {
        $image.Dispose()
    }
}

$sourceImage = $null
$logoImage = $null
$temporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("ProjectGAndroidIcons_" + [System.Guid]::NewGuid().ToString('N'))

try {
    $sourceImage = [System.Drawing.Image]::FromFile($SourcePath)

    if ($sourceImage.RawFormat.Guid -ne [System.Drawing.Imaging.ImageFormat]::Png.Guid) {
        throw 'Android icon source content must use PNG format.'
    }

    if ($sourceImage.Width -ne $sourceImage.Height) {
        throw "Android icon source must be square. Actual size: $($sourceImage.Width)x$($sourceImage.Height)"
    }

    if ($sourceImage.Width -lt 512) {
        throw "Android icon source must be at least 512x512. Actual size: $($sourceImage.Width)x$($sourceImage.Height)"
    }

    # RelativePath, Width, Height, Kind (Legacy | Foreground | Background | Splash)
    $outputs = @(
        @{ RelativePath = 'Build\Android\res\drawable\icon.png'; Width = 48; Height = 48; Kind = 'Legacy' },
        @{ RelativePath = 'Build\Android\res\drawable-ldpi\icon.png'; Width = 36; Height = 36; Kind = 'Legacy' },
        @{ RelativePath = 'Build\Android\res\drawable-mdpi\icon.png'; Width = 48; Height = 48; Kind = 'Legacy' },
        @{ RelativePath = 'Build\Android\res\drawable-hdpi\icon.png'; Width = 72; Height = 72; Kind = 'Legacy' },
        @{ RelativePath = 'Build\Android\res\drawable-xhdpi\icon.png'; Width = 96; Height = 96; Kind = 'Legacy' },
        @{ RelativePath = 'Build\Android\res\drawable-xxhdpi\icon.png'; Width = 144; Height = 144; Kind = 'Legacy' },
        @{ RelativePath = 'Build\Android\res\drawable-xxxhdpi\icon.png'; Width = 192; Height = 192; Kind = 'Legacy' },
        @{ RelativePath = 'ProjectG.png'; Width = 192; Height = 192; Kind = 'Legacy' }
    )

    # Adaptive layers per density bucket (dp * density scale).
    $densities = @(
        @{ Folder = 'drawable-mdpi'; Scale = 1.0 },
        @{ Folder = 'drawable-hdpi'; Scale = 1.5 },
        @{ Folder = 'drawable-xhdpi'; Scale = 2.0 },
        @{ Folder = 'drawable-xxhdpi'; Scale = 3.0 },
        @{ Folder = 'drawable-xxxhdpi'; Scale = 4.0 }
    )
    foreach ($density in $densities) {
        $canvas = [int]($AdaptiveCanvasDp * $density.Scale)
        $outputs += @{ RelativePath = "Build\Android\res\$($density.Folder)\icon_foreground.png"; Width = $canvas; Height = $canvas; Kind = 'Foreground'; Visible = [int]($AdaptiveVisibleDp * $density.Scale) }
        $outputs += @{ RelativePath = "Build\Android\res\$($density.Folder)\icon_background.png"; Width = $canvas; Height = $canvas; Kind = 'Background' }
    }

    if (Test-Path -LiteralPath $LogoPath -PathType Leaf) {
        $logoImage = [System.Drawing.Image]::FromFile($LogoPath)
        $outputs += @{ RelativePath = 'Build\Android\res\drawable\splashscreen_portrait.png'; Width = $SplashWidth; Height = $SplashHeight; Kind = 'Splash' }
    }
    else {
        Write-Warning "Logo source was not found, launch image skipped: $LogoPath"
    }

    foreach ($output in $outputs) {
        $generatedPath = Join-Path $temporaryRoot $output.RelativePath
        switch ($output.Kind) {
            'Legacy' { New-ResizedPng -SourceImage $sourceImage -Size $output.Width -DestinationPath $generatedPath }
            'Foreground' { New-AdaptiveForegroundPng -SourceImage $sourceImage -CanvasSize $output.Width -VisibleSize $output.Visible -DestinationPath $generatedPath }
            'Background' { New-AdaptiveBackgroundPng -SourceImage $sourceImage -CanvasSize $output.Width -DestinationPath $generatedPath }
            'Splash' { New-SplashPng -LogoImage $logoImage -DestinationPath $generatedPath }
        }
        Test-GeneratedSize -Path $generatedPath -Width $output.Width -Height $output.Height
    }

    # Generate and validate every image before replacing any project file.
    foreach ($output in $outputs) {
        $generatedPath = Join-Path $temporaryRoot $output.RelativePath
        $destinationPath = Join-Path $ProjectRoot $output.RelativePath
        Install-GeneratedFile -GeneratedPath $generatedPath -DestinationPath $destinationPath
        Write-Host ("Generated {0} ({1}x{2})" -f $output.RelativePath, $output.Width, $output.Height)
    }

    Write-Host 'Android branding image generation completed.' -ForegroundColor Green
}
finally {
    if ($null -ne $sourceImage) {
        $sourceImage.Dispose()
    }
    if ($null -ne $logoImage) {
        $logoImage.Dispose()
    }

    if (Test-Path -LiteralPath $temporaryRoot) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
