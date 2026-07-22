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

## 새 클론 복구 핵심

> [!NOTE]
> **Editor Preferences와 Android SDK 경로는 PC별 로컬 설정입니다.** 아래 워크플로우에 따라 필요한 값만 직접 확인하거나 복구합니다.

새 환경에서는 다음 순서만 따르면 됩니다.

1. UE 5.6.1과 C++ 빌드 환경 준비
2. Android SDK·NDK·JDK 연결 확인
3. 에디터 개인 설정 복구
4. 자동 복구된 Project Settings 확인
5. Editor 빌드와 Blueprint 검증
6. Android ASTC 패키징과 실기기 확인

## 프로젝트

러너는 자동으로 전진합니다. 플레이어는 좌우 중심을 보정하고 점프하며, 머리 위 적재물을 떨어뜨리지 않고 오래 배달해야 합니다. 난이도는 적재물 누적과 바람·눈바람·이동형 위험 구역 RunEvent를 통해 증가합니다.

| 환경 | 조작 |
|:---|:---|
| Android | 좌우 드래그로 중심 보정, 좌·우 플릭으로 방향 점프, 위 플릭으로 제자리 점프 |
| Desktop 테스트 | `Q/E` 중심 보정, `Space` 점프 |

## 1. 필수 환경 준비

| 항목 | 프로젝트 기준 |
|:---|:---|
| Unreal Engine | `5.6.1` |
| C++ 빌드 | Visual Studio 2022 C++ 빌드 환경 |
| Android 도구 | Android Studio, SDK Platform `35` |
| Android NDK | r27c `27.2.12479018` |
| Java | JBR 또는 OpenJDK `21` |
| 대상 기기 | Android 8 이상, arm64 |

Visual Studio 워크로드와 컴파일러 구성은 각 개발 환경에 맞게 준비합니다. Android Studio와 Build Tools 버전은 고정하지 않으며 SDK 35, NDK r27c, JDK 21 조합을 프로젝트 기준으로 사용합니다.

Android 도구를 처음 구성한다면 에디터와 Epic Games Launcher를 종료한 뒤 UE 설치 폴더의 `Engine/Extras/Android/SetupAndroid.bat`을 실행합니다. 이후 Android Studio SDK Manager에서 SDK Platform 35와 NDK `27.2.12479018` 설치 여부를 확인합니다.

**위치**

Windows 탐색기의 `ProjectG.uproject`와 프로젝트 루트

**할 일**

1. `ProjectG.uproject`를 UE 5.6에 연결합니다.
2. Visual Studio 프로젝트 파일을 생성합니다.
3. 프로젝트가 열리면 최초 Shader와 Derived Data 생성이 끝날 때까지 기다립니다.

> [!TIP]
> **완료 기준:** `ProjectG.uproject`가 UE 5.6.1로 열리고 `ProjectGEditor Win64 Development` 타깃을 빌드할 수 있어야 합니다.

- [Epic Games: Android 개발 요구사항](https://dev.epicgames.com/documentation/en-us/unreal-engine/android-development-requirements-for-unreal-engine?application_version=5.6)
- [Epic Games: Android SDK/NDK 설정](https://dev.epicgames.com/documentation/en-us/unreal-engine/how-to-set-up-android-sdk-and-ndk-for-your-unreal-engine-development-environment?application_version=5.6)

## 2. Android 도구 연결

**위치**

`Edit > Project Settings > Platforms > Android SDK`

**할 일**

1. SDK, NDK, JDK가 자동으로 인식되는지 확인합니다.
2. 비어 있거나 잘못된 항목만 현재 PC의 실제 설치 경로로 지정합니다.
3. Android SDK 라이선스 승인 버튼이 활성화되어 있다면 승인합니다.

| 도구 | 일반적인 경로 |
|:---|:---|
| SDK | `%LOCALAPPDATA%\Android\Sdk` |
| NDK | `%LOCALAPPDATA%\Android\Sdk\ndk\27.2.12479018` |
| JDK | Android Studio 설치 폴더의 `jbr` |

로컬 절대 경로는 PC마다 다르므로 Git에 커밋하지 않습니다.

> [!TIP]
> **완료 기준:** SDK Platform 35와 NDK r27c가 인식되고, 에디터의 Android ASTC 패키징 메뉴가 활성화되어야 합니다.

## 3. 에디터 개인 설정 복구

에디터 개인 설정은 `Saved/Config/WindowsEditor`에 저장되며 Git에 포함되지 않습니다. 프로젝트 동작에 필요한 설정은 아니지만, 기존 작업 환경과 같은 방식으로 테스트하려면 아래 값을 직접 복구합니다.

**위치**

에디터 상단 Play 버튼 드롭다운과 `Edit > Editor Preferences`

**할 일**

아래 복구 권장값을 먼저 적용하고, 선택값은 기존 로컬 작업 환경을 그대로 재현할 때만 적용합니다.

### 복구 권장

**Play 버튼 드롭다운**

- 실행 모드: `Mobile Preview`

**위치: `Edit > Editor Preferences > Level Editor > Play`**

| 설정 | 값 |
|:---|:---|
| New Window Width | `540` |
| New Window Height | `1170` |
| Play Net Mode | `Standalone` |
| Run Under One Process | Enabled |
| Number of Clients | `1` |

> [!TIP]
> **완료 기준:** Play 버튼으로 실행했을 때 `540×1170` Mobile Preview가 열리고 단일 Standalone 클라이언트로 실행되어야 합니다.

### 현재 로컬 환경 재현용 선택값

Editor Preferences 검색창에서 설정 이름을 검색하는 것이 가장 빠릅니다.

| 설정 | 값 | 의미 |
|:---|:---|:---|
| Confirm Editor Close | Disabled | 에디터 종료 확인창을 표시하지 않음 |
| Always Build UAT | Enabled | 패키징 시 UAT를 항상 빌드 |
| Save on Compile | `Never` | Blueprint 컴파일 시 자동 저장하지 않음 |
| Index All Blueprints | `Load Only` | 로드된 Blueprint 중심으로 인덱싱 |

창 배치, 최근 에셋, Content Browser 경로, 뷰포트 카메라와 콘솔 기록은 복구하지 않습니다.

## 4. 자동 복구된 Project Settings 확인

이 단계는 설정값을 다시 입력하는 절차가 아닙니다. Git에서 내려온 Config가 에디터에 정상 반영됐는지만 확인합니다.

**위치**

`Edit > Project Settings`

**할 일**

아래 기대값과 에디터 표시값을 비교합니다.

| 위치 | 확인 항목 | 기대값 |
|:---|:---|:---|
| `Project > Maps & Modes` | Game Default Map | `/Game/Level/Coastal/LV_Coastal` |
| `Project > Maps & Modes` | Editor Startup Map | `/Game/Level/Coastal/LV_Coastal` |
| `Platforms > Android` | Package Name | `com.junhyeong.projectg` |
| `Platforms > Android` | Orientation | Portrait |
| `Platforms > Android` | Minimum / Target SDK | `26 / 35` |
| `Platforms > Android` | Architecture | arm64 only |
| `Platforms > Android` | Bundle / Universal APK | Enabled / Enabled |
| `Platforms > Android` | Package game data inside APK | Enabled |
| `Platforms > Android` | Google Play / AdMob | Disabled / Disabled |
| `Project > Packaging` | Build Configuration | Shipping |
| `Project > Packaging` | Full Rebuild | Enabled |
| `Project > Packaging` | For Distribution | Disabled |
| `Project > Packaging` | Maps to Cook | Coastal, City |

Input Mapping, DeviceProfile, Go Deliver! Settings, Blueprint, DataAsset와 레벨 배치는 저장소의 Config와 Content에서 자동 복구됩니다.

> [!TIP]
> **완료 기준:** 표의 값이 모두 일치하고 Coastal 레벨과 프로젝트 콘텐츠가 정상적으로 열려야 합니다.

> [!WARNING]
> 기대값과 다르다면 Project Settings에서 바로 수정하지 않습니다. 먼저 `git status`와 `Config/Default*.ini` 누락 여부를 확인해야 다른 환경에서도 같은 설정을 유지할 수 있습니다.

## 5. Android 패키징

**위치**

`Platforms > Android > Package Project > Android (ASTC)`

**할 일**

1. 4단계의 자동 복구 설정이 일치하는지 확인합니다.
2. 텍스처 포맷으로 `Android (ASTC)`를 선택합니다.
3. 패키징 출력 폴더를 선택합니다. 이 경로는 로컬 설정이므로 새 환경에서 다시 선택합니다.
4. 패키징 로그가 끝날 때까지 기다립니다.
5. 출력 폴더에서 실제 게임 APK 또는 AAB를 확인합니다.

> [!NOTE]
> UE 5.6.1의 Android Gradle Plugin은 compileSdk 35 지원 경고를 출력할 수 있습니다. 현재 프로젝트는 해당 경고가 있는 상태에서 Target SDK 35 Android ASTC 패키징 성공을 확인했습니다.

### 산출물 구분

| 파일 | 용도 |
|:---|:---|
| `ProjectG-Android-Shipping.aab` | Google Play용 App Bundle. 현재 테스트 서명본은 제출용이 아님 |
| `ProjectG-Android-Shipping*.apk` | 실기기에 직접 설치하는 게임 APK |
| `AFS_ProjectG-*.apk` | Unreal Android File Server 보조 앱. 게임 본체가 아님 |
| `ProjectG_Symbols_*` | 네이티브 크래시 분석 심볼 |

> [!TIP]
> **완료 기준:** UAT가 ExitCode 0으로 종료되고, `AFS_*.apk`가 아닌 실제 게임 APK를 설치해 앱 아이콘, 첫 실행, 맵 전환과 재실행을 확인해야 합니다.

## 자주 막히는 지점

- **Android 패키징 메뉴가 비활성화됨:** Android SDK 페이지에서 SDK, NDK, JDK 인식을 다시 확인합니다.
- **Project Settings 값이 다름:** 값을 덮어쓰기 전에 Config 파일과 Git 상태를 확인합니다.
- **설치한 APK가 게임이 아님:** `AFS_*.apk`가 아니라 `ProjectG-Android-Shipping*.apk`를 설치합니다.

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
