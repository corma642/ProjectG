# Google Play 스토어 등록정보 초안

기준: 2026-10-12, 그래픽 2026-10-31. 그래픽 규격은 Play Console 기준(아이콘 512×512 PNG, 피처 그래픽 1024×500 알파 없음, 세로 스크린샷 1080×1920 4~8장 — 긴 변이 짧은 변의 2배를 넘으면 거부되므로 1080×2340은 쓰지 않는다, 선택 30초 내외 YouTube 트레일러).
스크린샷·피처 그래픽은 PC 쇼케이스 녹화 프레임(`Tools/Showcase/shot_run.txt`·`shot_home_shop.txt`, 1080×1920·10fps, `-KeepFrames`)에서 `Tools/Showcase/make_store_assets.py`로 만들어 `Docs/Release/StoreAssets/`에 둔다. 장면을 바꾸려면 스크립트의 `SCENES`·`FEATURE` 프레임 번호를 고친다. 사운드가 필요한 트레일러만 실기기 `adb shell screenrecord`로 찍는다. 에셋 교체는 없다(소유자 확인, `THIRD_PARTY_NOTICES.md`). Runner_Laco 제거(2026-10-12) 후 캐릭터 상점 스킨은 4종이다.

## 앱 이름 (30자 이내)

- 한국어: `Go Deliver! 밸런스 러너`
- 영어: `Go Deliver! Balance Runner`

## 짧은 설명 (80자 이내)

- 한국어: `머리 위 배달물을 떨어뜨리지 않고 달려라! 드래그로 중심을 잡고 플릭으로 점프하는 밸런스 러너`
- 영어: `Keep the cargo on your head! Drag to balance, flick to jump in this physics runner.`

## 전체 설명 (4000자 이내, 한국어)

```text
머리 위에 쌓인 배달물을 끝까지 지켜내는 3D 밸런스 러너!

Go Deliver!의 배달원은 멈추지 않고 달립니다. 당신이 할 일은 단 두 가지.
• 손가락으로 드래그해 중심을 잡기
• 플릭으로 끊긴 길을 뛰어넘기

하지만 머리 위 바구니에는 배달물이 계속 쌓입니다. 병, 화분, 신문 뭉치… 흔들리고, 튀어 오르고, 떨어지려 합니다. 한 번의 실수로 배달은 끝납니다.

■ 긴장감 넘치는 물리 배달
배달물은 실제 물리로 흔들립니다. 급하게 꺾으면 쏟아지고, 점프 후 착지 순간에는 숨을 참게 됩니다.

■ 날씨와 방해꾼
바람이 중심을 밀어내고, 눈보라는 손끝을 예민하게 만들며, 까마귀가 코앞으로 날아듭니다. 전조를 보고 미리 대비하세요.

■ 배달 완료 보상
정해진 시간 동안 배달물을 지키면 배달 완료! 점수와 영수증을 받고, 영수증으로 새로운 배달원과 맵을 열어보세요.

■ 간단한 조작, 깊은 긴장감
한 손으로 플레이할 수 있는 조작이지만 결과는 매번 다릅니다. 최고 기록에 도전하세요.

지금 바로 배달을 시작하세요. Deliver smiles, every mile!
```

## 전체 설명 (영어)

```text
A 3D balance runner where the delivery rides on your head!

Your courier never stops running. You only do two things:
• Drag to keep your balance
• Flick to jump over broken road

But the basket on your head keeps filling up. Bottles, pots, stacks of newspapers... they wobble, bounce, and try to fall. One slip and the delivery is over.

■ Physics-driven tension
Every item sways with real physics. Turn too hard and it spills; every landing is a held breath.

■ Weather and troublemakers
Wind pushes you sideways, snowstorms make your fingertips twitchy, and crows dive at you. Watch for the warning and get ready.

■ Deliver to earn
Keep an item safe until its timer runs out to complete the delivery. Earn score and receipts, then unlock new couriers and maps.

■ Simple controls, deep tension
One-handed controls, but no two runs are the same. Chase your best score.

Start delivering now. Deliver smiles, every mile!
```

## 그래픽 체크리스트

- [x] 아이콘 512×512 PNG: `Docs/Release/StoreAssets/icon_512.png`(원본 `Resources/Branding/AndroidAppIcon.png`)
- [x] 피처 그래픽 1024×500: `Docs/Release/StoreAssets/feature_graphic.png` — 적재물 3개를 얹고 다리를 건너는 러너 + 오른쪽 로고. 텍스트는 로고만.
- [x] 세로 스크린샷 1080×1920 8장: `Docs/Release/StoreAssets/screenshots/01~08` — 01 적재물 3개, 02 배달 완료 +1,150, 03 바람 배지, 04 눈보라, 05 까마귀 전조, 06 결과 화면(배달 실패·이어하기 버튼), 07 캐릭터 상점, 08 맵 상점. `09_home.jpg`는 예비(폰 스크린샷은 최대 8장). 실패 슬로모션은 흐림·암전 프레임이라 결과 화면으로 대체. 캡션은 넣지 않았다(저장소에 TTF가 없고 문구 없이도 장면이 읽힌다. 넣으려면 Console 업로드 전에 이미지 편집기로).
- [ ] 트레일러 30초: 조작 설명(드래그→플릭) 5초, 적재물 흔들림·배달 완료 15초, 실패 장면 5초, 로고 5초. 무음 녹화는 `Tools/Record-Showcase.ps1`, 사운드 포함본은 실기기 녹화.
- [x] 스크린샷에 가격·"무료"·순위 문구 없음(2026-10-31 산출물 확인). 트레일러도 같은 규칙.

## 카테고리와 태그

- 카테고리: 게임 > 아케이드
- 태그: 러너, 캐주얼, 물리, 싱글 플레이, 오프라인
- 연락처 이메일: corma642@naver.com (개인 개발자 계정, `PrivacyPolicy.ko.md`와 동일)
- 개인정보처리방침 URL: `https://corma642.github.io/godeliver-site/privacy/`(공개 저장소 `godeliver-site`에 게시 후)
