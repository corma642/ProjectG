<#
.SYNOPSIS
    ProjectG 변경 검증을 한 번에 실행합니다: Editor 빌드 → Blueprint 전체 컴파일 → 자동화 테스트 → (선택) 위젯 QA → (선택) Android 패키징.

.DESCRIPTION
    각 단계는 실패하면 즉시 멈추고 종료 코드 1을 반환합니다. 로그는 Saved\Verify 아래에 남습니다.
    완료 기준은 프로젝트 복구 가이드와 같습니다: UBT Succeeded, Blueprint 오류 0 / 경고 0 / 로드 실패 0, ProjectG.Core 테스트 전부 Success.

.EXAMPLE
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\Verify-Project.ps1
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\Verify-Project.ps1 -SkipBuild -SkipBlueprints
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\Verify-Project.ps1 -WidgetQA
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\Verify-Project.ps1 -Package
#>
[CmdletBinding()]
param(
    [string]$ProjectRoot,
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.6',
    [string]$TestFilter = 'ProjectG.Core',
    [switch]$SkipBuild,
    [switch]$SkipBlueprints,
    [switch]$SkipTests,
    [switch]$WidgetQA,
    [switch]$Package
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Join-Path $PSScriptRoot '..'
}
$ProjectRoot = [System.IO.Path]::GetFullPath($ProjectRoot)
$ProjectFile = Join-Path $ProjectRoot 'ProjectG.uproject'
$EditorCmd = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$BuildBat = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$RunUAT = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
$LogRoot = Join-Path $ProjectRoot 'Saved\Verify'

foreach ($required in @($ProjectFile, $EditorCmd, $BuildBat)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Required file was not found: $required"
    }
}
New-Item -ItemType Directory -Force -Path $LogRoot | Out-Null

function Write-Step {
    param([string]$Message)
    Write-Host ''
    Write-Host "== $Message" -ForegroundColor Cyan
}

function Invoke-Timed {
    param([string]$Name, [scriptblock]$Action)
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    & $Action
    $stopwatch.Stop()
    Write-Host ("   {0} finished in {1:n0}s" -f $Name, $stopwatch.Elapsed.TotalSeconds) -ForegroundColor DarkGray
}

# UnrealEditor-Cmd는 실패해도 종료 코드가 0일 수 있어 로그 본문으로 결과를 판정합니다.
function Invoke-EditorCmd {
    param([string]$LogName, [string[]]$Arguments, [switch]$Render)
    $logPath = Join-Path $LogRoot $LogName
    if (Test-Path -LiteralPath $logPath) {
        Remove-Item -LiteralPath $logPath -Force
    }
    # 위젯을 실제로 그려야 하는 단계만 RHI를 켠다(창 없이 오프스크린 렌더링).
    $rhiArgument = if ($Render) { '-RenderOffscreen' } else { '-NullRHI' }
    & $EditorCmd $ProjectFile @Arguments -Unattended -NoSplash -NoSound $rhiArgument "-abslog=$logPath" | Out-Null
    if (-not (Test-Path -LiteralPath $logPath)) {
        throw "Editor did not produce a log: $logPath"
    }
    return $logPath
}

if (-not $SkipBuild) {
    Write-Step 'Build ProjectGEditor Win64 Development'
    Invoke-Timed 'Build' {
        & $BuildBat ProjectGEditor Win64 Development $ProjectFile -NoHotReloadFromIDE | Out-Host
        if ($LASTEXITCODE -ne 0) {
            throw "Editor build failed with exit code $LASTEXITCODE."
        }
    }
}

if (-not $SkipBlueprints) {
    Write-Step 'CompileAllBlueprints'
    Invoke-Timed 'CompileAllBlueprints' {
        $logPath = Invoke-EditorCmd 'CompileAllBlueprints.log' @('-run=CompileAllBlueprints')
        $summary = Select-String -LiteralPath $logPath -Pattern 'Compiling Completed with (\d+) errors and (\d+) warnings and (\d+) blueprints that failed to load' | Select-Object -Last 1
        if (-not $summary) {
            throw "CompileAllBlueprints summary line was not found. See $logPath"
        }
        $errors = [int]$summary.Matches[0].Groups[1].Value
        $warnings = [int]$summary.Matches[0].Groups[2].Value
        $failedLoads = [int]$summary.Matches[0].Groups[3].Value
        Write-Host "   errors=$errors warnings=$warnings failedLoads=$failedLoads"
        if ($errors -ne 0 -or $warnings -ne 0 -or $failedLoads -ne 0) {
            Select-String -LiteralPath $logPath -Pattern 'LogBlueprint: (Error|Warning)|Failed to load' | ForEach-Object { Write-Host "   $($_.Line)" -ForegroundColor Yellow }
            throw "CompileAllBlueprints reported problems. See $logPath"
        }
    }
}

if (-not $SkipTests) {
    Write-Step "Automation tests ($TestFilter)"
    Invoke-Timed 'Automation' {
        $logPath = Invoke-EditorCmd 'Automation.log' @("-ExecCmds=Automation RunTests $TestFilter; Quit", '-TestExit=Automation Test Queue Empty')
        $results = Select-String -LiteralPath $logPath -Pattern 'Test Completed\. Result=\{(\w+)\} Name=\{[^}]*\} Path=\{([^}]*)\}'
        if (-not $results) {
            throw "No automation test results were found. See $logPath"
        }
        $failed = 0
        foreach ($result in $results) {
            $status = $result.Matches[0].Groups[1].Value
            $path = $result.Matches[0].Groups[2].Value
            $color = if ($status -eq 'Success') { 'Green' } else { 'Red'; $failed++ }
            Write-Host "   [$status] $path" -ForegroundColor $color
        }
        if ($failed -ne 0) {
            throw "$failed automation test(s) failed. See $logPath"
        }
    }
}

if ($WidgetQA) {
    Write-Step 'Widget QA (render + hit test)'
    Invoke-Timed 'Widget QA' {
        $script = Join-Path $PSScriptRoot 'EditorScripts\qa_widgets.py'
        $logPath = Invoke-EditorCmd 'WidgetQA.log' @("-ExecutePythonScript=$script") -Render
        Select-String -LiteralPath $logPath -Pattern 'LogPython: ### (.*)$' | ForEach-Object { Write-Host "   $($_.Matches[0].Groups[1].Value)" }
        $pythonErrors = Select-String -LiteralPath $logPath -Pattern 'LogPython: Error'
        if ($pythonErrors -or -not (Select-String -LiteralPath $logPath -Pattern '### WIDGET_QA: PASS' -Quiet)) {
            $pythonErrors | Select-Object -First 10 | ForEach-Object { Write-Host "   $($_.Line)" -ForegroundColor Yellow }
            throw "Widget QA failed. See $logPath and $(Join-Path $LogRoot 'WidgetQA')"
        }
        Write-Host "   PNG: $(Join-Path $LogRoot 'WidgetQA')"
    }
}

if ($Package) {
    Write-Step 'Package Android (ASTC, arm64, Shipping)'
    if (-not (Test-Path -LiteralPath $RunUAT)) {
        throw "RunUAT was not found: $RunUAT"
    }
    # 명령줄 UBT는 ANDROID_HOME / NDKROOT / JAVA_HOME 환경 변수로 Android SDK를 찾고, 에디터의 Android SDK 설정은 읽지 않는다.
    # 비어 있으면 에디터가 Project Settings > Platforms > Android SDK에 저장한 사용자 설정(UserEngine.ini)을 이 프로세스에만 적용한다.
    $userEngineIni = Join-Path $env:LOCALAPPDATA 'Unreal Engine\Engine\Config\UserEngine.ini'
    function Get-AndroidSdkSettingPath {
        param([string]$Key)
        if (-not (Test-Path -LiteralPath $userEngineIni)) { return $null }
        $match = Select-String -LiteralPath $userEngineIni -Pattern ('^{0}=\(Path="([^"]+)"\)' -f $Key) | Select-Object -First 1
        if ($match) { return $match.Matches[0].Groups[1].Value.Replace('/', '\') }
        return $null
    }
    if (-not $env:ANDROID_HOME) { $env:ANDROID_HOME = Get-AndroidSdkSettingPath 'SDKPath' }
    if (-not $env:NDKROOT) { $env:NDKROOT = Get-AndroidSdkSettingPath 'NDKPath' }
    if (-not $env:JAVA_HOME) { $env:JAVA_HOME = Get-AndroidSdkSettingPath 'JavaPath' }
    Write-Host "   ANDROID_HOME=$env:ANDROID_HOME"
    Write-Host "   NDKROOT=$env:NDKROOT"
    Write-Host "   JAVA_HOME=$env:JAVA_HOME"
    # 일부 에이전트/CI 셸은 NoDefaultCurrentDirectoryInExePath=1을 설정해 cmd가 현재 폴더의 실행 파일을 찾지 않는다.
    # UE의 AFS 보조 앱 빌드(cmd /c gradlew.bat)와 XGE 워커 실행이 그 상태에서 실패하므로 이 프로세스에서만 지운다.
    Remove-Item Env:NoDefaultCurrentDirectoryInExePath -ErrorAction SilentlyContinue
    # Gradle 증분 패키징은 기존 APK를 제자리에서 갱신하며 교체된 main.obb.png 자리를 빈 공간으로 남겨 APK가 수백 MB 부풀 수 있다.
    # 복구 가이드의 Full Rebuild와 같은 결과가 나오도록 Android 중간 산출물을 지우고 Gradle 프로젝트를 새로 만든다. Cook 결과는 유지된다.
    $androidIntermediate = Join-Path $ProjectRoot 'Intermediate\Android'
    Remove-Item -LiteralPath $androidIntermediate -Recurse -Force -ErrorAction SilentlyContinue
    if (Test-Path -LiteralPath $androidIntermediate) {
        # 잠긴 파일이 남으면 증분 패키징이 그대로 일어날 수 있다. 패키징은 계속하되 APK 크기를 확인하도록 알린다.
        Write-Warning "Intermediate\Android was not fully removed; check the APK size after packaging."
    }
    Invoke-Timed 'Package' {
        $archiveDir = Join-Path $LogRoot 'Android_ASTC'
        # 쿠커 셰이더 컴파일이 XGE 컨트롤러를 거치면 Incredibuild 상태(라이선스·워커 실행 환경)에 따라 영원히 기다릴 수 있다
        # ("No shader compile worker state change"). 항상 로컬 ShaderCompileWorker를 쓰도록 XGEController 플러그인을 쿠커에서만 끈다.
        & $RunUAT BuildCookRun "-project=$ProjectFile" -noP4 -platform=Android -cookflavor=ASTC -clientconfig=Shipping `
            -build -cook -stage -pak -iostore -compressed -package -archive "-archivedirectory=$archiveDir" -utf8output `
            '-additionalcookeroptions=-DisablePlugins=XGEController' | Out-Host
        if ($LASTEXITCODE -ne 0) {
            throw "Android packaging failed with exit code $LASTEXITCODE."
        }
        $apk = Get-ChildItem -LiteralPath $archiveDir -Recurse -Filter 'ProjectG-Android-Shipping-arm64.apk' | Select-Object -First 1
        if ($apk) {
            Write-Host ("   APK: {0} ({1:n0} bytes)" -f $apk.FullName, $apk.Length)
        }
    }
}

Write-Host ''
Write-Host 'All requested verification steps passed.' -ForegroundColor Green
