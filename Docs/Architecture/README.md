# Go Deliver! 코드 아키텍처 지도

현재 코드의 객체 수명, 소유 관계, 이벤트와 데이터 흐름을 하나의 Excalidraw 캔버스에 정리한 문서입니다. 기획 구조나 향후 확장안은 포함하지 않습니다.

## 파일

- [ProjectG_CodeArchitecture.excalidraw](ProjectG_CodeArchitecture.excalidraw) — 편집 원본
- [ProjectG_CodeArchitecture.svg](ProjectG_CodeArchitecture.svg) — 전체 지도 미리보기

기준 커밋은 `ba955c724137f26fcf397509f4cd92cb1751f08d`입니다.

## 읽는 순서

1. `00 · 지도 안내`에서 색상과 선의 의미를 확인합니다.
2. 중앙의 `01 · 전체 런타임 아키텍처`에서 객체 수명과 소유 관계를 확인합니다.
3. 실행 순서가 필요하면 `02 · 런 생명주기와 화면 전환`을 봅니다.
4. 입력이 이동, 물리와 실패로 전달되는 경로는 `03 · 입력과 플레이 시스템 흐름`에서 확인합니다.
5. 설정과 DataAsset의 로드 경로는 `04 · 콘텐츠와 비동기 예열`에서 확인합니다.
6. 화면 입력, 사용자 상태와 저장 복구는 `05 · UI 책임과 사용자 데이터 저장`에서 확인합니다.

## 표기 규칙

| 표기 | 의미 |
|:---|:---|
| 네이비 | 상태와 제어 흐름 |
| 청록 | 런타임 객체와 플레이 처리 |
| 주황 | Delegate, 이벤트와 UI 피드백 |
| 노랑 | 설정, DataAsset과 저장 데이터 |
| 회색 | Unreal 기반 요소와 보조 참조 |
| 실선 | 소유 또는 직접 호출 |
| 점선 | 데이터와 참조 공급 |
| 굵은 선 | 주 실행 흐름 |

## 근거 범위

- `Source/ProjectG`의 C++ 타입, 멤버 소유와 직접 호출
- `GetSubsystem`으로 확인되는 Subsystem 사용 관계
- `AddDynamic`, `Broadcast`로 확인되는 Delegate 흐름
- `.uproject`와 `ProjectG.Build.cs`의 Runtime Module 및 엔진 의존성
- 코드가 참조하는 Widget Blueprint, Blueprint Actor와 DataAsset 타입

Blueprint 그래프 내부 동작과 에셋 간 연결은 이름만으로 추정하지 않습니다. 빈 폴더, 외부 에셋 개별 파일, 빌드 산출물과 Android UPL도 제외합니다.

## 갱신 기준

다음 변경이 생기면 관련 Frame과 기준 커밋을 함께 갱신합니다.

- 객체 소유자나 Subsystem 수명이 변경된 경우
- 런 상태 또는 화면 전환 경로가 변경된 경우
- 입력, 적재물, 이벤트, 트랙과 보상 흐름이 변경된 경우
- Preload 그룹이나 Soft Reference 경계가 변경된 경우
- 사용자 데이터 필드, 저장 검증 또는 복구 정책이 변경된 경우

`.excalidraw`를 편집 원본으로 사용하고, 수정 후 전체 SVG를 다시 내보냅니다. Git diff를 읽을 수 있도록 원본 JSON은 들여쓰기 형식을 유지합니다.
