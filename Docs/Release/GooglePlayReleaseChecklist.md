# Google Play 출시 보류 및 재개 체크리스트

> 상태: **정식 출시 보류**
> 기준일: 2026-07-22
> 현재 목적: Android 개발 및 실기기 검증용 패키지 생성

## 현재 패키징 기준

- [x] Unreal Engine 5.6.1 유지
- [x] Android Target SDK 35 적용
- [x] arm64 전용 AAB 및 ABI split 유지
- [x] Shipping / Full Rebuild 유지
- [x] `ForDistribution=False`로 테스트 서명 패키징 복구
- [x] Upload Keystore와 Distribution Signing은 미설정 상태로 유지
- [x] Google Play Games, AdMob, IAP, GooglePAD 비활성화
- [x] 네 손가락 콘솔 호출 비활성화
- [x] Receipt Shop 공개 진입과 Shipping 테스트 영수증 지급 차단
- [x] Settings 개인정보처리방침 버튼과 프로젝트 HTTPS URL 설정 제거

현재 Target SDK 35는 **개발 패키징 기준**일 뿐, 향후 Google Play 출시 기준을 보장하지 않는다. 출시를 재개할 때 최신 Target API와 Unreal Engine 지원 범위를 먼저 다시 확인한다.

## 2026-07-22 검증 결과

- [x] `ProjectGEditor Win64 Development` 빌드 성공
- [x] `CompileAllBlueprints` 오류 0, 경고 0, 로드 실패 0
- [x] Android ASTC Shipping `BuildCookRun` 성공
- [x] Cook / Stage / Package / Archive 전체 경로 성공
- [x] `bundletool validate` 성공
- [x] 기존 `DistributionSigning settings are not all set` 오류 제거 확인
- [x] 테스트 AAB 생성
  - 경로: `Saved/PackageTests/Android_ASTC/ProjectG-Android-Shipping.aab`
  - 크기: 184,022,348 bytes
  - 서명: Gradle debug signing

이 AAB는 기술 검증용이며 Google Play 업로드본이 아니다.

## 알려진 경고와 미검증 항목

- [ ] AAB 크기가 약 184MB이므로 출시 재개 시 Play 다운로드 크기와 모듈별 용량을 다시 분석한다.
- [ ] Android Gradle Plugin 8.5.2의 compileSdk 35 지원 범위 경고를 출시 시점 기준으로 다시 확인한다.
- [ ] Java 8 source/target deprecation 경고는 현재 패키징을 막지 않지만, 엔진 또는 Android 도구 체인 변경 시 재검토한다.
- [ ] 이전 패키징에서 발생했던 `M_Rock` Android ShaderMap 경고는 이번 전체 Cook에서 재현되지 않았다. 실기기에서 기본 머티리얼로 대체되는지 시각 검수한다.
- [ ] Galaxy Note9에서 신규 설치, 첫 실행, 결과, Retry, Home 복귀를 3회 이상 반복한다.
- [ ] Android 최신 버전 기기에서 동일 흐름을 검증한다.
- [ ] Coastal/City 맵 이동과 앱 재실행 후 장착 맵 복구를 확인한다.
- [ ] 백그라운드 전환과 복귀, 강제 종료 후 UserData 복구를 확인한다.

## 출시 재개 순서

1. 재개 시점의 Google Play Target API, Families, Data Safety 정책을 다시 확인한다.
2. UE 5.6.1에서 요구 API와 Android 도구 체인을 안전하게 지원할 수 있는지 검증한다.
3. Upload Keystore를 생성하고 키 파일, alias, 암호를 오프라인에 백업한다.
4. Project Settings > Android > Distribution Signing을 설정한다.
5. `ForDistribution=True`로 복구한다.
6. 앱 내부 개인정보처리방침 링크 또는 텍스트 표시 방식을 다시 결정한다.
7. 공개 개인정보처리방침 URL과 지원 이메일을 준비한다.
8. Play Console의 Data Safety, 광고 여부, 대상 연령, IARC 항목을 작성한다.
9. Store Version을 증가시키고 Distribution Shipping AAB를 Full Rebuild한다.
10. `bundletool validate`, Release 서명, Target SDK, arm64, `android:debuggable` 미포함을 확인한다.
11. Manifest에서 BILLING, CHECK_LICENSE, Play Games, AdMob, OBB Downloader 실행 컴포넌트가 없는지 확인한다.
12. Google Play 내부 테스트에 업로드하고 Pre-launch Report와 기기별 다운로드 크기를 확인한다.
13. Note9 및 최신 Android 기기에서 반복 플레이를 검증한다.
14. 512x512 아이콘, 1024x500 Feature Graphic, 세로 스크린샷, 스토어 설명을 준비한다.
15. 캐릭터, 환경, 음악, 효과음, 폰트 등 외부 에셋 라이선스 증빙을 최종 확인한다.

## 보존 자료

- `Docs/Release/PrivacyPolicyDraft.ko.md`: 출시 재개 시 사용할 개인정보처리방침 초안
- `Docs/Release/ThirdPartyAssetRegister.md`: 외부 에셋 라이선스 점검표

개인정보처리방침 초안은 현재 앱 UI에 연결하지 않지만, Google Play 출시를 재개할 때 정책 검토 자료로 유지한다.
