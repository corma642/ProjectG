# Branding Resources

Android 브랜딩 이미지의 원본은 이 폴더에서 관리합니다.

- `AndroidAppIcon.png`: 런처 아이콘 원본
  - 정사각형 PNG, 최소 512x512
  - 모서리를 미리 둥글게 만들지 않음
  - 주요 피사체는 가장자리에서 약 10~15% 안쪽에 배치(어댑티브 아이콘은 가운데 72dp만 보이고 원형 마스크가 모서리를 잘라냄)
- `Logo.png`: 런치 이미지(앱 실행 직후 첫 프레임까지 보이는 화면)에 넣을 로고. Home 화면 로고 텍스처 `T_Logo`를 PNG로 내보낸 것과 같은 그림이어야 Home·로딩 화면과 이어집니다.

원본을 추가하거나 교체한 뒤 프로젝트 루트에서 다음 명령을 실행합니다.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\GenerateAndroidIcons.ps1
```

스크립트가 만드는 것:

- 밀도별 런처 아이콘 `Build/Android/res/drawable-*/icon.png`와 루트 `ProjectG.png`
- 어댑티브 아이콘 층 `Build/Android/res/drawable-*/icon_foreground.png`(전경: 원본을 72dp로 가운데 배치)·`icon_background.png`(배경: 원본을 뭉갠 색 바탕). Android 8+는 `Build/Android/res/drawable-anydpi-v26/icon.xml`(저장소에 있는 고정 파일)을 통해 이 두 층을 씁니다.
- 런치 이미지 `Build/Android/res/drawable/splashscreen_portrait.png`(1440x3120, Home 하늘색 배경 + 로고. 엔진이 화면에 맞춰 가운데를 잘라 보여 줌)

아이콘을 바꾼 뒤에는 `Intermediate/Android`를 지우고 다시 패키징하며, 실기기에서는 기존 앱을 삭제하고 재설치해 런처 캐시를 배제합니다.
