# Google Play 전 연령 출시 체크리스트

## 코드와 패키징

- [x] UE 5.6.1 유지
- [x] Android Target SDK 35 명시
- [x] arm64 전용 AAB와 ABI split 설정
- [x] Shipping 및 For Distribution 설정
- [x] Google Play Games, AdMob, 테스트 IAP 비활성화
- [x] 네 손가락 콘솔 호출 비활성화
- [x] Receipt Shop 공개 진입 및 테스트 지급 차단
- [ ] Upload Keystore 생성 및 오프라인 백업
- [ ] Project Settings > Android > Distribution Signing에 로컬 서명 정보 입력
- [ ] `PrivacyPolicyURL`에 공개 HTTPS 주소 입력
- [ ] `WBP_HomeSettingsScreen`에 `PrivacyPolicyButton` 추가

## 최종 바이너리 게이트

- [ ] Android Shipping AAB Full Rebuild
- [x] 기술 검증용 Android Shipping AAB 생성 및 `bundletool validate` 성공
- [ ] Target SDK 35, arm64, Release 서명 확인
- [ ] `android:debuggable=true`가 없는지 확인
- [ ] BILLING, CHECK_LICENSE, AdMob, Play Games, OBB Downloader가 Manifest에 없는지 확인
- [ ] Play Console 내부 테스트 업로드 성공
- [ ] Play가 표시한 기기별 다운로드 크기가 허용 범위인지 확인

현재 기술 검증용 AAB는 Target SDK 35, arm64, 약 51.1MB이며 금지 Manifest 항목이 제거된 것을 확인했습니다. 다만 `Build.bat` 직접 산출물은 debug signing이므로 Google Play 업로드 대상이 아닙니다. Upload Keystore를 설정한 뒤 Editor 또는 UAT의 Distribution 경로로 최종 AAB를 다시 만들어 위 미완료 항목을 검증합니다.

## 실기기 검증

- [ ] Galaxy Note9에서 신규 설치 후 Home 진입
- [ ] 첫 플레이, 결과, Retry, Home 복귀를 3회 이상 반복
- [ ] Coastal/City 이동과 앱 재실행 후 장착 맵 유지
- [ ] 백그라운드 전환과 복귀
- [ ] 앱 강제 종료 후 UserData 복구
- [ ] Android 15 이상 기기에서 동일 흐름 검증
- [ ] Play Pre-launch Report의 크래시, ANR, 호환성 확인

## 전 연령 및 스토어 정보

- [ ] Target audience에 실제 대상 연령을 정확히 선택하고 Families 정책 확인
- [ ] Data Safety를 최종 바이너리 기준으로 작성
- [ ] 광고 없음, 결제 없음, 계정 없음 선언
- [ ] IARC 콘텐츠 등급 설문 작성
- [ ] 공개 개인정보처리방침 URL과 지원 이메일 등록
- [ ] 512x512 앱 아이콘
- [ ] 1024x500 Feature Graphic
- [ ] 세로 스크린샷 3장 이상
- [ ] 짧은 설명과 전체 설명 작성
- [ ] 외부 에셋 라이선스 증빙 완료

## 일정 게이트

- [ ] 2026-08-01 이전 비공개 테스트 시작
- [ ] 테스터 12명 이상이 14일 연속 참여
- [ ] 2026-08-20 전후 프로덕션 심사 제출
- [ ] 2026-08-30 이전 API 35 기반 출시 완료

2026-08-31 이후 신규 앱 정책이 API 36을 요구하면 UE 5.6에서 강제로 우회하지 않고 엔진 및 출시 일정을 다시 검토합니다.
