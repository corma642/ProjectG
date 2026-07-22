# Branding Resources

Android 앱 아이콘 원본은 이 폴더의 `AndroidAppIcon.png`로 관리합니다.

- 정사각형 PNG
- 최소 512x512
- 모서리를 미리 둥글게 만들지 않음
- 주요 피사체는 가장자리에서 약 10~15% 안쪽에 배치

원본을 추가하거나 교체한 뒤 프로젝트 루트에서 다음 명령을 실행합니다.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\GenerateAndroidIcons.ps1
```

스크립트는 Android 밀도별 런처 아이콘, 루트 프로젝트 이미지, Play Console용 512px 이미지를 생성합니다.
