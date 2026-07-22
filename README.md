# Go Deliver!

![Go Deliver!](ProjectG.png)

머리 위에 쌓이는 배달 적재물의 균형을 지키며 달리는 Android용 3D 무한 러너입니다.

- 엔진: Unreal Engine 5.6.1
- 주 플랫폼: Android
- 개발 환경: Windows, Visual Studio 2022
- 모바일 조작: 좌우 드래그로 중심 보정, 좌/우 플릭으로 방향 점프, 위 플릭으로 제자리 점프
- 데스크톱 테스트 조작: `Q/E` 중심 보정, `Space` 점프

## 저장소 복구 원칙

Unreal의 모든 설정이 로컬에만 저장되는 것은 아닙니다. Project Settings에서 변경한 대부분의 값은 `Config/Default*.ini`, `ProjectG.uproject`, `Content`에 기록되며 Git에 커밋되어 있으면 새 클론에서도 자동으로 복구됩니다.

| 구분 | 대표 항목 | Git 복구 |
|:---|:---|:---:|
| 프로젝트 설정 | 렌더링, 시작 맵, Android, 패키징, 입력, Device Profile | 자동 |
| 에셋과 레벨 설정 | Blueprint, DataAsset, World Settings, 레벨 배치 | 자동 |
| C++ 코드 | 게임 로직, 빌드 타깃 | 자동 |
| 에디터 개인 설정 | 플레이 모드, 미리보기 크기, 창 배치, 최근 경로 | 수동 |
| 로컬 패키징 상태 | 출력 폴더, SDK 절대 경로, 서명 정보 | 수동 |
| 로컬 사용자 데이터 | SaveGame, 콘솔 기록, 크래시 로그 | 복구 대상 아님 |

`Saved`, `Intermediate`, `DerivedDataCache`, `Binaries`, 패키징 산출물은 다시 생성되는 로컬 데이터이므로 저장소에 포함하지 않습니다.

> 로컬 프로젝트를 삭제하기 전에는 반드시 `git status`를 확인하십시오. Project Settings 또는 에셋 변경이 커밋되지 않았다면 새 클론에서 복구되지 않습니다.

## 검증된 프로젝트 설정

아래 값은 현재 저장소의 Config에 기록되어 있으므로 새로 입력하지 않아도 됩니다. 최초 실행 후 값이 일치하는지만 확인합니다.

### Maps & Modes

| 항목 | 값 |
|:---|:---|
| Game Default Map | `/Game/Level/Coastal/LV_Coastal` |
| Editor Startup Map | `/Game/Level/Coastal/LV_Coastal` |

### Android

| 항목 | 값 |
|:---|:---|
| Package Name | `com.junhyeong.projectg` |
| Orientation | Portrait |
| Minimum SDK | 26 |
| Target SDK | 35 |
| Architecture | arm64 |
| Package game data inside APK | Enabled |
| Generate Bundle (AAB) | Enabled |
| Generate Universal APK from Bundle | Enabled |
| ABI / Language / Density Split | Enabled |
| Google Play Support / AdMob | Disabled |

Target SDK 35는 현재 개발 패키징 기준입니다. 정식 출시를 재개할 때는 Google Play의 최신 Target API 정책과 UE 5.6.1 지원 여부를 다시 확인해야 합니다.

### Packaging

| 항목 | 값 |
|:---|:---|
| Build Configuration | Shipping |
| Full Rebuild | Enabled |
| For Distribution | Disabled |
| Package Format | Pak + IoStore, Oodle |
| Maps to Cook | Coastal, City |

현재 설정은 실기기 검증용입니다. Google Play 제출 전에는 Upload Keystore와 Distribution Signing을 설정하고 `For Distribution`을 다시 활성화해야 합니다.

## 새 클론 초기 구성

### 1. 필수 도구 설치

1. Epic Games Launcher에서 Unreal Engine `5.6.1`을 설치합니다.
2. Visual Studio Installer에서 저장소의 `.vsconfig`를 가져와 필요한 C++ 및 Unreal 구성 요소를 설치합니다.
3. Android Studio를 설치합니다.
4. Unreal의 `Engine/Extras/Android/SetupAndroid.bat`을 실행합니다.

현재 검증된 Android 툴체인은 다음과 같습니다.

| 도구 | 검증 값 |
|:---|:---|
| Android SDK Platform | 35 |
| Android NDK | `27.2.12479018` |
| Java | Android Studio JBR 21 |

Android 툴체인 경로는 PC마다 다르므로 저장소에 기록하지 않습니다. Unreal의 Android SDK 설정이나 환경 변수에는 각 PC의 실제 설치 경로를 지정합니다.

### 2. 저장소 복제

```powershell
git clone https://github.com/corma642/ProjectG.git
Set-Location ProjectG
```

`ProjectG.uproject`를 우클릭해 다음을 실행합니다.

1. `Switch Unreal Engine version...`에서 UE 5.6 선택
2. `Generate Visual Studio project files`

`ProjectG.sln`이 이미 있더라도 엔진 경로나 Visual Studio 구성이 달라졌다면 다시 생성하는 편이 안전합니다.

### 3. Editor 빌드

프로젝트 루트의 PowerShell에서 실행합니다.

```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" `
  ProjectGEditor Win64 Development `
  "$PWD\ProjectG.uproject" `
  -NoHotReloadFromIDE
```

빌드가 끝나면 `ProjectG.uproject`를 열고 Shader와 Derived Data 생성이 완료될 때까지 기다립니다. 최초 실행은 캐시가 없어 오래 걸릴 수 있습니다.

## 에디터 개인 설정 복구

개인 설정은 `Saved/Config/WindowsEditor`에 생성되며 Git에 포함되지 않습니다. 새 클론에서는 아래 항목만 수동으로 복구합니다.

### 복구 권장

| 위치 | 설정 | 값 |
|:---|:---|:---|
| Play 버튼 드롭다운 | 실행 모드 | Mobile Preview |
| Editor Preferences > Level Editor > Play | New Window Width | `540` |
| Editor Preferences > Level Editor > Play | New Window Height | `1170` |
| Editor Preferences > Level Editor > Play | Play Net Mode | Standalone |
| Editor Preferences > Level Editor > Play | Number of Clients | `1` |

모바일 프리뷰가 필요하지 않은 C++ 디버깅에서는 Selected Viewport 또는 Standalone을 사용해도 됩니다.

### 선택 사항

아래 값은 현재 로컬에서 확인됐지만 프로젝트 동작을 결정하지 않으므로 필요할 때만 복구합니다.

| 위치 | 현재 값 |
|:---|:---|
| Editor Preferences > General | Always Build UAT: Enabled |
| Editor Preferences > General | Confirm Editor Close: Disabled |
| Editor Preferences > Blueprint Editor | Save on Compile: Never |
| Editor Preferences > Blueprint Editor | Index All Blueprints: Load Only |

### 복구하지 않는 항목

- 에디터 창과 탭 배치
- 레벨별 뷰포트 카메라 위치
- 최근 열었던 에셋과 Content Browser 경로
- PC 이름과 실행 장치 기록
- 콘솔 명령 기록
- 임시 Import/Export 경로

이 값들은 작업 과정에서 자동으로 다시 생성되며, 다른 PC로 복사하면 경로 충돌이나 불필요한 diff를 만들 수 있습니다.

## Blueprint 검증

에디터에서 `Tools > Compile All Blueprints`를 실행하거나 명령줄에서 다음을 실행합니다.

```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "$PWD\ProjectG.uproject" `
  -run=CompileAllBlueprints `
  -unattended `
  -nop4 `
  -nosplash
```

완료 기준은 Blueprint 오류, 경고, 로드 실패가 모두 0인 상태입니다.

## Android 패키징

### 패키징 전 확인

1. `Project Settings > Platforms > Android`에서 SDK 상태가 정상인지 확인합니다.
2. `Project Settings > Packaging`에서 Shipping, Full Rebuild, `For Distribution=False`를 확인합니다.
3. Editor 메뉴에서 `Platforms > Android > Package Project > Android (ASTC)`를 선택합니다.
4. 패키징 출력 폴더는 각 PC에서 원하는 로컬 경로를 선택합니다.

### 산출물 구분

| 산출물 | 용도 |
|:---|:---|
| `ProjectG-Android-Shipping.aab` | Google Play App Bundle. 현재 테스트 서명본은 정식 제출 불가 |
| Universal 또는 일반 `ProjectG-Android-Shipping-*.apk` | 실기기 직접 설치 |
| `AFS_ProjectG-*.apk` | Unreal Android File Server 보조 앱. 게임 본체가 아님 |
| `ProjectG_Symbols_*` | 네이티브 크래시 분석 심볼 |

APK 하나로 테스트하려면 다음 중 하나를 사용합니다.

- 현재처럼 AAB와 `Generate Universal APK from Bundle`을 함께 활성화
- `Generate Bundle (AAB)`을 끄고 독립 APK로 다시 패키징

`Package game data inside .apk`가 활성화되어 있으므로 독립 APK에는 별도 OBB가 필요하지 않습니다. 대신 파일 크기가 커질 수 있습니다.

## 최초 복구 확인 체크리스트

- [ ] UE 5.6.1로 프로젝트가 열림
- [ ] `ProjectGEditor Win64 Development` 빌드 성공
- [ ] Coastal이 Editor/Game 기본 맵으로 열림
- [ ] Mobile Preview가 `540×1170`으로 실행됨
- [ ] Blueprint 전체 컴파일 오류·경고 0
- [ ] Home, Shop, Start, Retry 흐름 정상
- [ ] Android SDK/NDK/JDK 인식 정상
- [ ] Android ASTC 패키징 성공
- [ ] Universal/독립 APK를 실기기에 설치 가능

## 로컬 데이터와 보안

다음 항목은 Git에 커밋하지 않습니다.

- `Saved`, `Intermediate`, `DerivedDataCache`, `Binaries`
- 패키징 산출물과 로컬 Build 폴더
- Upload Keystore와 서명 암호
- Android SDK/NDK/JDK 절대 경로
- 로컬 SaveGame과 크래시 로그

정식 Google Play 출시 재개 절차는 [Google Play 출시 체크리스트](Docs/Release/GooglePlayReleaseChecklist.md)를 참고하십시오.
