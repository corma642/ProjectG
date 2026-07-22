[CmdletBinding()]
param(
    [string]$SourcePath,
    [string]$ProjectRoot
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Join-Path $PSScriptRoot '..'
}

$ProjectRoot = [System.IO.Path]::GetFullPath($ProjectRoot)

if ([string]::IsNullOrWhiteSpace($SourcePath)) {
    $SourcePath = Join-Path $ProjectRoot 'Resources\Branding\AndroidAppIcon.png'
}

$SourcePath = [System.IO.Path]::GetFullPath($SourcePath)

if (-not (Test-Path -LiteralPath $SourcePath -PathType Leaf)) {
    throw "Android icon source was not found: $SourcePath"
}

if ([System.IO.Path]::GetExtension($SourcePath) -ine '.png') {
    throw 'Android icon source must be a PNG file.'
}

Add-Type -AssemblyName System.Drawing

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
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        try {
            $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
            $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
            $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
            $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
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

        $directory = [System.IO.Path]::GetDirectoryName($DestinationPath)
        [System.IO.Directory]::CreateDirectory($directory) | Out-Null
        $bitmap.Save($DestinationPath, [System.Drawing.Imaging.ImageFormat]::Png)
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

$sourceImage = $null
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

    $outputs = @(
        @{ RelativePath = 'Build\Android\res\drawable\icon.png'; Size = 48 },
        @{ RelativePath = 'Build\Android\res\drawable-ldpi\icon.png'; Size = 36 },
        @{ RelativePath = 'Build\Android\res\drawable-mdpi\icon.png'; Size = 48 },
        @{ RelativePath = 'Build\Android\res\drawable-hdpi\icon.png'; Size = 72 },
        @{ RelativePath = 'Build\Android\res\drawable-xhdpi\icon.png'; Size = 96 },
        @{ RelativePath = 'Build\Android\res\drawable-xxhdpi\icon.png'; Size = 144 },
        @{ RelativePath = 'Build\Android\res\drawable-xxxhdpi\icon.png'; Size = 192 },
        @{ RelativePath = 'ProjectG.png'; Size = 192 },
        @{ RelativePath = 'Resources\Branding\GooglePlayIcon512.png'; Size = 512 }
    )

    foreach ($output in $outputs) {
        $generatedPath = Join-Path $temporaryRoot $output.RelativePath
        New-ResizedPng -SourceImage $sourceImage -Size $output.Size -DestinationPath $generatedPath

        $generatedImage = [System.Drawing.Image]::FromFile($generatedPath)
        try {
            if ($generatedImage.Width -ne $output.Size -or $generatedImage.Height -ne $output.Size) {
                throw "Generated icon has an invalid size: $($output.RelativePath)"
            }
        }
        finally {
            $generatedImage.Dispose()
        }
    }

    # Generate and validate every image before replacing any project file.
    foreach ($output in $outputs) {
        $generatedPath = Join-Path $temporaryRoot $output.RelativePath
        $destinationPath = Join-Path $ProjectRoot $output.RelativePath
        Install-GeneratedFile -GeneratedPath $generatedPath -DestinationPath $destinationPath
        Write-Host ("Generated {0} ({1}x{1})" -f $output.RelativePath, $output.Size)
    }

    Write-Host 'Android launcher icon generation completed.' -ForegroundColor Green
}
finally {
    if ($null -ne $sourceImage) {
        $sourceImage.Dispose()
    }

    if (Test-Path -LiteralPath $temporaryRoot) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
