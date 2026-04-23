// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Settings/PGGameSettings.h"
#include "PGRunnerCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;

/**
 * Go Deliver!의 자동 전진 러너 캐릭터
 */
UCLASS()
class PROJECTG_API APGRunnerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	APGRunnerCharacter();

	// 매 프레임 자동 전진, 라인 이동, 몸 기울기, 디버그 표시를 갱신한다.
	virtual void Tick(float DeltaSeconds) override;

	// 시작 위치를 기준으로 러너 진행 축, 중앙 라인, 초기 메시 상태를 저장한다.
	virtual void BeginPlay() override;

	// 입력 방향(-1: 왼쪽, 1: 오른쪽)에 따라 인접 라인 이동을 요청한다.
	void RequestLaneChange(int32 Direction);

	// 자이로/키보드에서 계산된 중심 입력값을 캐릭터 기울기 반응에 반영한다.
	void SetBalanceInput(float InBalanceInput);

	// 현재 캐릭터가 위치한 라인 인덱스
	UFUNCTION(BlueprintPure, Category = "ProjectG | Runner")
	int32 GetCurrentLaneIndex() const { return CurrentLaneIndex; }

	// 라인 이동 중 목표로 삼고 있는 라인 인덱스
	UFUNCTION(BlueprintPure, Category = "ProjectG | Runner")
	int32 GetTargetLaneIndex() const { return TargetLaneIndex; }

	// 현재 적용 중인 좌우 중심 입력값
	UFUNCTION(BlueprintPure, Category = "ProjectG | Runner")
	float GetBalanceInput() const { return BalanceInput; }

protected:
	// 캐릭터를 약간 위에서 따라가는 추적 카메라 암
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	// 플레이어가 실제로 바라보는 추적 카메라
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

private:
	// Project Settings의 Go Deliver 설정값을 시작 시 캐시하고 런타임 컴포넌트에 반영한다.
	void ApplyGameSettings();

	// 러너 기준 전방 축으로 매 프레임 자동 전진 입력을 넣는다.
	void UpdateForwardMovement();

	// 러너 기준 오른쪽 축으로 라인 변경을 보간하고 메시 점프 연출을 갱신한다.
	void UpdateLaneMovement(float DeltaSeconds);

	// 중심 입력값을 캐릭터 메시의 좌우 기울기 연출로 변환한다.
	void UpdateBodyLean(float DeltaSeconds);

	// 디버그 옵션이 켜져 있을 때 러너 기준 3라인 위치를 월드에 표시한다.
	void DrawDebugLanes() const;

	// 지정한 라인 인덱스로 이동을 시작한다.
	void StartLaneChange(int32 NewLaneIndex);

	// 라인 이동 중 입력된 다음 이동 요청을 처리한다.
	void ConsumeBufferedLaneInput();

	// 현재 라인 개수 기준 중앙 라인 인덱스를 반환한다.
	int32 GetCenterLaneIndex() const;

	// 라인 인덱스가 유효 범위를 벗어나지 않도록 제한한다.
	int32 ClampLaneIndex(int32 LaneIndex) const;

	// 중앙 라인을 기준으로 특정 라인의 좌우 오프셋을 계산한다.
	float GetLaneOffset(int32 LaneIndex) const;

	// 현재 캡슐 위치가 중앙 라인 기준으로 얼마나 오른쪽에 있는지 계산한다.
	float GetCurrentLaneOffset() const;

	// 현재 전방 진행 거리를 유지한 채 지정한 좌우 오프셋의 월드 위치를 계산한다.
	FVector GetLocationAtLaneOffset(float LaneOffset) const;

	// 캡슐 Z 이동 없이 메시 상대 위치만 올려 라인 점프감을 만든다.
	void SetMeshJumpOffset(float JumpOffset);

	// BeginPlay에서 캐시한 자동 전진/몸 기울기 설정
	FPGRunnerSettings CachedRunnerSettings;

	// BeginPlay에서 캐시한 3라인 이동 설정
	FPGLaneSettings CachedLaneSettings;

	// BeginPlay에서 캐시한 자이로 중심 제어 설정
	FPGGyroSettings CachedGyroSettings;

	// BeginPlay에서 캐시한 카메라 설정
	FPGCameraSettings CachedCameraSettings;

	// BeginPlay에서 캐시한 에디터 디버그 설정
	FPGDebugSettings CachedDebugSettings;

	// 현재 실제 위치한 라인 인덱스
	int32 CurrentLaneIndex = 0;

	// 라인 이동 중 도착해야 할 목표 라인 인덱스
	int32 TargetLaneIndex = 0;

	// 라인 이동 중 미리 입력된 다음 이동 방향
	int32 BufferedLaneDirection = 0;

	// 시작 시 중앙 라인으로 잡은 월드 위치
	FVector LaneOriginLocation = FVector::ZeroVector;

	// 시작 시 러너가 바라보던 수평 전방 축
	FVector RunnerForwardVector = FVector::ForwardVector;

	// 시작 시 러너의 수평 오른쪽 축
	FVector RunnerRightVector = FVector::RightVector;

	// 라인 이동 시작 시점의 중앙 라인 기준 좌우 오프셋
	float LaneStartOffset = 0.0f;

	// 라인 이동 목표 지점의 중앙 라인 기준 좌우 오프셋
	float LaneTargetOffset = 0.0f;

	// 현재 라인 이동이 진행된 시간
	float LaneMoveElapsed = 0.0f;

	// 버퍼링된 라인 입력이 유효한 남은 시간
	float BufferedLaneInputRemainingTime = 0.0f;

	// 착지 직후 자이로 입력을 완화하는 남은 안정화 시간
	float LandingStabilizeRemainingTime = 0.0f;

	// 현재 좌우 중심 입력값
	float BalanceInput = 0.0f;

	// 메시 연출에 적용 중인 현재 롤 각도
	float CurrentBodyRoll = 0.0f;

	// 라인 이동 점프가 진행 중인지 여부
	bool bIsChangingLane = false;

	// 몸 기울기 연출을 더하기 전 Skeletal Mesh의 초기 상대 회전
	FRotator InitialMeshRelativeRotation = FRotator::ZeroRotator;

	// 점프 연출을 더하기 전 Skeletal Mesh의 초기 상대 위치
	FVector InitialMeshRelativeLocation = FVector::ZeroVector;
};
