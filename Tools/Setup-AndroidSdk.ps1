<#
.SYNOPSIS
    새 PC에서 UE 에디터의 Android SDK 설정을 채웁니다: SDK·NDK·JDK 경로를 찾아 엔진 전역 사용자 ini에 씁니다.

.DESCRIPTION
    Project Settings > Platforms > Android SDK 값은 프로젝트 Config가 아니라 엔진 전역 사용자 설정
    (%LOCALAPPDATA%\Unreal Engine\Engine\Config\UserEngine.ini)에 저장되어 Git으로 넘어오지 않습니다.
    이 스크립트는 그 파일의 [/Script/AndroidPlatformEditor.AndroidSDKSettings] 섹션에서 SDKPath·NDKPath·JavaPath만 갱신하고
    다른 섹션과 키는 그대로 둡니다. SDKAPILevel·NDKAPILevel은 없을 때만 프로젝트 기준값(latest, android-26)을 넣습니다.

    탐색 순서
      SDK : -SdkPath → ANDROID_HOME → ANDROID_SDK_ROOT → %LOCALAPPDATA%\Android\Sdk
      NDK : -NdkPath → <SDK>\ndk\<NdkVersion> → NDKROOT → <SDK>\ndk 중 가장 높은 버전(프로젝트 기준과 다르면 경고)
      JDK : -JavaPath → JAVA_HOME → C:\Program Files\Java\jdk-17* 중 가장 높은 버전 → Android Studio jbr(경고)

    에디터를 닫고 실행합니다. 에디터가 열린 채 설정을 저장하면 이 파일을 다시 덮어씁니다.
    끝나면 Git 제외 배포 서명 파일(Build\Android\upload.keystore, Config\UserEngine.ini의 KeyStorePassword)이 복원됐는지도 알려 줍니다.

.EXAMPLE
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\Setup-AndroidSdk.ps1
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\Setup-AndroidSdk.ps1 -DryRun
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\Setup-AndroidSdk.ps1 -JavaPath 'C:\Program Files\Java\jdk-17.0.18+8'
#>
[CmdletBinding()]
param(
    [string]$SdkPath,
    [string]$NdkPath,
    [string]$JavaPath,
    # 프로젝트 기준 NDK(r27c). 복구 가이드 1절과 같아야 한다.
    [string]$NdkVersion = '27.2.12479018',
    # 결과만 보여 주고 파일은 쓰지 않는다.
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$IniPath = Join-Path $env:LOCALAPPDATA 'Unreal Engine\Engine\Config\UserEngine.ini'
$Section = '[/Script/AndroidPlatformEditor.AndroidSDKSettings]'
$MinBuildTools = [version]'36.1.0'

function Find-ExistingPath {
    # 후보 중 Probe 파일·폴더가 있는 첫 경로를 돌려준다. 없으면 $null.
    param([string[]]$Candidates, [string]$Probe)
    foreach ($candidate in $Candidates) {
        if ([string]::IsNullOrWhiteSpace($candidate)) { continue }
        $full = [System.IO.Path]::GetFullPath($candidate.Trim('"'))
        if (Test-Path -LiteralPath (Join-Path $full $Probe)) { return $full }
    }
    return $null
}

function Get-HighestVersionDir {
    # 폴더 이름이 버전 형식인 하위 폴더 중 가장 높은 것. 없으면 $null.
    param([string]$Parent, [string]$Pattern = '*')
    if (-not (Test-Path -LiteralPath $Parent)) { return $null }
    $dirs = Get-ChildItem -LiteralPath $Parent -Directory -Filter $Pattern | ForEach-Object {
        $v = $null
        # "jdk-17.0.18+8", "27.2.12479018"처럼 앞뒤 군더더기를 떼고 숫자.숫자 부분만 버전으로 읽는다.
        if ([version]::TryParse(($_.Name -replace '^[^0-9]*', '' -replace '[^0-9.].*$', ''), [ref]$v)) { [pscustomobject]@{ Dir = $_; Version = $v } }
    }
    if (-not $dirs) { return $null }
    return ($dirs | Sort-Object Version -Descending | Select-Object -First 1).Dir.FullName
}

# ---------------------------------------------------------------- SDK
$sdk = Find-ExistingPath @($SdkPath, $env:ANDROID_HOME, $env:ANDROID_SDK_ROOT, (Join-Path $env:LOCALAPPDATA 'Android\Sdk')) 'platform-tools\adb.exe'
if (-not $sdk) {
    throw "Android SDK was not found (platform-tools\adb.exe). Run Engine\Extras\Android\SetupAndroid.bat or pass -SdkPath (recovery guide, section 3)."
}
$buildTools = Get-HighestVersionDir (Join-Path $sdk 'build-tools')
if (-not $buildTools -or [version](Split-Path -Leaf $buildTools) -lt $MinBuildTools) {
    Write-Warning "build-tools $MinBuildTools or newer was not found under $sdk\build-tools (release artifact checks use aapt2/apksigner/zipalign)."
}
$platforms = @(Get-ChildItem -LiteralPath (Join-Path $sdk 'platforms') -Directory -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name)
# UE의 SDKAPILevel=latest는 정수 이름(android-35)만 고르고 android-36.1 같은 이름은 건너뛴다. 2026-10-12 기준 빌드는 android-35로 컴파일됐다.
$integerLevels = @($platforms | ForEach-Object { if ($_ -match '^android-(\d+)$') { [int]$Matches[1] } })
if (-not ($integerLevels | Where-Object { $_ -ge 35 })) {
    Write-Warning "No SDK Platform android-35 or newer with an integer name was found (found: $($platforms -join ', ')). Install SDK Platform 35/36 from the Android Studio SDK Manager."
}

# ---------------------------------------------------------------- NDK
$ndk = Find-ExistingPath @($NdkPath, (Join-Path $sdk "ndk\$NdkVersion"), $env:NDKROOT) 'source.properties'
if (-not $ndk) {
    $ndk = Get-HighestVersionDir (Join-Path $sdk 'ndk')
}
if (-not $ndk) {
    throw "Android NDK was not found. Install NDK $NdkVersion from the Android Studio SDK Manager or pass -NdkPath."
}
$ndkRevision = (Select-String -LiteralPath (Join-Path $ndk 'source.properties') -Pattern '^Pkg\.Revision\s*=\s*(.+)$' | Select-Object -First 1).Matches[0].Groups[1].Value.Trim()
if ($ndkRevision -ne $NdkVersion) {
    Write-Warning "NDK $ndkRevision differs from the project baseline $NdkVersion (recovery guide, section 1)."
}

# ---------------------------------------------------------------- JDK
$jdkCandidates = @($JavaPath, $env:JAVA_HOME, (Get-HighestVersionDir 'C:\Program Files\Java' 'jdk-17*'))
$jdk = Find-ExistingPath $jdkCandidates 'bin\java.exe'
if (-not $jdk) {
    $jdk = Find-ExistingPath @('C:\Program Files\Android\Android Studio\jbr') 'bin\java.exe'
    if ($jdk) { Write-Warning "Using Android Studio jbr ($jdk). The project baseline is OpenJDK 17 (recovery guide, section 1)." }
}
if (-not $jdk) {
    throw "JDK 17 was not found. Install OpenJDK 17 or pass -JavaPath."
}
$releaseFile = Join-Path $jdk 'release'
if ((Test-Path -LiteralPath $releaseFile) -and -not (Select-String -LiteralPath $releaseFile -Pattern '^JAVA_VERSION="17\.' -Quiet)) {
    Write-Warning "JDK at $jdk is not version 17 (see its release file)."
}

# ---------------------------------------------------------------- ini 갱신: 해당 섹션의 세 키만 바꾸고 나머지는 보존
$values = [ordered]@{ SDKPath = $sdk; NDKPath = $ndk; JavaPath = $jdk }
$defaults = [ordered]@{ SDKAPILevel = 'latest'; NDKAPILevel = 'android-26' }
$lines = New-Object 'System.Collections.Generic.List[string]'
if (Test-Path -LiteralPath $IniPath) {
    foreach ($line in @(Get-Content -LiteralPath $IniPath -Encoding UTF8)) { $lines.Add($line) }
}
$start = -1
for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i].Trim() -eq $Section) { $start = $i; break }
}
if ($start -lt 0) {
    if ($lines.Count -gt 0 -and $lines[$lines.Count - 1].Trim() -ne '') { $lines.Add('') }
    $lines.Add($Section)
    $start = $lines.Count - 1
}
$end = $lines.Count
for ($i = $start + 1; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -match '^\s*\[') { $end = $i; break }
}
function Set-SectionKey {
    param([string]$Key, [string]$Value)
    $newLine = "$Key=$Value"
    for ($i = $start + 1; $i -lt $script:end; $i++) {
        if ($lines[$i] -match ('^\s*' + [regex]::Escape($Key) + '\s*=')) { $lines[$i] = $newLine; return }
    }
    # 섹션 끝에 빈 줄이 있으면 그 앞(마지막 키 바로 뒤)에 넣는다.
    $insertAt = $script:end
    while ($insertAt -gt $start + 1 -and $lines[$insertAt - 1].Trim() -eq '') { $insertAt-- }
    $lines.Insert($insertAt, $newLine)
    $script:end++
}
foreach ($key in $values.Keys) {
    Set-SectionKey $key ('(Path="{0}")' -f ($values[$key] -replace '\\', '/'))
}
foreach ($key in $defaults.Keys) {
    $exists = $false
    for ($i = $start + 1; $i -lt $end; $i++) { if ($lines[$i] -match ('^\s*' + $key + '\s*=')) { $exists = $true; break } }
    if (-not $exists) { Set-SectionKey $key $defaults[$key] }
}

Write-Host "Android SDK settings for Unreal Editor ($IniPath):"
for ($i = $start; $i -lt $end; $i++) { Write-Host "   $($lines[$i])" }
if ($DryRun) {
    Write-Host '   (dry run: file not written)'
}
else {
    if (Get-Process -Name 'UnrealEditor' -ErrorAction SilentlyContinue) {
        Write-Warning 'UnrealEditor is running. Close it and run this script again, or the editor may overwrite the file when it saves settings.'
    }
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $IniPath) | Out-Null
    # 기존 파일과 같은 UTF-8(BOM 없음)로 쓴다.
    [System.IO.File]::WriteAllLines($IniPath, [string[]]$lines, (New-Object System.Text.UTF8Encoding($false)))
    Write-Host '   written.'
}

# ---------------------------------------------------------------- 배포 서명 파일(Git 제외) 복원 여부
$keystore = Join-Path $ProjectRoot 'Build\Android\upload.keystore'
$userEngineIni = Join-Path $ProjectRoot 'Config\UserEngine.ini'
$hasPassword = (Test-Path -LiteralPath $userEngineIni) -and (Select-String -LiteralPath $userEngineIni -Pattern '^KeyStorePassword=.+' -Quiet)
Write-Host 'Release signing files (restore from the offline backup, recovery guide section 7):'
Write-Host ("   {0} Build\Android\upload.keystore" -f $(if (Test-Path -LiteralPath $keystore) { 'ok     ' } else { 'MISSING' }))
Write-Host ("   {0} Config\UserEngine.ini KeyStorePassword" -f $(if ($hasPassword) { 'ok     ' } else { 'MISSING' }))
