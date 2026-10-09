<#
.SYNOPSIS
    작업 결과 확인용 인게임 영상을 사람 없이 녹화합니다: 화면 없는 게임 실행 → 쇼케이스 시나리오 재생 → PNG 프레임 → MP4.

.DESCRIPTION
    시나리오는 "<초> <콘솔 명령>" 줄로 이루어진 텍스트이며 PG.Showcase.* 명령(Source/ProjectG/Debug/PGShowcaseCommands.cpp)으로 게임을 진행시킵니다.
    시나리오 안의 {OUT}은 프레임 출력 폴더로 바뀌고, "# seed N" 줄은 트랙 세그먼트 순서를 고정합니다.
    녹화 중 런 결과가 개발자 세이브(Saved\SaveGames)에 반영되지 않도록 실행 전에 백업하고 끝나면 되돌립니다.
    결과물은 Saved\Verify\Showcase\<이름>.mp4 이고 소리는 없습니다. 인코딩에는 Blender에 포함된 FFmpeg를 씁니다.

.EXAMPLE
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\Record-Showcase.ps1
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\Record-Showcase.ps1 -Scenario .\Tools\Showcase\run_effects.txt -Seed 2 -KeepFrames
#>
[CmdletBinding()]
param(
    [string]$Scenario,
    [int]$Seed = -1,
    [string]$Map = '/Game/Level/Coastal/LV_Coastal?StartFlow=RetryCountdown',
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.6',
    [string]$Blender,
    [int]$TimeoutSeconds = 300,
    [switch]$KeepFrames
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$ProjectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if ([string]::IsNullOrWhiteSpace($Scenario)) {
    $Scenario = Join-Path $PSScriptRoot 'Showcase\run_effects.txt'
}
$Scenario = (Resolve-Path -LiteralPath $Scenario).Path
$Name = [System.IO.Path]::GetFileNameWithoutExtension($Scenario)
$Editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
if ([string]::IsNullOrWhiteSpace($Blender)) {
    $Blender = Get-ChildItem 'C:\Program Files\Blender Foundation\Blender *\blender.exe' -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
}
foreach ($required in @($Editor, $Blender)) {
    if ([string]::IsNullOrWhiteSpace($required) -or -not (Test-Path -LiteralPath $required)) {
        throw "Required program was not found: $required"
    }
}

$OutRoot = Join-Path $ProjectRoot 'Saved\Verify\Showcase'
$FrameDir = Join-Path $OutRoot "$Name\frames"
$ScriptFile = Join-Path $OutRoot "$Name\script.txt"
$LogFile = Join-Path $OutRoot "$Name\game.log"
$VideoFile = Join-Path $OutRoot "$Name.mp4"
$SaveDir = Join-Path $ProjectRoot 'Saved\SaveGames'
$SaveBackup = Join-Path $OutRoot '_savebackup'

# 콘솔 명령 인자는 공백으로, -ExecCmds는 쉼표로 나뉘고 작은따옴표는 큰따옴표로 바뀐다. 그런 경로면 스크립트가 열리지 않아 게임이 제한 시간까지 멈춰 있으므로 미리 막는다.
if ("$ScriptFile$FrameDir" -match "[\s,']") {
    throw "Showcase paths must not contain whitespace, commas or apostrophes: $ScriptFile"
}

function Restore-DeveloperSave {
    # 백업 폴더가 남아 있다는 것은 세이브가 아직 녹화 런의 결과로 덮여 있다는 뜻이다. 폴더째 바꿔 녹화가 새로 만든 슬롯 파일도 남기지 않는다.
    if (-not (Test-Path -LiteralPath $SaveBackup)) {
        return
    }
    if (Test-Path -LiteralPath $SaveDir) {
        Remove-Item -LiteralPath $SaveDir -Recurse -Force
    }
    if (@(Get-ChildItem -LiteralPath $SaveBackup -Force).Count -gt 0) {
        Move-Item -LiteralPath $SaveBackup -Destination $SaveDir
    }
    else {
        Remove-Item -LiteralPath $SaveBackup -Force
    }
}

# 이전 녹화가 중간에 끊겨 되돌리지 못한 세이브가 있으면 새 백업으로 덮어쓰기 전에 먼저 되돌린다.
Restore-DeveloperSave

foreach ($stale in @((Join-Path $OutRoot $Name), $VideoFile)) {
    if (Test-Path -LiteralPath $stale) {
        Remove-Item -LiteralPath $stale -Recurse -Force
    }
}
New-Item -ItemType Directory -Force -Path $FrameDir | Out-Null

# 콘솔 명령 인자는 공백으로 나뉘고 역슬래시는 엔진이 경로로 풀지 못하는 경우가 있어 슬래시 경로를 넘긴다.
$lines = Get-Content -LiteralPath $Scenario -Encoding UTF8
if ($Seed -lt 0) {
    $seedLine = $lines | Where-Object { $_ -match '^#\s*seed\s+(\d+)' } | Select-Object -First 1
    $Seed = if ($seedLine -and $seedLine -match '(\d+)') { [int]$Matches[1] } else { 1 }
}
# 게임은 '#'로 시작하는 줄을 건너뛰므로 실행되는 "<초> <명령>" 줄에서만 FPS를 읽는다.
$dumpPattern = '^\s*[\d.]+\s+PG\.Showcase\.DumpFrames\s+\S+\s+(\d+)\s+\d+'
$dumpLine = $lines | Where-Object { $_ -match $dumpPattern } | Select-Object -First 1
if (-not $dumpLine) {
    throw "The scenario has no 'PG.Showcase.DumpFrames {OUT} <Fps> <Frames>' line: $Scenario"
}
$null = $dumpLine -match $dumpPattern
$Fps = [int]$Matches[1]
$lines | ForEach-Object { $_.Replace('{OUT}', $FrameDir.Replace('\', '/')) } | Set-Content -LiteralPath $ScriptFile -Encoding ASCII

$arguments = @(
    "`"$(Join-Path $ProjectRoot 'ProjectG.uproject')`"", $Map,
    '-game', '-windowed', '-ResX=540', '-ResY=1170', '-RenderOffscreen',
    '-NoSplash', '-NoSound', '-Unattended', '-NoLoadingScreen', '-NoScreenMessages',
    "-PGShowcaseSeed=$Seed",
    "`"-ExecCmds=PG.Showcase.Play $($ScriptFile.Replace('\', '/'))`"",
    "`"-abslog=$LogFile`""
)

Write-Host "== Recording '$Name' (seed $Seed, $Fps fps)" -ForegroundColor Cyan
if (Test-Path -LiteralPath $SaveDir) {
    Copy-Item -LiteralPath $SaveDir -Destination $SaveBackup -Recurse
}
else {
    # 세이브가 없던 상태도 되돌릴 수 있게 빈 백업을 표시로 남긴다.
    New-Item -ItemType Directory -Force -Path $SaveBackup | Out-Null
}
try {
    $process = Start-Process -FilePath $Editor -ArgumentList $arguments -PassThru -WindowStyle Hidden
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        # 런이 시작되지 않는 등으로 덤프가 끝나지 않으면 화면 없는 프로세스가 남으므로 강제로 끝낸다.
        $process.Kill()
        throw "The game did not finish within $TimeoutSeconds seconds. See $LogFile"
    }
}
finally {
    Restore-DeveloperSave
}

# 종료 코드는 시나리오가 의도대로 진행됐는지 알려 주지 않으므로 로그로 판단한다.
$log = if (Test-Path -LiteralPath $LogFile) { Get-Content -LiteralPath $LogFile } else { @() }
$problems = @($log | Where-Object { $_ -match 'LogPGShowcase: (Warning|Error)' })
$frames = @(Get-ChildItem -LiteralPath $FrameDir -Filter '*.png' | Sort-Object Name)
if ($problems.Count -gt 0 -or $frames.Count -eq 0 -or -not ($log -match 'Frame dump finished')) {
    $problems | ForEach-Object { Write-Host $_ -ForegroundColor Red }
    throw "Showcase run failed: $($frames.Count) frames, $($problems.Count) showcase warnings. See $LogFile"
}

Add-Type -AssemblyName System.Drawing
$first = [System.Drawing.Image]::FromFile($frames[0].FullName)
$width = $first.Width - ($first.Width % 2)
$height = $first.Height - ($first.Height % 2)
$first.Dispose()

Write-Host "== Encoding $($frames.Count) frames ($width x $height)" -ForegroundColor Cyan
$encodeScript = Join-Path $PSScriptRoot 'Showcase\encode_video.py'
# Blender는 스크립트가 예외로 끝나도 종료 코드 0을 돌려주므로 --python-exit-code로 실패를 종료 코드에 싣는다.
& $Blender -b --factory-startup --python-exit-code 1 --python $encodeScript -- $FrameDir $VideoFile $Fps $width $height | Select-String -Pattern '^###'
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $VideoFile)) {
    throw "Encoding failed (Blender exit code $LASTEXITCODE): $VideoFile"
}
# 프레임 PNG는 30초에 1GB가 넘으므로 영상이 만들어지면 지운다. 로그와 실행한 스크립트는 남긴다. 장면을 프레임으로 확인하려면 -KeepFrames를 준다.
if (-not $KeepFrames) {
    Remove-Item -LiteralPath $FrameDir -Recurse -Force
}

Write-Host ''
Write-Host ("Video: {0} ({1:n1} s, {2:n1} MB)" -f $VideoFile, ($frames.Count / $Fps), ((Get-Item -LiteralPath $VideoFile).Length / 1MB)) -ForegroundColor Green
