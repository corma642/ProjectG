<p align="center">
  <img src="ProjectG.png" width="160" alt="Go Deliver! 앱 아이콘">
</p>

<h1 align="center">Go Deliver!</h1>

<p align="center">
  머리 위에 쌓이는 배달 적재물의 균형을 지키며 달리는 Android 3D 무한 러너
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Unreal_Engine-5.6.1-0E1128?logo=unrealengine" alt="Unreal Engine 5.6.1">
  <img src="https://img.shields.io/badge/Platform-Android-3DDC84?logo=android&logoColor=white" alt="Android">
  <img src="https://img.shields.io/badge/Code-C%2B%2B_%2B_Blueprint-00599C?logo=cplusplus&logoColor=white" alt="C++ and Blueprint">
</p>

## 프로젝트

러너는 자동으로 전진합니다. 플레이어는 좌우 중심을 보정하고 점프하며, 머리 위 적재물을 떨어뜨리지 않고 오래 배달해야 합니다. 난이도는 적재물 누적과 바람·눈바람·이동형 위험 구역 RunEvent를 통해 증가합니다.

| 환경 | 조작 |
|:---|:---|
| Android | 좌우 드래그로 중심 보정, 좌·우 플릭으로 방향 점프, 위 플릭으로 제자리 점프 |
| Desktop 테스트 | `Q/E` 중심 보정, `Space` 점프 |

## 필수 환경

| 항목 | 프로젝트 기준 |
|:---|:---|
| Unreal Engine | `5.6.1` |
| C++ 빌드 | Visual Studio 2022 C++ 빌드 환경 |
| Android 도구 | Android Studio, SDK Platform `35` |
| Android NDK | r27c `27.2.12479018` |
| Java | JBR 또는 OpenJDK `21` |
| 대상 기기 | Android 8 이상, arm64 |

Visual Studio 워크로드와 컴파일러 구성은 각 개발 환경에 맞게 준비합니다. Android Studio와 Build Tools 버전은 고정하지 않으며, SDK 35·NDK r27c·JDK 21 조합을 프로젝트 기준으로 사용합니다.

Android 도구를 처음 구성한다면 에디터와 Epic Games Launcher를 종료한 뒤 UE 설치 폴더의 `Engine/Extras/Android/SetupAndroid.bat`을 실행합니다. 이후 Android Studio SDK Manager에서 SDK Platform 35와 NDK `27.2.12479018` 설치 여부를 확인합니다.

- [Epic Games: Android 개발 요구사항](https://dev.epicgames.com/documentation/en-us/unreal-engine/android-development-requirements-for-unreal-engine?application_version=5.6)
- [Epic Games: Android SDK/NDK 설정](https://dev.epicgames.com/documentation/en-us/unreal-engine/how-to-set-up-android-sdk-and-ndk-for-your-unreal-engine-development-environment?application_version=5.6)

## 클론 후 설정

Project Settings와 콘텐츠 설정은 Git에 포함되어 있습니다. 새 클론에서 다시 입력할 항목과 로컬에서 확인할 항목은 다음과 같습니다.

| Git에서 자동 복구 | 로컬에서 확인 |
|:---|:---|
| Coastal 시작 맵과 판매 맵 콘텐츠 | UE 5.6.1 엔진 연결 |
| Portrait, arm64, Min SDK 26, Target SDK 35 | Android SDK/NDK/JDK 인식 상태 |
| Shipping, Full Rebuild, AAB·Universal APK | 패키징 출력 폴더 |
| Coastal·City Cook 목록 | 선택 사항: Mobile Preview `540×1170` |
| Android 밀도별 런처 아이콘 | 실기기 USB 디버깅 및 연결 |

### 1. 엔진 연결

`ProjectG.uproject`를 UE 5.6에 연결하고 Visual Studio 프로젝트 파일을 생성합니다. 엔진 경로나 C++ 도구 구성이 바뀐 환경에서는 기존 솔루션보다 새로 생성한 프로젝트 파일을 사용합니다.

### 2. Editor 빌드

프로젝트 루트 PowerShell에서 실행합니다.

```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" `
  ProjectGEditor Win64 Development `
  "$PWD\ProjectG.uproject" `
  -NoHotReloadFromIDE
```

### 3. Android 도구 확인

`Project Settings > Platforms > Android SDK`에서 SDK, NDK, JDK가 정상 인식되는지 확인합니다. 자동 탐지가 실패한 경우에만 현재 PC의 실제 설치 경로를 지정합니다. 로컬 절대 경로는 Git에 커밋하지 않습니다.

### 4. Blueprint 검증

에디터의 `Tools > Compile All Blueprints`를 실행하거나 다음 명령을 사용합니다.

```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "$PWD\ProjectG.uproject" `
  -run=CompileAllBlueprints `
  -unattended -nop4 -nosplash
```

완료 기준은 오류, 경고, 로드 실패 Blueprint가 모두 0인 상태입니다.

### 5. Android 테스트 패키징

아래 Android 패키징 설정을 확인한 뒤 `Platforms > Android > Package Project > Android (ASTC)`를 실행합니다.

## Android 패키징

현재 Config에 저장된 테스트 패키징 기준입니다. 새 클론에서 값을 다시 입력하지 않고, 패키징 전에 일치 여부만 확인합니다.

| 항목 | 값 |
|:---|:---|
| Build Configuration | Shipping |
| Full Rebuild | Enabled |
| For Distribution | Disabled |
| Texture Format | ASTC |
| Architecture | arm64 |
| Orientation | Portrait |
| Min / Target SDK | 26 / 35 |
| Bundle / Universal APK | Enabled / Enabled |
| Game data inside APK | Enabled |
| Maps to Cook | Coastal, City |

> UE 5.6.1의 Android Gradle Plugin은 compileSdk 35에 대한 지원 경고를 출력할 수 있습니다. 현재 프로젝트는 해당 경고가 있는 상태에서 Target SDK 35 Android ASTC 패키징 성공을 확인했습니다.

### 산출물

| 파일 | 용도 |
|:---|:---|
| `ProjectG-Android-Shipping.aab` | Google Play용 App Bundle. 현재 테스트 서명본은 제출용이 아님 |
| `ProjectG-Android-Shipping*.apk` | 실기기에 직접 설치하는 게임 APK |
| `AFS_ProjectG-*.apk` | Unreal Android File Server 보조 앱. 게임 본체가 아님 |
| `ProjectG_Symbols_*` | 네이티브 크래시 분석 심볼 |

실기기 테스트에는 `AFS_*.apk`가 아닌 Universal 또는 독립 게임 APK를 사용합니다. 독립 APK에는 게임 데이터가 포함되므로 별도 OBB가 필요하지 않습니다.

## 검증

- Editor 빌드 성공
- Blueprint 컴파일 오류·경고·로드 실패 0
- Coastal이 기본 맵으로 열리고 Home → Shop → Start → Retry 흐름 정상
- Android ASTC 패키징 성공
- 실제 게임 APK 설치 및 첫 실행 성공
- City 전환, 앱 재실행, UserData 유지 확인

## 유지보수 참고

<details>
<summary><strong>Android 앱 아이콘 교체</strong></summary>

1. 정사각형 512×512 이상 PNG를 `Resources/Branding/AndroidAppIcon.png`에 배치합니다.
2. 프로젝트 루트에서 다음 스크립트를 실행합니다.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\GenerateAndroidIcons.ps1
```

스크립트는 Android 밀도별 `icon.png`, 루트 `ProjectG.png`, Play Console용 `GooglePlayIcon512.png`를 생성합니다. 변경 후 `Intermediate/Android`만 삭제하고 Full Rebuild로 패키징하며, 기존 앱을 제거한 뒤 다시 설치해 런처 캐시 영향을 배제합니다.

</details>

<details>
<summary><strong>Google Play 출시 재개</strong></summary>

현재 정식 출시는 보류 상태이며 `For Distribution`은 비활성화되어 있습니다. 출시 재개 시 최신 Target API 및 Families 정책, Upload Keystore, Distribution Signing, Data Safety와 스토어 리소스를 다시 확인합니다.

상세 절차는 [Google Play 출시 체크리스트](Docs/Release/GooglePlayReleaseChecklist.md)를 참고합니다.

</details>

`Saved`, `Intermediate`, `DerivedDataCache`, `Binaries`, 패키징 산출물, SDK 절대 경로, 서명 키와 로컬 SaveGame은 Git에 포함하지 않습니다.
