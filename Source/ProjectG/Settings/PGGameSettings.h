// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "PGGameSettings.generated.h"

class UPGCargoTypeDefinition;
class UPGCargoDropSet;
class UPGCharacterShopCatalog;
class UPGCharacterShopItemWidget;
class UPGRunResultWidget;
class UPGRunEventPresentationSet;
class UPGRunEventSet;
class UPGTutorialPageSet;
class UCameraShakeBase;
class UNiagaraSystem;
class USoundBase;
class UUserWidget;

/**
 * 자동 전진 캐릭터의 기본 이동/기울기 반응 설정
 */
USTRUCT(BlueprintType)
struct FPGRunnerSettings
{
	GENERATED_BODY()

	// 캐릭터가 매초 전진하는 기본 속도
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Runner", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1200.0"))
	float ForwardSpeed = 600.0f;

	// 중심 입력을 시각적으로 표현할 때 허용하는 최대 롤 각도
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Runner", meta = (ClampMin = "0.0", ClampMax = "45.0", UIMin = "0.0", UIMax = "30.0"))
	float BodyLeanMaxRoll = 12.0f;

	// 몸 기울기 보간 속도
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Runner", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "20.0"))
	float BodyLeanInterpSpeed = 8.0f;

	// 시작 높이보다 이 값 이상 아래로 떨어지면 추락 실패로 처리한다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Runner", meta = (ClampMin = "100.0", UIMin = "200.0", UIMax = "2000.0"))
	float FallFailZOffset = 600.0f;
};

/**
 * 직선 주행 중에도 줄타기처럼 중심이 흔들리는 기본 밸런스 설정
 */
USTRUCT(BlueprintType)
struct FPGBalanceSettings
{
	GENERATED_BODY()

	// 기본 흔들림이 최종 중심 입력에 더해지는 최대 크기
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Balance", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float BaseInstabilityAmplitude = 0.25f;

	// 기본 흔들림이 1초에 반복되는 횟수
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Balance", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "3.0"))
	float BaseInstabilityFrequency = 0.75f;

	// 기본 중심 흔들림이 플레이어 캡슐을 좌우로 밀어내는 가속도
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Balance | Movement", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "2000.0"))
	float BalanceDriftAcceleration = 420.0f;

	// 플레이어 입력이 라인 중심으로 되돌리기 위해 적용하는 보정 가속도
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Balance | Movement", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "3000.0"))
	float PlayerCorrectionAcceleration = 720.0f;

	// 중심 흔들림으로 발생하는 최대 좌우 이동 속도
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Balance | Movement", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1000.0"))
	float BalanceMaxSpeed = 220.0f;

	// 입력이 없거나 방향이 바뀔 때 과속과 잔진동을 줄이는 감쇠 속도
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Balance | Movement", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "20.0"))
	float BalanceCenteringDamping = 4.0f;

	// 개발 빌드에서 기본 흔들림/플레이어 보정/최종 중심 입력을 화면에 표시할지 여부
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Balance | Debug")
	bool bDrawBalanceDebug = false;
};

/**
 * 화면 터치/Space 입력으로 실행하는 수직 점프 설정
 */
USTRUCT(BlueprintType)
struct FPGJumpSettings
{
	GENERATED_BODY()

	// 점프가 도달할 목표 높이. 실제 초기 Z 속도는 중력값을 기준으로 계산한다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Jump", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "300.0"))
	float JumpHeight = 150.0f;

	// 연속 점프를 막기 위한 쿨타임
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Jump", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "3.0"))
	float JumpCooldownDuration = 1.0f;

	// 공중 상태에서 플레이어 좌우 보정 반응을 강화하는 배율
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Jump | Airborne Control", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "5.0"))
	float AirborneBalanceControlScale = 2.5f;
};

/**
 * 모바일 기울임 입력에서 중심 보정에 사용할 센서 축
 */
UENUM(BlueprintType)
enum class EPGTiltInputAxis : uint8
{
	X UMETA(DisplayName = "X"),
	Y UMETA(DisplayName = "Y"),
	Z UMETA(DisplayName = "Z")
};

/**
 * 모바일 자이로 중심 제어 설정
 */
USTRUCT(BlueprintType)
struct FPGGyroSettings
{
	GENERATED_BODY()

	// 중심 보정 입력으로 사용할 모바일 기울임 센서 축
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Gyro")
	EPGTiltInputAxis TiltInputAxis = EPGTiltInputAxis::X;

	// 자이로 좌우 기울기 입력 민감도
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Gyro", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "5.0"))
	float TiltSensitivity = 4.0f;

	// 정규화된 자이로 입력의 최대 절댓값
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Gyro", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float MaxTiltInput = 1.0f;

	// 캘리브레이션 기준값으로 보간되는 속도
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Gyro", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "20.0"))
	float CalibrationBlendSpeed = 8.0f;

	// 착지 직후 조작 안정화를 보조하는 시간
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Gyro", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "0.5"))
	float LandingStabilizeTime = 0.15f;

	// 기기별 센서 축이 반대로 들어오는 경우 좌우 기울기 입력을 반전한다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Gyro")
	bool bInvertTiltInput = false;
};

/**
 * 추적 카메라 기본 자세 설정
 */
USTRUCT(BlueprintType)
struct FPGCameraSettings
{
	GENERATED_BODY()

	// SpringArm 초기 피치 각도
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "-89.0", ClampMax = "0.0", UIMin = "-70.0", UIMax = "-10.0"))
	float InitialPitch = -40.0f;

	// SpringArm 소켓 오프셋
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Camera")
	FVector SocketOffset = FVector(0.0f, 15.0f, 50.0f);

	// 카메라 위치 래그 속도
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "30.0"))
	float LagSpeed = 12.0f;

	// 초기 카메라 암 길이
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "100.0", UIMin = "200.0", UIMax = "800.0"))
	float DefaultArmLength = 400.0f;

	// 기본 시야각
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "30.0", ClampMax = "130.0", UIMin = "60.0", UIMax = "110.0"))
	float DefaultFOV = 90.0f;
};

/**
 * 러닝 중 진행 거리와 거리 기반 점수 계산 설정
 */
USTRUCT(BlueprintType)
struct FPGRunProgressSettings
{
	GENERATED_BODY()

	// 결과와 HUD에 표시할 거리 갱신 단위. 기본값은 5m 단위 표시다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run Progress | Distance", meta = (ClampMin = "1.0", UIMin = "1.0", UIMax = "20.0"))
	float DistanceUpdateStepMeters = 5.0f;

	// 표시 거리 1m당 획득하는 기본 점수
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run Progress | Score", meta = (ClampMin = "0", UIMin = "0", UIMax = "1000"))
	int32 ScorePerMeter = 10;

	// 거리 기반 점수에 곱하는 배율. 이후 콤보/부스터 시스템이 생기면 이 값을 확장한다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run Progress | Score", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "10.0"))
	float ScoreMultiplier = 1.0f;
};

/**
 * 러닝 중 HUD, Pause 화면, 적재물 리스트 표시 설정입니다.
 */

USTRUCT(BlueprintType)
struct FPGRunHUDSettings
{
	GENERATED_BODY()

	// Running 상태에서 표시할 HUD Widget Blueprint 클래스
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run HUD | UI")
	TSoftClassPtr<UUserWidget> RunHUDWidgetClass;

	// Pause 버튼을 눌렀을 때 표시할 Pause Widget Blueprint 클래스
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run HUD | UI")
	TSoftClassPtr<UUserWidget> RunPauseWidgetClass;

	// 적재물 리스트에 들어가는 개별 카드 Widget Blueprint 클래스
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run HUD | UI")
	TSoftClassPtr<UUserWidget> CargoDeliveryItemWidgetClass;

	// 적재물 배달 완료 시 카드 위치에 표시할 점수 인디케이터 Widget Blueprint 클래스
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run HUD | Cargo")
	TSoftClassPtr<UUserWidget> CargoScoreIndicatorWidgetClass;

	// 적재물 배달 완료 시 적재물 월드 위치에 한 번 재생할 공통 Niagara VFX
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run HUD | Cargo")
	TSoftObjectPtr<UNiagaraSystem> CargoDeliveredVFX;

	// 적재물 배달 완료 시 적재물 월드 위치에 한 번 재생할 공통 사운드
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run HUD | Cargo")
	TSoftObjectPtr<USoundBase> CargoDeliveredSound;

	// 적재물 남은 시간과 진행 바를 갱신하는 간격
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run HUD | Cargo", meta = (ClampMin = "0.03", UIMin = "0.03", UIMax = "0.5"))
	float CargoListRefreshInterval = 0.1f;

	// Pause 화면의 배경 블러 강도. 인게임 시야를 약하게만 흐리게 둔다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run HUD | Pause", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "12.0"))
	float PauseBackdropBlurStrength = 4.0f;

	// Pause 화면의 어두운 딤 알파값
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run HUD | Pause", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "0.8"))
	float PauseBackdropDimAlpha = 0.28f;
};

/**
 * Android 실기기 품질 확인용 독립 디버그 UI 설정입니다.
 */
USTRUCT(BlueprintType)
struct FPGDebugUISettings
{
	GENERATED_BODY()

	// Settings에서 켰을 때 표시할 FPS Widget Blueprint 클래스
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Debug UI | FPS")
	TSoftClassPtr<UUserWidget> FPSDisplayWidgetClass;
};

/**
 * 홈 화면과 3-2-1-Go 카운트다운 시작 흐름 설정
 */
USTRUCT(BlueprintType)
struct FPGHomeFlowSettings
{
	GENERATED_BODY()

	// 앱 실행 직후 표시할 홈 화면 위젯 클래스
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Home Flow | UI")
	TSoftClassPtr<UUserWidget> HomeScreenWidgetClass;

	// START DELIVERY 이후 표시할 3-2-1-Go 카운트다운 위젯 클래스
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Home Flow | UI")
	TSoftClassPtr<UUserWidget> RunCountdownWidgetClass;

	// 홈 화면 Settings 버튼으로 표시할 설정 위젯 클래스
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Home Flow | UI")
	TSoftClassPtr<UUserWidget> SettingsScreenWidgetClass;

	// 홈 화면 Help 버튼으로 표시할 튜토리얼 위젯 클래스
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Home Flow | UI")
	TSoftClassPtr<UUserWidget> TutorialScreenWidgetClass;

	// Help 튜토리얼에서 책처럼 넘겨 볼 설명 이미지 목록
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Home Flow | Tutorial")
	TSoftObjectPtr<UPGTutorialPageSet> TutorialPageSet;

	// 홈 화면에서 사용할 CameraActor 태그
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Home Flow | Camera")
	FName HomeCameraTag = TEXT("HomeCamera");

	// 홈 카메라에서 러너 카메라로 전환하는 블렌드 시간
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Home Flow | Camera", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "3.0"))
	float HomeToRunnerCameraBlendTime = 0.0f;

	// START DELIVERY 이후 홈 UI가 화면 밖으로 빠지는 시간
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Home Flow | Animation", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "2.0"))
	float HomeExitAnimationDuration = 0.35f;

	// 3-2-1-Go 카운트다운 전체 시간
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Home Flow | Countdown", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "10.0"))
	float RunStartCountdownDuration = 3.0f;
};

/**
 * Home 상점 화면과 캐릭터 스킨 카탈로그 설정
 */
USTRUCT(BlueprintType)
struct FPGShopSettings
{
	GENERATED_BODY()

	// Home Shop 버튼으로 표시할 상점 위젯 클래스
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Shop | UI")
	TSoftClassPtr<UUserWidget> ShopScreenWidgetClass;

	// 상점 캐릭터 리스트에 사용할 카드 위젯 클래스
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Shop | UI")
	TSoftClassPtr<UUserWidget> CharacterShopItemWidgetClass;

	// 판매할 캐릭터 스킨 목록과 기본 캐릭터를 담은 카탈로그
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Shop | Character")
	TSoftObjectPtr<UPGCharacterShopCatalog> CharacterShopCatalog;

	// 상점 화면에서 사용할 CameraActor 태그
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Shop | Camera")
	FName ShopCameraTag = TEXT("ShopCamera");

	// HomeCamera에서 ShopCamera로 전환하는 블렌드 시간
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Shop | Camera", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "3.0"))
	float HomeToShopCameraBlendTime = 0.45f;

	// ShopCamera에서 HomeCamera로 복귀하는 블렌드 시간
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Shop | Camera", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "3.0"))
	float ShopToHomeCameraBlendTime = 0.35f;
};

/**
 * 홈/러닝 배경음 재생과 전환 페이드 설정
 */
USTRUCT(BlueprintType)
struct FPGMusicSettings
{
	GENERATED_BODY()

	// Home 상태에서 반복 재생할 배경음
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Music")
	TSoftObjectPtr<USoundBase> HomeMusic;

	// Countdown 시작부터 Running 중 반복 재생할 배경음
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Music")
	TSoftObjectPtr<USoundBase> RunMusic;

	// Home/Run BGM 전환과 실패 페이드아웃에 사용할 시간
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Music", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "5.0"))
	float MusicFadeDuration = 0.6f;

	// 일반 상태의 BGM 볼륨
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Music", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "2.0"))
	float MusicVolume = 1.0f;

	// Pause 중 Run BGM을 낮출 목표 볼륨
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Music | Pause", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0"))
	float PauseDuckedVolume = 0.35f;

	// Pause 진입/복귀 시 볼륨 보간 시간
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Music | Pause", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "2.0"))
	float PauseDuckFadeDuration = 0.2f;
};

/**
 * 공통 UI/SFX 연출에 사용할 사운드 설정입니다.
 */
USTRUCT(BlueprintType)
struct FPGSFXSettings
{
	GENERATED_BODY()

	// 모든 기본 UI 버튼 클릭에 공통으로 사용할 사운드
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "SFX | UI")
	TSoftObjectPtr<USoundBase> CommonButtonClickSound;

	// 3-2-1-Go 카운트다운 단계가 바뀔 때 재생할 사운드
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "SFX | Countdown")
	TSoftObjectPtr<USoundBase> CountdownTickSound;

	// GO! 단계에서 카운트다운 사운드에 곱할 볼륨 배율
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "SFX | Countdown", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "3.0"))
	float CountdownGoVolumeMultiplier = 1.35f;

	// 상점 구매 성공 시 재생할 사운드
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "SFX | Shop")
	TSoftObjectPtr<USoundBase> ShopPurchaseSuccessSound;

	// 상점 구매 실패 또는 재화 부족 시 재생할 사운드
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "SFX | Shop")
	TSoftObjectPtr<USoundBase> ShopPurchaseFailSound;

	// 남자 캐릭터가 점프를 시작할 때 랜덤으로 재생할 사운드 목록
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "SFX | Runner")
	TArray<TSoftObjectPtr<USoundBase>> MaleJumpSounds;

	// 여자 캐릭터가 점프를 시작할 때 랜덤으로 재생할 사운드 목록
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "SFX | Runner")
	TArray<TSoftObjectPtr<USoundBase>> FemaleJumpSounds;

	// CargoDrop Hold가 끝나고 실제 물리 적재물로 전환될 때 재생할 사운드
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "SFX | Cargo")
	TSoftObjectPtr<USoundBase> CargoDropSound;
};

/**
 * 모바일 런타임 품질 프리셋 종류
 */
UENUM(BlueprintType)
enum class EPGMobileQualityPreset : uint8
{
	Low UMETA(DisplayName = "Low"),
	Balanced UMETA(DisplayName = "Balanced"),
	High UMETA(DisplayName = "High"),
	Ultra UMETA(Hidden, DisplayName = "Deprecated Ultra")
};

/**
 * 모바일 품질 프리셋 하나가 적용하는 런타임 그래픽 설정
 */
USTRUCT(BlueprintType)
struct FPGMobileQualityPresetSettings
{
	GENERATED_BODY()

	// 내부 렌더 해상도 비율. 모바일 자글거림과 GPU 비용에 가장 직접적으로 영향을 준다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Mobile Quality", meta = (ClampMin = "50.0", ClampMax = "100.0", UIMin = "50.0", UIMax = "100.0"))
	float ResolutionScalePercent = 90.0f;

	// 모바일 프레임 제한. 0이면 엔진 기본값을 사용한다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Mobile Quality", meta = (ClampMin = "0", UIMin = "0", UIMax = "60"))
	int32 FrameRateLimit = 60;

	// 모바일 안티앨리어싱 방식 CVar 값. 기본값 2는 FXAA보다 외곽 자글거림을 줄이는 쪽으로 둔다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Mobile Quality", meta = (ClampMin = "0", ClampMax = "3", UIMin = "0", UIMax = "3"))
	int32 MobileAntiAliasingMode = 2;

	// Scalability AntiAliasing 품질 단계
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Mobile Quality", meta = (ClampMin = "0", ClampMax = "3", UIMin = "0", UIMax = "3"))
	int32 AntiAliasingQuality = 2;

	// Scalability Texture 품질 단계
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Mobile Quality", meta = (ClampMin = "0", ClampMax = "3", UIMin = "0", UIMax = "3"))
	int32 TextureQuality = 2;

	// Scalability Shadow 품질 단계
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Mobile Quality", meta = (ClampMin = "0", ClampMax = "3", UIMin = "0", UIMax = "3"))
	int32 ShadowQuality = 1;

	// Scalability PostProcess 품질 단계
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Mobile Quality", meta = (ClampMin = "0", ClampMax = "3", UIMin = "0", UIMax = "3"))
	int32 PostProcessQuality = 1;

	// Scalability Effects 품질 단계
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Mobile Quality", meta = (ClampMin = "0", ClampMax = "3", UIMin = "0", UIMax = "3"))
	int32 EffectsQuality = 1;

	// Scalability ViewDistance 품질 단계
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Mobile Quality", meta = (ClampMin = "0", ClampMax = "3", UIMin = "0", UIMax = "3"))
	int32 ViewDistanceQuality = 1;

	// 사선 텍스처 선명도. 너무 높이면 저사양 GPU 비용이 증가한다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Mobile Quality", meta = (ClampMin = "1", ClampMax = "8", UIMin = "1", UIMax = "8"))
	int32 MaxAnisotropy = 4;
};

/**
 * 홈 Settings 화면과 GameUserSettings에서 공유하는 모바일 품질 기본값
 */
USTRUCT(BlueprintType)
struct FPGMobileQualitySettings
{
	GENERATED_BODY()

	// 새 유저 또는 설정 파일이 없는 경우 적용할 기본 프리셋
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Mobile Quality")
	EPGMobileQualityPreset DefaultPreset = EPGMobileQualityPreset::High;

	// 저전력/저사양 기기용 프리셋
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Mobile Quality")
	FPGMobileQualityPresetSettings Low;

	// 기본 권장 프리셋
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Mobile Quality")
	FPGMobileQualityPresetSettings Balanced;

	// 선명도 우선 프리셋
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Mobile Quality")
	FPGMobileQualityPresetSettings High;

};

/**
 * 런 실패 후 결과 화면에 사용할 로컬 UI/SFX/카메라 연출 설정
 */
USTRUCT(BlueprintType)
struct FPGRunResultSettings
{
	GENERATED_BODY()

	// 런 실패 시 자동으로 표시할 게임오버/결과 위젯 클래스
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run Result | UI")
	TSoftClassPtr<UPGRunResultWidget> GameOverWidgetClass;

	// 런 실패 시 한 번 재생할 사운드
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run Result | SFX")
	TSoftObjectPtr<USoundBase> GameOverSound;

	// 런 실패 시 러너 위치에 한 번 재생할 Niagara VFX
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run Result | VFX")
	TSoftObjectPtr<UNiagaraSystem> GameOverVFX;

	// 런 실패 시 로컬 플레이어 카메라에 적용할 카메라 쉐이크
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run Result | Camera")
	TSoftClassPtr<UCameraShakeBase> GameOverCameraShake;
};

/**
 * 플레이어 기준 물리 적재물 스폰/손실 판정 설정
 */
USTRUCT(BlueprintType)
struct FPGCargoSettings
{
	GENERATED_BODY()

	// CargoDropSet이 비어 있거나 후보를 고르지 못했을 때 사용할 fallback 적재물 타입
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo")
	TSoftObjectPtr<UPGCargoTypeDefinition> DefaultCargoType;

	// 적재물이 CargoStackComponent 위치에서 대기한 뒤 실제 물리 적재물로 전환되기까지의 시간
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo | Spawn", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "5.0"))
	float CargoDropHoldDuration = 3.0f;

	// 적재물 물리 오브젝트가 사용할 충돌 프로파일
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo | Physics")
	FName CargoCollisionProfileName = TEXT("PhysicsActor");

	// 중심 불균형이 적재물에 직접 가하는 보조 물리 힘. 실제 물리 충돌만으로 부족할 때 체감 흔들림을 보강한다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo | Physics", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1000.0"))
	float CargoBalanceForce = 160.0f;

	// 직선 주행 중 적재물에 가하는 약한 흔들림 힘
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo | Physics", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "500.0"))
	float CargoIdleShakeForce = 25.0f;

	// 직선 주행 흔들림 힘의 반복 빈도
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo | Physics", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "5.0"))
	float CargoIdleShakeFrequency = 1.15f;

	// 점프 또는 트랙 높이 차이로 공중에 떠 있는 동안 적재물 이탈을 줄이기 위해 상대속도 제한을 유지하는 시간
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo | Physics", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0"))
	float AirborneCargoAssistDuration = 0.2f;

	// 공중 보정 중 CargoStack 앵커 기준 적재물 최대 상대속도
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo | Physics", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1000.0"))
	float AirborneCargoMaxRelativeSpeed = 180.0f;

	// 공중 보정 중 CargoStack 앵커 기준 적재물 최대 위쪽 상대속도
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo | Physics", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "500.0"))
	float AirborneCargoMaxUpwardRelativeSpeed = 60.0f;

	// 플레이어 Actor 위치 기준 이 높이보다 아래로 적재물이 떨어지면 손실로 처리한다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo | Loss", meta = (ClampMin = "10.0", UIMin = "50.0", UIMax = "500.0"))
	float CargoLossZOffset = 140.0f;

	// 플레이어 Actor 위치 기준 이 수평 거리보다 멀어지면 적재물 손실로 처리한다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo | Loss", meta = (ClampMin = "10.0", UIMin = "50.0", UIMax = "500.0"))
	float CargoLossHorizontalDistance = 180.0f;

	// 개발 빌드에서 CargoStack 생성 앵커, 플레이어 기준 손실 반경, 적재물 위치를 표시할지 여부
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo | Debug")
	bool bDrawCargoDebug = false;
};

/**
 * 배달형 적재물 드롭 스케줄 설정
 */
USTRUCT(BlueprintType)
struct FPGCargoDeliverySettings
{
	GENERATED_BODY()

	// 러닝 중 랜덤으로 드롭할 적재물 후보 목록
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo Delivery")
	TSoftObjectPtr<UPGCargoDropSet> DefaultCargoDropSet;

	// Running 진입 후 첫 적재물 드롭까지 기다리는 시간. 0이면 즉시 드롭을 시도한다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo Delivery | Schedule", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "30.0"))
	float FirstDropDelay = 0.0f;

	// 난이도 0일 때 적재물 드롭 간격
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo Delivery | Schedule", meta = (ClampMin = "0.5", UIMin = "2.0", UIMax = "60.0"))
	float BaseDropInterval = 14.0f;

	// 난이도 1일 때 적재물 드롭 간격
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo Delivery | Schedule", meta = (ClampMin = "0.5", UIMin = "1.0", UIMax = "30.0"))
	float MinDropInterval = 6.0f;

	// 적재물 드롭 간격이 최소값에 가까워지는 시간
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo Delivery | Difficulty", meta = (ClampMin = "1.0", UIMin = "30.0", UIMax = "300.0"))
	float DifficultyRampDuration = 180.0f;

	// 같은 적재물 드롭 순서를 재현할 때 사용하는 시드
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo Delivery | Random")
	int32 RandomSeed = 4117;

	// 켜면 PIE/Game 시작마다 새 랜덤 시드를 사용한다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo Delivery | Random")
	bool bUseRandomSeedEachRun = true;

	// 개발 빌드에서 이 개수 이상 물리 적재물이 쌓이면 튜닝 경고를 출력한다. 제한은 걸지 않는다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo Delivery | Debug", meta = (ClampMin = "1", UIMin = "3", UIMax = "50"))
	int32 ActiveCargoWarningCount = 12;

	// 개발 빌드에서 드롭 스케줄과 활성 적재 수를 표시할지 여부
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo Delivery | Debug")
	bool bDrawCargoDeliveryDebug = false;
};

/**
 * 바람, 눈바람, 적재 증가 같은 환경 이벤트의 기본 반응 설정
 */
USTRUCT(BlueprintType)
struct FPGEnvironmentSettings
{
	GENERATED_BODY()

	// 바람 강도가 중심 입력 편향으로 변환될 때 곱하는 기본 비율
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Environment | Wind", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0"))
	float WindBalanceBiasScale = 0.35f;

	// 눈바람 중 플레이어 기울임 입력을 증폭하는 비율. 입력이 0이면 추가 효과도 없다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Environment | Snowstorm", meta = (ClampMin = "1.0", UIMin = "1.0", UIMax = "6.0"))
	float SnowstormInputSensitivityScale = 2.5f;

	// 눈바람 중 플레이어 입력이 적재물 흔들림 힘으로 변환될 때 추가로 곱하는 비율
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Environment | Snowstorm", meta = (ClampMin = "1.0", UIMin = "1.0", UIMax = "8.0"))
	float SnowstormCargoInputForceScale = 3.0f;

	// 개발 빌드에서 현재 환경 효과 상태를 디버그 텍스트로 표시할지 여부
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Environment | Debug")
	bool bDrawEnvironmentDebug = false;
};

/**
 * 러닝 시간 기반 전역 이벤트 예약/발동 설정
 */
USTRUCT(BlueprintType)
struct FPGRunEventSettings
{
	GENERATED_BODY()

	// 러닝 중 사용할 전역 이벤트 규칙 DataAsset. 비어 있으면 C++ 기본 규칙을 사용한다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run Event")
	TSoftObjectPtr<UPGRunEventSet> DefaultRunEventSet;

	// 이벤트별 UI/VFX/SFX 리소스를 담은 기본 연출 DataAsset
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run Event")
	TSoftObjectPtr<UPGRunEventPresentationSet> DefaultPresentationSet;

	// 이벤트 난이도가 1.0까지 도달하는 시간
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run Event | Difficulty", meta = (ClampMin = "1.0", UIMin = "30.0", UIMax = "300.0"))
	float DifficultyRampDuration = 180.0f;

	// 시작 후 이 시간 동안은 자동 이벤트를 예약하지 않는다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run Event | Schedule", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "60.0"))
	float SafeInitialRunTime = 10.0f;

	// 난이도 0일 때 이벤트 사이 기본 간격
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run Event | Schedule", meta = (ClampMin = "0.5", UIMin = "2.0", UIMax = "30.0"))
	float BaseEventInterval = 9.0f;

	// 난이도 1일 때 이벤트 사이 최소 간격
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run Event | Schedule", meta = (ClampMin = "0.5", UIMin = "2.0", UIMax = "30.0"))
	float MinEventInterval = 4.0f;

	// 이벤트 발동 전 자동 전조 연출이 시작되는 시간
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run Event | Schedule", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "5.0"))
	float PreviewLeadTime = 1.5f;

	// 같은 이벤트 순서를 재현할 때 사용하는 시드
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run Event | Random")
	int32 RandomSeed = 7331;

	// 켜면 PIE/Game 시작마다 새 랜덤 시드를 사용한다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run Event | Random")
	bool bUseRandomSeedEachRun = true;

	// 개발 빌드에서 이벤트 예약/발동 상태를 화면에 표시할지 여부
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run Event | Debug")
	bool bDrawRunEventDebug = false;
};

/**
 * 프로젝트 전역 설정을 에디터(Project Settings)에서 관리하기 위한 클래스
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Go Deliver! Settings"))
class PROJECTG_API UPGGameSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPGGameSettings();

	// 자동 전진 캐릭터 기본 설정
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Runner")
	FPGRunnerSettings Runner;

	// 직선 주행 중 기본 중심 불안정성 설정
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Balance")
	FPGBalanceSettings Balance;

	// 화면 터치/Space 수직 점프 설정
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Jump")
	FPGJumpSettings Jump;

	// 모바일 자이로 중심 제어 설정
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Gyro")
	FPGGyroSettings Gyro;

	// 추적 카메라 기본 설정
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Camera")
	FPGCameraSettings Camera;

	// 런 실패 후 결과 화면 자동 표시 설정
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run Result")
	FPGRunResultSettings RunResult;

	// 홈 화면과 러닝 시작 카운트다운 설정
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Home Flow")
	FPGHomeFlowSettings HomeFlow;

	// 홈/러닝 배경음 설정
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Music")
	FPGMusicSettings Music;

	// 공통 UI 클릭음, 카운트다운, 상점 결과음 설정
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "SFX")
	FPGSFXSettings SFX;

	// Home 상점과 캐릭터 스킨 카탈로그 설정
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Shop")
	FPGShopSettings Shop;

	// 모바일 런타임 품질 프리셋 기본값
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Mobile Quality")
	FPGMobileQualitySettings MobileQuality;

	// 러닝 중 거리와 점수 진행 설정
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run Progress")
	FPGRunProgressSettings RunProgress;

	// 러닝 중 HUD와 Pause 화면 설정
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run HUD")
	FPGRunHUDSettings RunHUD;

	// Android 실기기 품질 확인용 독립 디버그 UI 설정
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Debug UI")
	FPGDebugUISettings DebugUI;

	// 플레이어 기준 물리 적재물 스폰/손실 판정 설정
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo")
	FPGCargoSettings Cargo;

	// 배달형 적재물 드롭 스케줄 설정
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo Delivery")
	FPGCargoDeliverySettings CargoDelivery;

	// 바람, 눈바람, 적재 증가 같은 환경 이벤트 설정
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Environment")
	FPGEnvironmentSettings Environment;

	// 러닝 시간 기반 전역 이벤트 예약/발동 설정
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run Event")
	FPGRunEventSettings RunEvent;
};
