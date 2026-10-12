<#
.SYNOPSIS
    ProjectG 변경 검증을 한 번에 실행합니다: Editor 빌드 → Blueprint 전체 컴파일 → 자동화 테스트 → (선택) 위젯 QA → (선택) Android 릴리스 패키징.

.DESCRIPTION
    각 단계는 실패하면 즉시 멈추고 종료 코드 1을 반환합니다. 로그는 Saved\Verify 아래에 남습니다.
    완료 기준은 프로젝트 복구 가이드와 같습니다: UBT Succeeded, Blueprint 오류 0 / 경고 0 / 로드 실패 0, ProjectG.Core 테스트 전부 Success.

    -Package는 Google Play 업로드용 릴리스 경로입니다. 항상 전체 검증을 먼저 통과해야 하고(-Skip* 불가), 작업 트리에 커밋되지 않은 변경이 있으면 거부합니다(-AllowDirty로 로컬 테스트 빌드만 허용).
    배포 서명(-distribution)된 AAB와 universal APK를 만들고 Releases\v<StoreVersion>\에 심볼 zip, build-info.txt, sha256과 함께 보관합니다.
    StoreVersion(versionCode)은 지정하지 않으면 현재 커밋 수(git rev-list --count HEAD)이고 VersionDisplayName은 1.0.<StoreVersion>입니다. 롤백 재빌드는 -StoreVersion <문제 빌드+1>로 지정합니다.
    업로드 키는 Build\Android\upload.keystore, 비밀번호는 Git 제외 파일 Config\UserEngine.ini(KeyStorePassword·KeyPassword)에서 읽습니다.

.EXAMPLE
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\Verify-Project.ps1
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\Verify-Project.ps1 -SkipBuild -SkipBlueprints
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\Verify-Project.ps1 -WidgetQA
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\Verify-Project.ps1 -Package
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\Verify-Project.ps1 -Package -StoreVersion 141 -VersionName 1.0.1
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
    [switch]$Package,
    [int]$StoreVersion = 0,
    [string]$VersionName = '',
    [switch]$AllowDirty
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

# aapt2·apksigner·java는 정상 경로에서도 stderr에 쓴다. Windows PowerShell 5.1은 $ErrorActionPreference=Stop이면 그 줄을 종료 오류로 바꾸므로
# 네이티브 도구는 이 함수로 실행해 출력을 문자열 배열로만 받는다. 성공 여부는 $LASTEXITCODE와 출력 내용으로 판정한다.
function Invoke-Native {
    param([string]$Exe, [string[]]$Arguments)
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = & $Exe @Arguments 2>&1 | ForEach-Object { "$_" }
    }
    finally {
        $ErrorActionPreference = $previous
    }
    return @($output)
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

# 릴리스 빌드의 입력(커밋·버전)은 40분짜리 빌드를 시작하기 전에 확정한다.
$gitCommit = ''
$gitBranch = ''
$gitDirty = $false
if ($Package) {
    if ($SkipBuild -or $SkipBlueprints -or $SkipTests) {
        throw '-Package is the release path and always runs Build, CompileAllBlueprints and the automation tests. Remove -SkipBuild/-SkipBlueprints/-SkipTests.'
    }
    $gitStatus = @(& git -C $ProjectRoot status --porcelain)
    if ($LASTEXITCODE -ne 0) {
        throw 'git status failed. A release build needs a git checkout so the artifact maps to one commit.'
    }
    $gitDirty = $gitStatus.Count -gt 0
    if ($gitDirty -and -not $AllowDirty) {
        throw ("Working tree has uncommitted changes. Commit first, or pass -AllowDirty for a local test build.`n" + ($gitStatus -join "`n"))
    }
    $gitCommit = (& git -C $ProjectRoot rev-parse HEAD).Trim()
    $gitBranch = (& git -C $ProjectRoot rev-parse --abbrev-ref HEAD).Trim()
    if ($StoreVersion -le 0) {
        # 커밋 수는 develop에서 단조 증가하므로 Play versionCode로 쓴다. 다른 브랜치에서 릴리스하면 수가 작아질 수 있어 -StoreVersion으로 직접 지정한다.
        $StoreVersion = [int](& git -C $ProjectRoot rev-list --count HEAD)
    }
    if ([string]::IsNullOrWhiteSpace($VersionName)) {
        $VersionName = "1.0.$StoreVersion"
    }
    Write-Host "   Release input: commit=$gitCommit branch=$gitBranch dirty=$gitDirty StoreVersion=$StoreVersion VersionName=$VersionName"
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
    Write-Step "Package Android release (ASTC, arm64, Shipping, distribution, AAB) v$StoreVersion"
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

    # 배포 서명 입력은 UAT가 40분 뒤에 실패하기 전에 확인한다. 비밀번호는 Git 제외 Config\UserEngine.ini에만 둔다.
    $keystorePath = Join-Path $ProjectRoot 'Build\Android\upload.keystore'
    $projectUserEngineIni = Join-Path $ProjectRoot 'Config\UserEngine.ini'
    if (-not (Test-Path -LiteralPath $keystorePath)) {
        throw "Upload keystore was not found: $keystorePath (see the recovery guide, Android release signing)."
    }
    if (-not (Test-Path -LiteralPath $projectUserEngineIni) -or -not (Select-String -LiteralPath $projectUserEngineIni -Pattern '^KeyStorePassword=.+' -Quiet)) {
        throw "KeyStorePassword was not found in $projectUserEngineIni (git-ignored; see the recovery guide, Android release signing)."
    }

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
    $releaseDir = Join-Path $ProjectRoot "Releases\v$StoreVersion"
    if (Test-Path -LiteralPath $releaseDir) {
        # 같은 versionCode를 다시 만들면 이전 산출물과 섞이지 않도록 비운다(Play에 올린 빌드는 git 태그와 오프라인 백업이 원본이다).
        Remove-Item -LiteralPath $releaseDir -Recurse -Force
    }
    Invoke-Timed 'Package' {
        # UAT는 UEDeployAndroid를 같은 프로세스에서 실행하므로 명령줄 -ini: 오버라이드로 versionCode와 표시 버전을 덮어쓴다(UBT ConfigCache).
        $androidSection = '[/Script/AndroidRuntimeSettings.AndroidRuntimeSettings]'
        # 쿠커 셰이더 컴파일이 XGE 컨트롤러를 거치면 Incredibuild 상태(라이선스·워커 실행 환경)에 따라 영원히 기다릴 수 있다
        # ("No shader compile worker state change"). 항상 로컬 ShaderCompileWorker를 쓰도록 XGEController 플러그인을 쿠커에서만 끈다.
        & $RunUAT BuildCookRun "-project=$ProjectFile" -noP4 -platform=Android -cookflavor=ASTC -clientconfig=Shipping `
            -build -cook -stage -pak -iostore -compressed -package -distribution -archive "-archivedirectory=$releaseDir" -utf8output `
            "-ini:Engine:${androidSection}:StoreVersion=$StoreVersion" "-ini:Engine:${androidSection}:VersionDisplayName=$VersionName" `
            '-additionalcookeroptions=-DisablePlugins=XGEController' | Out-Host
        if ($LASTEXITCODE -ne 0) {
            throw "Android packaging failed with exit code $LASTEXITCODE."
        }
    }

    Write-Step 'Verify release artifacts'
    $aab = Get-ChildItem -LiteralPath $releaseDir -Filter '*.aab' | Select-Object -First 1
    if (-not $aab) { throw "No .aab was archived to $releaseDir (bEnableBundle must be True)." }
    $universalApk = Get-ChildItem -LiteralPath $releaseDir -Filter '*_universal.apk' | Select-Object -First 1
    if (-not $universalApk) { throw "No universal APK was archived to $releaseDir (bEnableUniversalAPK must be True)." }

    $buildToolsRoot = Join-Path $env:ANDROID_HOME 'build-tools'
    $buildTools = Get-ChildItem -LiteralPath $buildToolsRoot -Directory | Where-Object { Test-Path (Join-Path $_.FullName 'aapt2.exe') } | Sort-Object { [version]($_.Name -replace '[^0-9.].*$', '') } -Descending | Select-Object -First 1
    if (-not $buildTools) { throw "No Android build-tools with aapt2 under $buildToolsRoot" }
    $aapt2 = Join-Path $buildTools.FullName 'aapt2.exe'
    $apksigner = Join-Path $buildTools.FullName 'apksigner.bat'
    $zipalign = Join-Path $buildTools.FullName 'zipalign.exe'
    $readelf = Join-Path $env:NDKROOT 'toolchains\llvm\prebuilt\windows-x86_64\bin\llvm-readelf.exe'

    # 1) 매니페스트: debuggable 없음, versionCode/targetSdk 일치, GMS 컴포넌트 0건.
    $badging = Invoke-Native $aapt2 @('dump', 'badging', $universalApk.FullName)
    $packageLine = $badging | Where-Object { $_ -like 'package:*' } | Select-Object -First 1
    if (-not $packageLine) { throw 'aapt2 dump badging did not report the package line.' }
    $packageName = [regex]::Match($packageLine, "name='([^']+)'").Groups[1].Value
    $versionCode = [regex]::Match($packageLine, "versionCode='([^']+)'").Groups[1].Value
    $versionNameActual = [regex]::Match($packageLine, "versionName='([^']+)'").Groups[1].Value
    $targetSdkLine = $badging | Where-Object { $_ -like 'targetSdkVersion:*' } | Select-Object -First 1
    $targetSdk = [regex]::Match([string]$targetSdkLine, "'([^']+)'").Groups[1].Value
    Write-Host "   package=$packageName versionCode=$versionCode versionName=$versionNameActual targetSdk=$targetSdk"
    if ([string]$versionCode -ne [string]$StoreVersion) { throw "versionCode is $versionCode but StoreVersion is $StoreVersion (the -ini override did not apply)." }
    if ($badging | Where-Object { $_ -like 'application-debuggable*' }) { throw 'APK is debuggable; -distribution did not apply.' }
    $manifestDump = Invoke-Native $aapt2 @('dump', 'xmltree', '--file', 'AndroidManifest.xml', $universalApk.FullName)
    $gmsLines = @($manifestDump | Where-Object { $_ -match 'com\.google\.android\.gms' })
    if ($gmsLines.Count -ne 0) {
        $gmsLines | Select-Object -First 5 | ForEach-Object { Write-Host "   $_" -ForegroundColor Yellow }
        throw "$($gmsLines.Count) com.google.android.gms manifest entries remain (ProjectG_UPL.xml AARImports)."
    }
    Write-Host '   manifest: not debuggable, no com.google.android.gms entries'

    # 2) 서명: debug 인증서가 아니어야 한다.
    $signerOutput = Invoke-Native $apksigner @('verify', '--print-certs', $universalApk.FullName)
    $signerDn = ($signerOutput | Where-Object { $_ -like '*certificate DN:*' } | Select-Object -First 1)
    if (-not $signerDn) { throw ("apksigner could not read the signer:`n" + ($signerOutput -join "`n")) }
    if ($signerDn -match 'Android Debug') { throw "APK is signed with the debug certificate: $signerDn" }
    Write-Host "   signer: $($signerDn.Trim())"

    # 3) 16KB 페이지: 네이티브 라이브러리의 모든 LOAD 세그먼트 정렬이 0x4000 이상이어야 한다.
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [System.IO.Compression.ZipFile]::OpenRead($universalApk.FullName)
    try {
        $soEntries = @($zip.Entries | Where-Object { $_.FullName -like 'lib/arm64-v8a/*.so' })
        if ($soEntries.Count -eq 0) { throw 'No lib/arm64-v8a/*.so inside the universal APK.' }
        foreach ($entry in $soEntries) {
            $tempSo = Join-Path $env:TEMP ('pg_pagecheck_' + $entry.Name)
            [System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $tempSo, $true)
            $loads = @(Invoke-Native $readelf @('-l', $tempSo) | Where-Object { $_ -match '^\s*LOAD\s' })
            Remove-Item -LiteralPath $tempSo -Force
            if ($loads.Count -eq 0) { throw "llvm-readelf found no LOAD segment in $($entry.Name)" }
            foreach ($load in $loads) {
                $align = [Convert]::ToInt64(($load -split '\s+')[-1], 16)
                if ($align -lt 0x4000) { throw "$($entry.Name) has a LOAD segment aligned to 0x$($align.ToString('x')) (< 0x4000, breaks 16KB page devices)." }
            }
            Write-Host "   16KB: $($entry.Name) LOAD x$($loads.Count) aligned >= 0x4000"
        }
    }
    finally {
        $zip.Dispose()
    }
    Invoke-Native $zipalign @('-c', '-P', '16', '-v', '4', $universalApk.FullName) | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'zipalign -P 16 verification failed.' }

    # 4) 심볼: Play Console 'Native debug symbols'는 <abi>/libUnreal.so 구조의 zip을 받는다.
    $symbolsSo = Get-ChildItem -LiteralPath $releaseDir -Recurse -Filter 'libUnreal.so' | Where-Object { $_.FullName -like '*_Symbols_v*' } | Select-Object -First 1
    $symbolsZip = $null
    if ($symbolsSo) {
        $symbolsStage = Join-Path $env:TEMP "pg_symbols_v$StoreVersion"
        if (Test-Path -LiteralPath $symbolsStage) { Remove-Item -LiteralPath $symbolsStage -Recurse -Force }
        New-Item -ItemType Directory -Force -Path (Join-Path $symbolsStage 'arm64-v8a') | Out-Null
        Copy-Item -LiteralPath $symbolsSo.FullName -Destination (Join-Path $symbolsStage 'arm64-v8a\libUnreal.so')
        $symbolsZip = Join-Path $releaseDir "symbols-v$StoreVersion.zip"
        [System.IO.Compression.ZipFile]::CreateFromDirectory($symbolsStage, $symbolsZip, [System.IO.Compression.CompressionLevel]::Optimal, $false)
        Remove-Item -LiteralPath $symbolsStage -Recurse -Force
        Write-Host ("   symbols: {0} ({1:n0} bytes)" -f $symbolsZip, (Get-Item $symbolsZip).Length)
    }
    else {
        Write-Warning 'No *_Symbols_v*/libUnreal.so was archived; crash stacks from Play Console will not be symbolized.'
    }

    # 5) build-info: 어떤 커밋·툴체인으로 만든 산출물인지 Play Console 릴리스 노트와 롤백에 쓴다.
    $engineVersion = (Get-Content -LiteralPath (Join-Path $EngineRoot 'Engine\Build\Build.version') -Raw | ConvertFrom-Json)
    $javaVersion = (Invoke-Native (Join-Path $env:JAVA_HOME 'bin\java.exe') @('-version') | Select-Object -First 1)
    $buildInfo = @(
        "builtAt=$((Get-Date).ToString('yyyy-MM-dd HH:mm:ss zzz'))",
        "commit=$gitCommit",
        "branch=$gitBranch",
        "dirty=$gitDirty",
        "storeVersion=$StoreVersion",
        "versionName=$versionNameActual",
        "packageName=$packageName",
        "targetSdk=$targetSdk",
        "signer=$($signerDn.Trim())",
        "engine=$($engineVersion.MajorVersion).$($engineVersion.MinorVersion).$($engineVersion.PatchVersion) CL $($engineVersion.Changelist)",
        "ndk=$env:NDKROOT",
        "java=$env:JAVA_HOME ($javaVersion)",
        "buildTools=$($buildTools.Name)"
    )
    foreach ($artifact in @($aab, $universalApk) + @(if ($symbolsZip) { Get-Item $symbolsZip })) {
        $hash = (Get-FileHash -LiteralPath $artifact.FullName -Algorithm SHA256).Hash
        $buildInfo += "$($artifact.Name) bytes=$($artifact.Length) sha256=$hash"
        Write-Host ("   {0} ({1:n0} bytes)" -f $artifact.Name, $artifact.Length)
    }
    [System.IO.File]::WriteAllLines((Join-Path $releaseDir 'build-info.txt'), $buildInfo, (New-Object System.Text.UTF8Encoding $false))
    Write-Host "   Release folder: $releaseDir"
    if ($gitDirty) {
        Write-Warning 'This build came from a dirty working tree (-AllowDirty). Do not upload it to Google Play.'
    }
}

Write-Host ''
Write-Host 'All requested verification steps passed.' -ForegroundColor Green
