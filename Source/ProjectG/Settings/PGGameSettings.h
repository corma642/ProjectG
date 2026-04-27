// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "PGGameSettings.generated.h"

class UPGCargoTypeDefinition;

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
 * 모바일 자이로 중심 제어 설정
 */
USTRUCT(BlueprintType)
struct FPGGyroSettings
{
	GENERATED_BODY()

	// 자이로 좌우 기울기 입력 민감도
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Gyro", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "5.0"))
	float TiltSensitivity = 1.0f;

	// 미세 흔들림을 무시하는 입력 데드존
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Gyro", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "0.2"))
	float TiltDeadZone = 0.03f;

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

	// 모바일 입력이 없는 에디터 테스트에서 키보드 대체 입력을 허용할지 여부
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bUseKeyboardFallbackInEditor = true;

	// 라인 위치 디버그 표시 여부
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bDrawLaneDebug = false;

	// 자이로 입력 디버그 표시 여부
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bDrawGyroDebug = false;
};

/**
 * 머리 위 적재물 스택과 시각 흔들림 설정
 */
USTRUCT(BlueprintType)
struct FPGCargoSettings
{
	GENERATED_BODY()

	// 에셋이 지정된 경우 시작/추가 적재물에 사용할 기본 적재물 타입
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo")
	TSoftObjectPtr<UPGCargoTypeDefinition> DefaultCargoType;

	// 게임 시작 시 머리 위에 올릴 적재물 개수
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo", meta = (ClampMin = "0", UIMin = "0", UIMax = "5"))
	int32 InitialCargoCount = 1;

	// 동시에 유지할 수 있는 최대 적재물 개수
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo", meta = (ClampMin = "1", UIMin = "1", UIMax = "5"))
	int32 MaxCargoCount = 3;

	// 적재물 사이의 세로 간격
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "80.0"))
	float CargoSlotSpacing = 18.0f;

	// 캐릭터 기준 첫 적재물 위치 오프셋
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo")
	FVector CargoBaseOffset = FVector(0.0f, 0.0f, 120.0f);

	// 중심 입력이 적재물 흔들림으로 표현되는 최대 롤 각도
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo", meta = (ClampMin = "0.0", ClampMax = "45.0", UIMin = "0.0", UIMax = "30.0"))
	float SwayMaxRoll = 10.0f;

	// 적재물 흔들림이 목표 각도로 따라가는 보간 속도
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "20.0"))
	float SwayInterpSpeed = 8.0f;

	// 라인 점프 높이가 적재물 시각 상승에 반영되는 비율
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0"))
	float JumpLiftScale = 0.35f;

	// 메시가 없을 때 적재 위치를 디버그 구체로 표시할지 여부
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo")
	bool bDrawCargoDebug = false;

	// 안정도 값이 이 값 이상이면 경고 상태로 표시한다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo | Stability", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float WarningThreshold = 0.45f;

	// 안정도 값이 이 값 이상이면 위험 상태로 표시한다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo | Stability", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float CriticalThreshold = 0.8f;

	// 안정도 값이 이 값 이상이면 적재물이 손실된다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo | Stability", meta = (ClampMin = "0.01", UIMin = "0.1", UIMax = "2.0"))
	float LossThreshold = 1.0f;

	// 중심 입력 절댓값이 초당 안정도 손상으로 누적되는 비율
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo | Stability", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "3.0"))
	float BalanceDamageRate = 0.65f;

	// 라인 점프를 시작할 때 한 번 추가되는 안정도 손상량
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo | Stability", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0"))
	float JumpDamageAmount = 0.12f;

	// 중심 입력이 안정됐을 때 초당 안정도가 회복되는 비율
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo | Stability", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "2.0"))
	float StabilityRecoveryRate = 0.25f;
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

	// 머리 위 적재물 스택과 시각 흔들림 설정
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cargo")
	FPGCargoSettings Cargo;
};
