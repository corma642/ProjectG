// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "PGGameSettings.generated.h"

class UPGCargoTypeDefinition;
class UPGCargoPresentationSet;
class UPGRunEventPresentationSet;
class UPGRunEventSet;
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

	// 직선 주행 중 기본 좌우 흔들림을 사용할지 여부
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Balance")
	bool bEnableBaseInstability = true;

	// 기본 흔들림이 최종 중심 입력에 더해지는 최대 크기
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Balance", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float BaseInstabilityAmplitude = 0.25f;

	// 기본 흔들림이 1초에 반복되는 횟수
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Balance", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "3.0"))
	float BaseInstabilityFrequency = 0.75f;

	// 플레이어 보정 입력이 기본 흔들림을 상쇄하는 영향 비율
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Balance", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "2.0"))
	float PlayerCorrectionScale = 1.0f;

	// 기본 중심 흔들림이 플레이어 캡슐을 좌우로 밀어내는 가속도
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Balance | Movement", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "2000.0"))
	float BalanceDriftAcceleration = 420.0f;

	// 플레이어 입력이 라인 중심으로 되돌리기 위해 적용하는 보정 가속도
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Balance | Movement", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "3000.0"))
	float PlayerCorrectionAcceleration = 720.0f;

	// 현재 라인 중심에서 허용하는 최대 좌우 이탈 거리
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Balance | Movement", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "300.0"))
	float BalanceMaxOffset = 90.0f;

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
 * 3라인 점프 이동 설정
 */
USTRUCT(BlueprintType)
struct FPGLaneSettings
{
	GENERATED_BODY()

	// 러닝 레인의 개수. 현재 기획 기준은 Left / Center / Right 3라인이다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Lane", meta = (ClampMin = "1", ClampMax = "5", UIMin = "1", UIMax = "5"))
	int32 LaneCount = 3;

	// 인접 라인 사이의 월드 거리
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Lane", meta = (ClampMin = "1.0", UIMin = "100.0", UIMax = "400.0"))
	float LaneWidth = 220.0f;

	// 라인 변경 점프가 완료되는 시간
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Lane", meta = (ClampMin = "0.01", UIMin = "0.05", UIMax = "0.6"))
	float LaneChangeDuration = 0.22f;

	// 라인 변경 중 점프 포물선의 최대 높이
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Lane", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "200.0"))
	float LaneJumpHeight = 80.0f;

	// 라인 이동 중 입력을 잠시 보관하는 시간
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Lane", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "0.5"))
	float LaneInputBufferTime = 0.1f;
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

	// 미세 흔들림을 무시하는 입력 데드존
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Gyro", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "0.2"))
	float TiltDeadZone = 0.01f;

	// 정규화된 자이로 입력의 최대 절댓값
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Gyro", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float MaxTiltInput = 1.0f;

	// 캘리브레이션 기준값으로 보간되는 속도
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Gyro", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "20.0"))
	float CalibrationBlendSpeed = 8.0f;

	// 점프 중 자이로 영향력을 완화하는 비율
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Gyro", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float JumpTiltDampingScale = 0.5f;

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
 * 에디터/개발 빌드에서만 쓰는 확인 보조 설정
 */
USTRUCT(BlueprintType)
struct FPGDebugSettings
{
	GENERATED_BODY()

	// 라인 위치 디버그 표시 여부
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bDrawLaneDebug = false;

	// 자이로 입력 디버그 표시 여부
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bDrawGyroDebug = false;
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

	// 홈 화면에서 사용할 CameraActor 태그
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Home Flow | Camera")
	FName HomeCameraTag = TEXT("HomeCamera");

	// 홈 카메라에서 러너 카메라로 전환하는 블렌드 시간
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Home Flow | Camera", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "3.0"))
	float HomeToRunnerCameraBlendTime = 0.6f;

	// START DELIVERY 이후 홈 UI가 화면 밖으로 빠지는 시간
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Home Flow | Animation", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "2.0"))
	float HomeExitAnimationDuration = 0.35f;

	// 3-2-1-Go 카운트다운 전체 시간
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Home Flow | Countdown", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "10.0"))
	float RunStartCountdownDuration = 3.0f;
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
	TSoftClassPtr<UUserWidget> GameOverWidgetClass;

	// 이전 임시 점수 계산에 사용하던 값입니다. 현재 점수는 RunProgress 설정의 거리 기반 점수를 사용합니다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run Result | Score", meta = (ClampMin = "0", UIMin = "0", UIMax = "1000"))
	int32 ScorePerSecond = 100;

	// 영수증 수집 시스템 도입 전 결과 화면에 표시할 임시 영수증 수
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run Result | Score", meta = (ClampMin = "0", UIMin = "0", UIMax = "99999"))
	int32 TemporaryReceipts = 2366;

	// 주문 시스템 도입 전 결과 화면에 표시할 임시 배달 완료 수
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run Result | Score", meta = (ClampMin = "0", UIMin = "0", UIMax = "999"))
	int32 TemporaryOrdersDelivered = 3;

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

	// 시작/추가 적재물에 사용할 기본 적재물 타입
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo")
	TSoftObjectPtr<UPGCargoTypeDefinition> DefaultCargoType;

	// 게임 시작 시 플레이어 기준 앵커에 생성할 적재물 개수
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo", meta = (ClampMin = "0", UIMin = "0", UIMax = "5"))
	int32 InitialCargoCount = 1;

	// 동시에 유지할 수 있는 최대 적재물 개수
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo", meta = (ClampMin = "1", UIMin = "1", UIMax = "5"))
	int32 MaxCargoCount = 3;

	// 여러 적재물을 시작 배치할 때 CargoStackComponent 위치에서 위로 띄우는 간격
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo | Spawn", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "80.0"))
	float InitialCargoSpacing = 18.0f;

	// 첫 적재물이 물리 Actor로 투입된 직후 손실 판정을 잠시 늦추는 안정화 시간
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo | Intro", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "2.0"))
	float InitialCargoSettleDelay = 0.3f;

	// 추가 적재물이 CargoStackComponent 위치에서 대기한 뒤 실제 물리 적재물로 전환되기까지의 시간
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

	// 라인 점프 중과 착지 직후 적재물 이탈을 줄이기 위해 상대속도 제한을 유지하는 시간
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo | Physics", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0"))
	float LaneJumpCargoAssistDuration = 0.2f;

	// 라인 점프 보정 중 CargoStack 앵커 기준 적재물 최대 상대속도
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo | Physics", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1000.0"))
	float LaneJumpMaxRelativeSpeed = 180.0f;

	// 라인 점프 보정 중 CargoStack 앵커 기준 적재물 최대 위쪽 상대속도
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo | Physics", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "500.0"))
	float LaneJumpMaxUpwardRelativeSpeed = 60.0f;

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
 * 적재물 관련 로컬 UI/VFX/SFX 자동 연출 설정
 */
USTRUCT(BlueprintType)
struct FPGCargoFeedbackSettings
{
	GENERATED_BODY()

	// 추가 적재/손실 이벤트별 UI/VFX/SFX 리소스를 담은 기본 연출 DataAsset
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo Feedback")
	TSoftObjectPtr<UPGCargoPresentationSet> DefaultPresentationSet;
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

	// 3라인 점프 이동 설정
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Lane")
	FPGLaneSettings Lane;

	// 모바일 자이로 중심 제어 설정
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Gyro")
	FPGGyroSettings Gyro;

	// 추적 카메라 기본 설정
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Camera")
	FPGCameraSettings Camera;

	// 에디터 테스트와 디버그 보조 설정
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Debug")
	FPGDebugSettings Debug;

	// 런 실패 후 결과 화면 자동 표시 설정
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run Result")
	FPGRunResultSettings RunResult;

	// 홈 화면과 러닝 시작 카운트다운 설정
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Home Flow")
	FPGHomeFlowSettings HomeFlow;

	// 러닝 중 거리와 점수 진행 설정
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run Progress")
	FPGRunProgressSettings RunProgress;

	// 플레이어 기준 물리 적재물 스폰/손실 판정 설정
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo")
	FPGCargoSettings Cargo;

	// 적재물 추가/손실 로컬 연출 설정
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo Feedback")
	FPGCargoFeedbackSettings CargoFeedback;

	// 바람, 눈바람, 적재 증가 같은 환경 이벤트 설정
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Environment")
	FPGEnvironmentSettings Environment;

	// 러닝 시간 기반 전역 이벤트 예약/발동 설정
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Run Event")
	FPGRunEventSettings RunEvent;
};
