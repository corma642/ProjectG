# 외부 에셋 출처·라이선스 원장

기준일: 2026-10-12. 출처는 `.uasset`의 AssetImportData 경로와 git 이력으로 추정하고, 라이선스는 각 판매처 페이지에서 확인했다(조사 세션 기록은 Notion 패치 노트 2026-10-12 항목).

**소유자 확인(2026-10-12):** 프로젝트에 포함된 모든 에셋은 소유자가 직접 제작했거나, AI로 제작했거나, Fab에서 무료로 취득한 것이다. 출처 불명으로 표시된 항목(캐릭터 메시, BGM, Desktop SFX, SkyBox, Water, PresetsV3, Bowl 텍스처)은 이 확인에 따라 자체 제작 또는 Fab 무료 취득분으로 본다. 따라서 저작권 문제로 교체할 에셋은 없고, 남는 일은 아래 의무 사항뿐이다.
- Fab 무료 취득분: 라이브러리 캡처를 `Releases/evidence/`(Git 제외)에 보관한다(분쟁 대비, 선택).
- AI 생성물(Runner_Laco 등): Meshy Free 플랜 산출물이면 앱 내 크레딧 "Model created with Meshy – CC BY 4.0 License"가 필요하고, 유료 플랜이면 불필요하다. 플랜 확인이 유일한 미결 사항이다.
- 폰트: 무료라도 고지 의무가 있다(4절). 앱 내 라이선스 화면에 넣는다.
- 저장소 공개 범위: Fab·Epic 콘텐츠가 든 `Content/Assets`를 public 저장소에 두는 것은 Fab EULA 5(a)의 단독 배포에 해당한다. 저장소가 public이면 private으로 바꾼다.
아래 1~3절과 5~6절은 조사 당시 기록이며, 위 소유자 확인이 우선한다.

## 1. 요약

| 폴더·에셋 | 제품·출처 | 라이선스 | 상용 게임 포함 | 표기 의무 | 신뢰도 | 필요한 증빙 |
|:---|:---|:---|:---:|:---|:---:|:---|
| `Content/Assets/Construction_VOL1` | Dekogon Studios "Construction Site VOL. 1 - Supply and Material Props" (Fab, 2019-11 Permanent Collection 무료) https://www.fab.com/listings/ba44a508-bfa5-444c-bbf4-69e8b5dee530 | Fab Standard | 가능 | 없음 | 확실 | Fab 라이브러리 캡처 |
| `Content/Assets/Fantastic_Dungeon_Pack`, `Content/Assets/VFX/Fantastic_Dungeon_Pack` | Tidal Flask Studios "FANTASTIC - Dungeon Pack" (Fab 유료, 2026-04-21까지 한시 무료) https://www.fab.com/listings/25c67cda-991e-48a8-a194-478624ae4c0e | Fab Standard | 가능 | 없음 | 제품 확실 / 취득 미확인 | 라이브러리 캡처(취득일 2026-04-21 이전) |
| `Content/Assets/Isometric_World` (Core, Sky_Temple) | GameAssetFactory "Isometric World : Sky Temple" (Fab 유료, 2023-04 월간 무료) https://www.fab.com/listings/57bff667-e233-43db-b8e0-d29b9fbdafde | Fab Standard | 가능 | 없음 | 추정 | 라이브러리·Vault 캡처 또는 영수증 |
| `Content/Assets/ModularLostRuinKit` | KK Design "Modular Lost Ruins Kit" (Fab 유료, 2021-12 월간 무료) https://www.fab.com/listings/3a42926f-8e22-4041-98ac-529524f6d7e4 | Fab Standard | 가능 | 없음 | 확실 | 라이브러리·Vault 캡처 |
| `Content/Assets/SoulCity` | Epic Games "Soul: City" (무료) https://www.fab.com/listings/dd77fee6-0ad2-41ce-b32c-09300c24c9f3 | Fab Standard + Epic UE 전용 콘텐츠 | 가능(UE 게임) | 없음 | 확실 | 라이브러리 캡처 |
| `Content/Assets/StylizedVolClouds` | SERLO "Stylized Volumetric Clouds Shader" (Fab, 무료~유료 등급) https://www.fab.com/listings/3dcea533-24d9-4aa7-bbf6-bdf876c1f3eb | Fab Standard | 가능 | 없음 | 확실 | 라이브러리 캡처(취득 등급) |
| `Content/Assets/VFX/Stylized_Nature_Set` | Baturinets "Stylized Nature Set" (Fab 무료) https://www.fab.com/listings/ac774c74-0424-435e-8996-21ae6de158db | Fab Standard | 가능 | 없음 | 확실 | 라이브러리 캡처 |
| `Content/Assets/Crow` | PROTOFACTOR INC "CROW" (Fab 유료) https://www.fab.com/listings/cf805b14-dfb4-4b70-80b9-317f9a5f3992 | Fab Standard | 가능 | 없음 | 추정 | 라이브러리 캡처 또는 영수증(유료이므로 필수) |
| `Content/Assets/Characters/Mannequins` | Epic "Mannequins Asset Pack" / UE5 템플릿 동봉 애니메이션 https://www.fab.com/listings/ad542ee6-34d0-4828-a56c-13846cd649d0 | Fab Standard + Epic UE 전용 | 가능(UE 게임) | 없음 | 확실 | 불필요(템플릿 동봉) |
| `Content/Assets/Characters/Runner_Daisy`·`Runner_Luna`·`Runner_Male`·`Runner_Tyron` 메시 | 불명(소유자 Desktop fbx) | — | 확인 필요 | — | 불명 | 제작 과정 파일 또는 구매 증빙 |
| 위 캐릭터 애니메이션(Bored, Bashful, Thinking, Talking, Slide Hip Hop Walk) | Adobe Mixamo 추정 https://helpx.adobe.com/creative-cloud/faq/mixamo-faq.html | Mixamo(Adobe 일반약관) 로열티 프리 | 가능(비디오 게임 명시) | 없음 | 추정 | Adobe ID, 다운로드 원본 FBX |
| `Content/Assets/Characters/Runner_Laco` | Meshy AI 생성(2026-06-04) https://www.meshy.ai/terms-of-use | Free 플랜: CC BY 4.0 / 유료 플랜: 사용자 소유 | 가능 | Free면 "Model created with Meshy – CC BY 4.0 License" | 플랜 불명 | Meshy 플랜·결제 이력, 에셋 페이지 캡처 |
| `Content/Assets/CityCargos/NewsPaper` | Entroverse Lab "Stylized NYC Street Props" (Fab 무료) 추정 https://www.fab.com/listings/a2442500-d061-4b60-8d38-8dd4898120ab | Fab Standard | 가능 | 없음 | 추정 | 라이브러리 캡처 |
| `Content/Assets/SkyBox` | 불명(SM_SkySphere.FBX, Day_02_Sunless.png, T_Sky_RockyHills.PNG) | — | 확인 필요 | — | 불명 | 다운로드 출처 URL |
| `Content/Assets/Materials/Water` (T_Water_N) | 불명 | — | 확인 필요 | — | 불명 | 출처 |
| `Content/Assets/Materials/PresetsV3` (CelshaderV3, 8bit_LUT) | 불명(튜토리얼·Gumroad 계열 추정) | — | 확인 필요 | — | 불명 | 출처 |
| `Content/Assets/Bowl` 텍스처 T_Bowl_Diffuse | Sketchfab 다운로드 추정("clay-bowl", Misa gliniana) | CC 계열 추정 | 조건부(NC·SA면 불가) | CC BY면 표기 | 불명 | zip 내 license.txt, 원본 URL |
| `Content/Assets/Font` BMDOHYEON | 우아한형제들 배민 도현체 https://www.woowahan.com/fonts/license | OFL 1.1 | 가능(앱·게임 UI 명시) | 저작권 고지 + OFL 전문 표시 | 확실 | 라이선스 페이지 캡처 |
| `Content/Assets/Font` Cafe24 PRO Slim Air·Fit v2.0, Cafe24 Ohsquare | 카페24 https://fonts.cafe24.com/ | OFL 1.1 | 가능 | 라이선스 전문 포함(어려우면 출처 표기) | 확실 | zip 내 라이선스 txt |
| `Content/Assets/Font` KBLJump_EB_Condensed | KBL 점프체 https://www.kbl.or.kr/intro/font | KBL 자체 약관 | 가능(상업 명시) | 없음 | 임베딩 조항 없음(자료 상충) | 페이지 캡처, KBL 서면 확인 |
| `Content/Assets/Font` NEXON Football Gothic B·L | 넥슨 https://brand.nexon.com/ko/ci-brand-guidelines/typeface | 넥슨 서체 약관 | 가능(임베딩 허용) | 패키지의 "저작권 안내" 동봉, 출처 표기 권장 | 확실 | FAQ 캡처 + 패키지 라이선스 txt |
| `Content/Assets/Font` SB_Aggro B·L·M | 샌드박스네트워크 어그로체 https://noonnu.cc/font_page/738 | 샌드박스 자체 약관 | 가능 | 없음 | 추정(수정 조건 상충) | 패키지 라이선스 PDF |
| `Content/Sounds/BGM/SW_HomeBGM_City` | Epic Soul 팩 동봉 음원 추정(`E:/Soul/Music/Soul_Music_FlyUp.wav`) | Soul: City와 동일 추정 | 확인 후 가능 | 없음 | 추정 | Vault 폴더 내 동일 파일 캡처 |
| `Content/Sounds/BGM/SW_HomeBGM_Coastal`, `SW_InGameBGM` | 불명(Desktop wav) | — | 확인 필요 | — | 불명 | 제작·구매 증빙 |
| `Content/Sounds/SFX` Clipchamp 태그 7종(SW_Buy, SW_CargoFinish, SW_Countdown, SW_Die, SW_Female_Jump_01, SW_Male_Jump_02, SW_ReceiptPickup) | Microsoft Clipchamp 스톡 추정 https://support.microsoft.com/en-gb/topic/adding-stock-music-and-audio-c770d09a-79ae-4533-8ba8-54e0bc05de42 | Clipchamp 스톡 약관 | 스톡이면 불가(Clipchamp 영상 내 사용 한정) | — | 추정 | 자체 녹음본이면 원본, 스톡이면 교체 |
| `Content/Sounds/SFX/SW_Flock` | 불명(D:/Download) | — | 확인 필요 | — | 불명 | 출처 |
| `Content/Sounds/SFX/SW_CargoDanger`, `SW_HazardHit` | 2026-10-08 세션에서 스크립트로 합성한 자체 제작물 | 자체 | 가능 | 없음 | 확실 | 없음(임시음, 교체 예정) |
| 나머지 SFX(SW_ButtonClick, SW_CargoDrop, SW_Lack, SW_Snowstorm, SW_Wind, SW_Female_Jump_02, SW_Male_Jump_01) | 불명(Desktop) | — | 확인 필요 | — | 불명 | 출처 |

## 2. 항목별 근거

- **Construction_VOL1**: Fab 설명 "Permanent Collection Unreal Engine Sponsored Content for November 2019, Art created by Dekogon Kollab Artists", 스탠다드 라이선스. 검색에 먼저 뜨는 리스팅 079a0cd7은 VOL. 2라 폴더와 다르다. 임포트 경로의 `F:/1.3D_WORK/1.1_Sales/CityTrashSet`은 참여 아티스트 작업 경로로 팩에 동봉된 텍스처다.
- **Fantastic Dungeon Pack**: 현재 유료(₩132,810~). 한시 무료는 CG Channel 2026-04 기사(https://www.cgchannel.com/2026/04/get-650-free-3d-assets-for-building-dungeons-in-blender-unity-and-unreal-engine/). Fab EULA 1(a) 라이브러리 추가=거래, 2(b) 거래 시 표준 라이선스 부여, 7(a) 취득 시점 약관 유지라 무료 기간 취득분은 영구 사용 가능(https://www.fab.com/eula). 취득일이 보이는 라이브러리 캡처가 핵심 증빙.
- **Isometric World : Sky Temple**: 2023-04 마켓플레이스 월간 무료(https://www.unrealengine.com/blog/featured-free-unreal-marketplace-content-april-2023). `Core` 폴더는 같은 판매자 공용 코어로 추정.
- **Modular Lost Ruins Kit**: 2021-12 월간 무료(https://www.unrealengine.com/en-US/blog/featured-free-marketplace-content-december-2021).
- **Soul: City**: Epic 공지 "Licensed for use with the Unreal Engine only"(https://www.unrealengine.com/en-US/blog/soul-city-soul-cave-assets-released-for-free). Epic 콘텐츠 라이선스 계약의 UE 전용 콘텐츠 조항(https://www.unrealengine.com/en-US/eula/content): 엔진 코드가 필요한 제품에서만 사용 가능 → UE 5.6 Android 게임은 가능, 타 엔진 이식 불가.
- **StylizedVolClouds**: Fab 리스팅 연락처 `mr.hamzik1026@mail.ru`가 임포트 경로 `C:/Users/mrham/`와 일치. `StylizedThroneRoom` 텍스처는 제작자의 다른 프로젝트에서 재사용한 흔적이며 별도 상품이 아니다. 무료~유료 등급이 있어 취득 등급 확인.
- **Crow**: SK_Crow 임포트 타임스탬프 2017-11-02, 3ds Max 프랑스어 경로, Unity식 `Crow@Fly` 파일명이 PROTOFACTOR "CROW"(2017-11-04 출시, UE+Unity, 17 애니메이션) 구성과 일치. 유료 상품이라 구매 또는 무료 제공 취득 증빙 필수.
- **Mannequins**: 임포트 경로 `D:/Builds/Ed.Fornoles_L2171_Depot/.../MM_Unarmed_Jump_*.fbx`는 Epic 내부 경로로 UE5 템플릿 동봉 애니메이션.
- **Runner 4종 + Mixamo**: 애니메이션 파일명이 Mixamo 클립명과 일치. 메시 본 이름은 `mixamorig:` 접두사 없는 root/Hips라 Mixamo 오토리그 후 보정 또는 자체 제작 모두 가능. Adobe FAQ: 캐릭터·애니메이션 모두 상업 게임 사용 로열티 프리. Adobe 일반약관 3.6: 콘텐츠 파일은 최종 용도 안에서만 배포 가능, 단독 배포 금지(https://www.adobe.com/legal/terms.html).
- **Runner_Laco (Meshy)**: 이용약관 3.2 Free 플랜 산출물은 CC BY 4.0, 유료 플랜은 사용자 소유; 3.3 Community 공개 시 CC0. 헬프센터(https://intercom.help/meshy/en/articles/16102098-can-i-use-meshy-assets-commercially): Free는 크레딧 필수, 권장 문구 "Model created with Meshy – CC BY 4.0 License". CC BY 4.0은 Fab EULA 6(a) 비호환 라이선스가 아니다.
- **NewsPaper**: 임포트 경로 `D:/Projeler/...`(터키어 Projeler=프로젝트)는 판매자 PC 경로. Entroverse Lab "Stylized NYC Street Props"(무료, Mailbox 태그)가 가장 근접하나 SM_NewsPaper 포함 여부 미확인. 현재 DA_Cargo_NewsPaper가 참조하지 않아 쿡되지 않는다(Shipping 미포함).
- **폰트**: 배민 도현체는 OFL 1.1, 번들 시 저작권 고지("Copyright © 2013, Woowa Brothers Corporation ... Reserved Font Name BM DoHyeon")와 OFL 전문 표시, 변형 시 이름 사용 불가. Cafe24 PRO Slim·Ohsquare는 OFL 1.1, 라이선스 전문 포함 또는 출처 표기 권장(https://www.sandollcloud.com/free-font/23427/Cafe24-PRO-Slim). KBL 점프체는 상업 이용 가능·유료 판매 금지·임베딩 조항 없음(눈누 "가능", 폰코 "별도 계약"으로 상충). 넥슨 풋볼 고딕 FAQ: 상업 사용 가능, 수정 재배포 금지, "본 저작권 안내를 포함해서 다른 소프트웨어와 번들하거나 임베디드 폰트로 사용" 가능, 출처 표기 권장. SB 어그로체는 상업 사용 가능·유료 판매 금지, 수정 가능 여부 자료 상충.
- **BGM**: SW_HomeBGM_City는 FileMD5 비어 있음·비정상 타임스탬프로 Epic 측 에셋 특징이며 2026-05-28 커밋 ab4a91e에서 SoulCity 916개 파일과 함께 추가됐다. Soul 팩 설명에 사운드 언급이 없어 Vault 폴더에서 동일 파일을 확인해야 한다.
- **SFX**: Microsoft 문서 "stock ... assets in videos that you save from Clipchamp ... may not download and redistribute the individual underlying stock files, use them in other editing programs". 스톡 음원을 wav로 추출해 게임에 쓰는 것은 범위 밖. 직접 녹음·제작한 소리를 Clipchamp로 편집만 했다면 문제 없음(태그는 내보내기 흔적).
- **Bowl 텍스처**: `D:/Download/clay-bowl/textures/3D_Z_2165_Misa_gliniana_color.jpeg`(Sketchfab 다운로드 구조, 폴란드어 "점토 그릇", 2021-03). CC BY-NC면 상용 불가, CC BY-SA면 Fab EULA 6(a) 비호환, CC BY면 표기 필요.

## 3. Fab Standard License·Epic UE 전용 콘텐츠 요약

출처: https://www.fab.com/eula , https://www.unrealengine.com/en-US/eula/content , https://dev.epicgames.com/documentation/en-us/fab/licenses-and-pricing-in-fab

1. 상업 배포 허용(EULA 4(c)): 콘텐츠를 통합한 게임을 최종 사용자에게 배포 가능, 크레딧 불필요. 개인 등급은 최근 12개월 총수익 10만 달러 미만 구매자용이며 구매 후 초과해도 업그레이드 불필요(2(a)).
2. 단독 재배포 금지(6(b)): 독립 실행형 판매·양도 불가, 제3자가 자기 제품에 통합하게 허용 불가, 최종 사용자에게는 통합된 형태만 허용하고 추출을 제한해야 한다(4(c)).
3. **공개 GitHub에 `.uasset` 포함은 위반**(5(a)): 협업자 또는 비공개 저장소만 허용. 저장소 `github.com/corma642/ProjectG`가 public이면 private으로 바꾸거나 `Content/Assets`의 Fab·Epic·Mixamo 콘텐츠를 커밋에서 제외해야 한다.
4. 한시 무료 취득분은 영구 라이선스(1(a), 2(b), 7(a)). 비호환 라이선스(GPL, LGPL 정적 링크, CC BY-SA)와 결합 금지(6(a)).
5. Epic UE 전용 콘텐츠(Soul: City, Mannequin): 엔진 코드가 필요한 제품에서만 권리 행사 가능. UE 5.6 Android 게임 출시 허용, 타 엔진 이식·단독 배포 불가.

## 4. 앱 내 라이선스 화면에 넣을 고지(증빙 확정 후 구현)

- 배민 도현체: 저작권 고지 + OFL 1.1 전문.
- Cafe24 PRO Slim Air/Fit, Cafe24 Ohsquare: "이 앱에는 카페24(주)이 제공한 Cafe24 PRO Slim, Cafe24 아네모네가 적용되어 있습니다." + OFL 1.1 전문(공유 가능).
- NEXON Football Gothic: 패키지 동봉 저작권 안내 문구 + "NEXON Football Gothic © NEXON Korea Corporation".
- Meshy(Runner_Laco가 Free 플랜 산출물일 때): "Model created with Meshy – CC BY 4.0 License".
- Bowl 텍스처가 CC BY이면 원작자·라이선스 표기.
- `Fonts_Custom`이 폰트 10종을 전부 참조해 미사용 6종(NEXON 2, SB_Aggro 3, Cafe24 PROSlimFit)도 쿡된다. 사용하지 않는 폰트는 제거해 고지 대상을 줄인다.

## 5. 소유자 확인 질문

1. Fab 라이브러리(https://www.fab.com/library)에서 취득일이 보이게 캡처: Construction Site VOL.1, FANTASTIC - Dungeon Pack(2026-04-21 이전), Isometric World : Sky Temple, Modular Lost Ruins Kit, Soul: City, Stylized Volumetric Clouds Shader(취득 등급), Stylized Nature Set, PROTOFACTOR CROW, Stylized NYC Street Props, Mannequins Asset Pack. 유료 4종(Sky Temple, Lost Ruins, CROW, Dungeon Pack)은 무료 제공 기간 취득인지 구매인지.
2. Runner_Daisy/Luna/Male/Tyron 메시는 누가 만들었나(자체 제작, Meshy, Mixamo 기본 캐릭터, 구매)? Mixamo 오토리그를 거쳤나? 애니메이션 5종이 Mixamo 다운로드가 맞나?
3. Meshy: 2026-06-04 생성 당시 플랜(Free/Pro/Studio), Community 공개 여부, 레퍼런스 이미지 사용 여부.
4. Clipchamp 태그 SFX 7종: Clipchamp 스톡인가, 직접 녹음·제작 후 편집만 한 것인가? 스톡이면 교체.
5. SW_HomeBGM_City가 Soul: City Vault 폴더의 `Soul_Music_FlyUp`과 같은 파일인가. SW_HomeBGM_Coastal, SW_InGameBGM, SW_Flock, 나머지 Desktop SFX 7종의 출처(자체 제작/AI 생성/구매 팩/프리 사운드 사이트와 라이선스).
6. SkyBox 3파일, Materials/Water 텍스처, Materials/PresetsV3(CelshaderV3, 8bit_LUT)의 다운로드 출처 URL.
7. Bowl 텍스처 다운로드 zip의 license.txt 내용과 원본 페이지 URL.
8. 폰트 패키지 동봉 라이선스 txt/PDF 보관(Cafe24 PRO Slim v2.0, Cafe24 Ohsquare, SB_Window_Font, NEXON). KBL에 앱 임베딩 허용 여부 서면 문의.
9. GitHub 저장소가 public이면 Content 폴더 처리 방식(private 전환 또는 제외).
10. README의 "무료로 사용할 수 있는 에셋이거나 직접 제작" 문구는 위 증빙이 모이기 전까지 근거가 없다. 원장 확정 후 문구를 맞춘다.

## 6. 교체 후보(증빙이 나오지 않으면)

- 유료 Fab 팩 4종 중 취득 증빙이 없는 것: 구매(가격은 1절) 또는 무료 대체 팩.
- Clipchamp 스톡 SFX 7종: CC0 사운드(freesound.org CC0 필터, Kenney 등)로 교체. 교체 시 Notion 에디터 가이드 19절의 사운드 지정 위치를 따른다.
- 출처 불명 BGM 2곡·SW_Flock·Desktop SFX 7종: 자체 제작 또는 CC0·구매 음원으로 교체.
- 캐릭터 4종·Bowl 텍스처·SkyBox·Water·PresetsV3: 출처 확인 불가 시 Fab 무료 에셋 또는 자체 제작으로 교체. 캐릭터 교체는 스토어 그래픽(`StoreListing.ko.md`) 재제작을 동반한다.
