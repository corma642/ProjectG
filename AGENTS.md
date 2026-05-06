# AGENTS.md

이 문서는 Codex가 `C:\Documents\GitHub\ProjectG`에서 작업할 때 따르는 프로젝트 기준 문서입니다.
CLI 세션이 새로 시작되면 가장 먼저 이 문서를 읽고, 이후 필요한 정보는 아래에 지정된 Notion 페이지 범위 안에서만 확인합니다.

Behavioral guidelines to reduce common LLM coding mistakes. Merge with project-specific instructions as needed.

**Tradeoff:** These guidelines bias toward caution over speed. For trivial tasks, use judgment.

## 1. Think Before Coding

**Don't assume. Don't hide confusion. Surface tradeoffs.**

Before implementing:
- State your assumptions explicitly. If uncertain, ask.
- If multiple interpretations exist, present them - don't pick silently.
- If a simpler approach exists, say so. Push back when warranted.
- If something is unclear, stop. Name what's confusing. Ask.

## 2. Simplicity First

**Minimum code that solves the problem. Nothing speculative.**

- No features beyond what was asked.
- No abstractions for single-use code.
- No "flexibility" or "configurability" that wasn't requested.
- No error handling for impossible scenarios.
- If you write 200 lines and it could be 50, rewrite it.

Ask yourself: "Would a senior engineer say this is overcomplicated?" If yes, simplify.

## 3. Surgical Changes

**Touch only what you must. Clean up only your own mess.**

When editing existing code:
- Don't "improve" adjacent code, comments, or formatting.
- Don't refactor things that aren't broken.
- Match existing style, even if you'd do it differently.
- If you notice unrelated dead code, mention it - don't delete it.

When your changes create orphans:
- Remove imports/variables/functions that YOUR changes made unused.
- Don't remove pre-existing dead code unless asked.

The test: Every changed line should trace directly to the user's request.

## 4. Goal-Driven Execution

**Define success criteria. Loop until verified.**

Transform tasks into verifiable goals:
- "Add validation" → "Write tests for invalid inputs, then make them pass"
- "Fix the bug" → "Write a test that reproduces it, then make it pass"
- "Refactor X" → "Ensure tests pass before and after"

For multi-step tasks, state a brief plan:
```
1. [Step] → verify: [check]
2. [Step] → verify: [check]
3. [Step] → verify: [check]
```

Strong success criteria let you loop independently. Weak criteria ("make it work") require constant clarification.

---

**These guidelines are working if:** fewer unnecessary changes in diffs, fewer rewrites due to overcomplication, and clarifying questions come before implementation rather than after mistakes.

---

## 1. 프로젝트 기본 정보

| 항목 | 내용 |
|:---|:---|
| 프로젝트명 | Go Deliver! |
| 장르 | 모바일 3D 하이퍼 캐주얼 감성의 무한 밸런스 러너 |
| 핵심 차별점 | 자동 전진 러닝 + 스마트폰 기울임 중심 제어 + 머리 위 적재물 균형 유지 |
| 목표 감정 | 속도감보다 긴장감. 언제든 무너질 수 있는 불안정한 배달 감각 |
| 엔진 | Unreal Engine 5.6.1 |
| 플랫폼 | Android |
| 개발 형태 | 1인 개발 |
| 리포지토리 | `ProjectG` |
| 솔루션 | `ProjectG.sln` |
| 프로젝트 파일 | `ProjectG.uproject` |
| 주 모듈 | `ProjectG` |
| 타깃 | `Source/ProjectG.Target.cs`, `Source/ProjectGEditor.Target.cs` |

---

## 2. 최상위 기획 기준

### 게임 한 줄 정의
**자이로로 중심을 잡고, 좌우 화면 터치로 끊긴 길을 넘어가며, 머리 위 배달물을 끝까지 지켜내는 3D 무한 밸런스 러너**

### 개발 시 반드시 유지할 방향성
- 이 게임의 본질은 일반 러너의 속도감이 아니라 **중심 유지와 적재 불안정성에서 오는 긴장감**이다.
- 입력은 단순해야 하지만 결과는 단순하지 않아야 한다.
- 속도 증가만으로 난이도를 올리지 말고, **적재 증가 + 환경 이벤트 빈도 증가 + 복합 압박 증가**로 난이도를 설계한다.
- 실패는 무조건 즉사형보다 **누적 페널티형 구조**를 우선한다.
- 콘텐츠 확장은 시스템 남발보다 **상황 조합**으로 해결한다.
- 프로토타입 단계에서는 테마 확장보다 **핵심 재미 검증**을 우선한다.

### 현재 프로토타입 최우선 검증 범위
- 자동 전진
- 자이로 중심 제어
- 좌/우 화면 터치 라인 점프
- 3라인 구조
- 계란 적재 1종
- 적재 추가 1단계 이상
- 바람 이벤트
- 얼음 길 이벤트
- 코인 수집
- 적재 손실 페널티
- 추락 게임 오버
- 점수/재시작 루프

---

## 3. Notion 접근 범위와 기준 문서

### 접근 허용 범위
Codex는 반드시 아래 최상위 페이지와 그 하위 페이지까지만 접근한다.

- `Go Deliver! Development Hub`  
  ID: `34aea150-fa75-80c9-9553-fb0762201bab`

같은 워크스페이스의 다른 페이지가 검색 결과에 보여도 열람하거나 수정하지 않는다.

### 기준 페이지
- 프로젝트 기획서  
  ID: `34aea150-fa75-80a5-8c7f-ebdc9cb012df`
- 스프린트 진행 현황  
  ID: `34aea150-fa75-8051-ac3d-e56c6cee93de`
- 에디터 작업 플로우 가이드  
  ID: `34aea150-fa75-80b6-b1cc-f9aac6e2957e`
- 패치 노트 (Patch Notes)  
  ID: `34aea150-fa75-8090-a345-c79f56064d8f`

### Notion 사용 원칙
- 게임의 기획, 의도, 방향성은 항상 `프로젝트 기획서`를 최상위 기준으로 삼는다.
- 스프린트 상태와 우선순위는 `스프린트 진행 현황`을 기준으로 맞춘다.
- 에디터에서 수동 세팅 절차가 새로 생기면 `에디터 작업 플로우 가이드`에 누적한다.
- 사용자가 직접 수행해야 하는 에디터 작업은 반드시 `에디터 작업 플로우 가이드`에 별도 워크플로우로 정리하고, 완료 보고 시 사용자에게 핵심 절차를 함께 안내한다.
- 구현 세션이 확정적으로 끝나면 `패치 노트 (Patch Notes)`를 갱신한다.
- 기획 변경이 필요하면 하위 문서를 먼저 바꾸지 말고, 사용자 확인 후 `프로젝트 기획서`부터 갱신한다.
- 조사 중이거나 방향 검토 단계에서는 Notion 문서를 갱신하지 않는다.

---

## 4. 완료 시 문서 동기화 규칙

### 스프린트 진행 현황 갱신 조건
- 새로운 시스템이나 기능을 구현했을 때
- 기존 시스템의 동작을 확정적으로 수정했을 때
- 스프린트 상태, 완료 항목, 남은 이슈, 다음 조건이 바뀌었을 때

### 에디터 작업 플로우 가이드 갱신 조건
- 새 Data Asset 타입, 블루프린트 세팅 절차, 레벨 배치 절차 등
- C++ 코어 시스템을 사용하기 위해 에디터에서 반복 수행해야 하는 수동 작업이 실제로 생겼을 때

### 패치 노트 갱신 조건
- 시스템 수정 또는 새 기능 구현 세션이 확정적으로 끝났을 때
- 이전 대비 변경점, 개선된 기능, 수반되는 에디터 작업을 요약할 수 있을 때

### 갱신 금지 조건
- 조사만 한 경우
- 방향성만 논의한 경우
- 구현이 끝나지 않은 중간 상태인 경우

---

## 5. 작업 방식

### 작업 시작 전
- 먼저 현재 코드베이스와 관련 문서를 분석하고, 바로 구현 가능한지 판단한다.
- 구현은 `프로젝트 기획서`와 `스프린트 진행 현황`의 순서를 따른다. 사용자가 별도 우선순위를 명시하지 않으면 현재 활성 스프린트의 다음 미완료 항목부터 진행한다.
- 작업은 사용자가 커밋하기 쉬운 논리 단위로 끊어서 진행한다. 서로 성격이 다른 변경은 가능한 한 한 번에 섞지 않는다.
- 요구사항이 기획서와 충돌하거나 방향성이 모호하면 사용자에게 먼저 질문한다.
- 예외 처리, 성능, 엔진 내장 기능 활용 측면에서 더 나은 방법이 보이면 구현 전에 사용자에게 선택지를 설명한다.

### 계획 제출 전 검토
- 계획을 제출하기 전에 기획서 방향성과 현재 스프린트 범위에 맞는지 한 번 더 검토한다.
- Tick, 동적 할당, 반복 Cast, 컴포넌트 재설정 등 모바일 성능에 불리한 구조가 없는지 확인한다.
- UE 내장 기능, 이벤트, 초기화 지점, 설정 반영 위치 중 더 적절한 대안이 있는지 검토한다.
- 현재 구현만 해결하고 이후 확장을 막는 구조가 아닌지 확인한다.
- 검토 결과 구조적 문제가 보이면 기존 계획을 그대로 제출하지 않고 수정된 계획을 제시한다.

### 구현 직전 재검토
- 사용자가 계획 구현을 요청하더라도 즉시 실행하지 않고, 구현 직전에 계획의 구조적 타당성을 다시 점검한다.
- 상태 갱신과 설정 적용 위치가 올바른지 확인한다.
- 런타임 로직과 에디터/튜닝 편의 로직이 섞이지 않았는지 확인한다.
- 불필요한 매 프레임 작업이나 과도한 비용이 없는지 확인한다.
- 커밋 단위가 너무 크거나 서로 다른 성격의 변경이 섞이지 않았는지 확인한다.
- 재검토 결과 문제가 있으면 구현을 멈추고, 문제점과 수정 방향을 먼저 사용자에게 제시한다.

### 구현 원칙
- Unreal Engine 5.6+의 내장 기능을 우선 활용한다.
- Android 타깃이므로 CPU/GPU 비용, Tick 남용, 과도한 물리 연산, 불필요한 UObject 생성 비용을 항상 경계한다.
- 임시 구현이라도 나중에 시스템 확장이 가능한 구조로 만든다. 단, 프로토타입 단계에서 과도한 추상화는 피한다.
- 입력 처리에는 가능하면 `Enhanced Input`과 엔진 표준 경로를 우선 사용한다.
- 자이로, 라인 이동, 적재 흔들림은 서로 분리된 책임으로 설계해 결합도를 낮춘다.
- 물리 정확성보다 플레이어가 체감하는 긴장감과 납득 가능한 실패 피드백을 우선한다.
- 모바일 화면 가독성을 항상 고려한다. 카메라, UI, 경고 연출은 적재물과 전방 시야를 함께 보여줘야 한다.

### 코드 작성 원칙
- 새 코드는 가능한 한 최신 C++ 문법과 Unreal 스타일을 따른다.
- 불필요한 동적 할당, 중복 Tick, 과도한 Cast, 불명확한 상태 플래그를 피한다.
- 같은 목적을 더 안전하고 효율적으로 처리하는 UE 내장 함수나 시스템이 있으면 우선 사용한다.
- 새 네이밍 규칙이 필요한 경우 현재 코드베이스와 모듈명을 우선 존중한다. 전역 접두사 체계 변경은 사용자 확인 후 적용한다.

### UI Widget 작업 기준
- 앞으로 새로 작성하는 위젯은 Blueprint Widget이 레이아웃, 디자인, 스타일, 해상도 대응을 담당한다.
- C++ 위젯 클래스는 상호작용, 이벤트 바인딩, 데이터 주입, 상태 전환, 애니메이션 로직만 담당한다.
- C++에서 로직이나 애니메이션에 필요한 위젯 참조는 `UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Transient)`로 선언한다.
- C++ 로직에 필요한 BindWidget은 필수로 간주한다. 미할당 가능성을 fallback UI로 처리하지 않고, `NativeConstruct()`에서 경고 로그로 드러낸다.
- 새 위젯에서 `WidgetTree->ConstructWidget()`로 전체 화면 구조를 C++에서 생성하지 않는다.
- 불필요한 스타일/레이아웃 튜닝값을 C++ 설정으로 노출하지 않는다. 디자인 조정은 Widget Blueprint의 슬롯, 앵커, Safe Zone, ScaleBox, 폰트, 브러시에서 처리한다.
- Codex는 위젯 구현 시 필요한 BindWidget 이름, 추천 레이아웃, 모바일 해상도 대응 방법, 주의할 점을 사용자에게 함께 안내한다.
- 기존 `UPGRunResultWidget`처럼 이미 C++ 생성형으로 만들어진 위젯은 새 작업에 섞어 고치지 않고, 별도 리팩터링 작업으로 분리한다.

### 주석 작성 원칙
- 모든 클래스에는 해당 클래스가 맡는 역할과 책임을 한국어 주석으로 반드시 설명한다.
- 변수 주석은 블루프린트/에디터에서 값을 수정할 때 바로 이해할 수 있도록 간결하고 직관적으로 작성한다. 예: `적재물 개수`, `라인 간격`, `자이로 민감도`.
- 함수에는 호출 목적과 외부에서 기대할 동작을 한국어 주석으로 설명한다.
- 복잡한 함수 내부 로직, 보정식, 예외 처리, 입력 변환, 상태 전환에는 의도와 이유를 알 수 있는 한국어 주석을 작성한다.
- 단순 대입처럼 코드만 봐도 명확한 부분은 과도하게 반복 설명하지 않되, 게임 기획 의도나 튜닝 의미가 있는 값은 주석을 남긴다.

---

## 6. 설계 판단 기준

### 우선순위
1. 기획 의도에 맞는가
2. 모바일 프로토타입에서 재미 검증에 직접 기여하는가
3. UE 기본 기능으로 안정적으로 구현 가능한가
4. 성능과 유지보수 비용이 합리적인가

### 피해야 할 방향
- 단순 속도 상승만으로 난이도를 만드는 방식
- 적재물 실패 원인이 보이지 않는 불합리한 연출
- 자이로와 터치 입력이 서로 싸우는 구조
- 프로토타입 범위를 넘어선 과도한 시스템 확장
- 에디터 수작업 의존도가 높은데 문서화되지 않은 워크플로우

---

## 7. Git 및 보고 규칙

- Git 커밋과 푸시는 절대 수행하지 않는다.
- 브랜치 생성, 커밋, 리셋, 강제 체크아웃 등 Git 상태를 바꾸는 작업은 사용자 명시 요청 없이는 하지 않는다.
- 작업 완료 보고 시 변경 내용을 커밋 가능한 단위로 나누고, 각 단위마다 커밋 제목과 설명을 정리한다.
- 커밋 제목은 프로젝트 규칙에 맞춰 `NEW:`, `CNG:`, `FIX:` 형식을 사용한다.
- 작업 완료 보고 시에는 다음만 간결히 정리한다.
  - 무엇을 왜 바꿨는지
  - 확인한 결과
  - 남은 리스크 또는 사용자 확인 필요 사항
  - 사용자가 직접 커밋할 때 참고할 수 있는 커밋 메시지 초안

---

## 8. 세션 종료 전 체크리스트

- 구현 내용이 `프로젝트 기획서` 방향성과 일치하는가
- 완료 기준이 충족됐는가
- 새 에디터 수동 작업이 생겼는가
- Notion 갱신 조건을 만족했는가
- 조사 단계가 아니라 확정 완료 상태인가
- 사용자에게 전달할 검증 결과와 잔여 리스크가 정리됐는가
