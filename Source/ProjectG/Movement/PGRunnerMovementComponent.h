// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Settings/PGGameSettings.h"
#include "PGRunnerMovementComponent.generated.h"

/**
 * Go Deliver! 러너의 자동 전진 축과 3라인 이동 상태를 관리하는 전용 이동 컴포넌트
 */
UCLASS()
class PROJECTG_API UPGRunnerMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UPGRunnerMovementComponent();

	// Project Settings에서 캐시한 설정과 시작 트랙 축을 받아 라인/밸런스 이동 상태를 초기화한다.
	void InitializeRunnerMovement(const FPGLaneSettings& InLaneSettings, const FPGGyroSettings& InGyroSettings, const FPGBalanceSettings& InBalanceSettings, const FVector& InLaneOriginLocation, const FVector& InRunnerForwardVector, const FVector& InRunnerRightVector);

	// 입력 방향(-1: 왼쪽, 1: 오른쪽)에 따라 인접 라인 이동을 요청한다.
	void RequestLaneChange(int32 Direction);

	// 현재 캐릭터가 위치한 라인 인덱스
	UFUNCTION(BlueprintPure, Category = "ProjectG | Runner Movement")
	int32 GetCurrentLaneIndex() const { return CurrentLaneIndex; }

	// 라인 이동 중 목표로 삼고 있는 라인 인덱스
	UFUNCTION(BlueprintPure, Category = "ProjectG | Runner Movement")
	int32 GetTargetLaneIndex() const { return TargetLaneIndex; }

	// 라인 이동이 진행 중인지 여부
	UFUNCTION(BlueprintPure, Category = "ProjectG | Runner Movement")
	bool IsChangingLane() const { return bIsChangingLane || bIsWaitingForLaneJumpLanding; }

	// 착지 직후 자이로 입력을 완화해야 하는 남은 시간
	UFUNCTION(BlueprintPure, Category = "ProjectG | Runner Movement")
	float GetLandingStabilizeRemainingTime() const { return LandingStabilizeRemainingTime; }

	// 러너 기준 수평 전방 축
	FVector GetRunnerForwardVector() const { return RunnerForwardVector; }

	// 러너 기준 수평 오른쪽 축
	FVector GetRunnerRightVector() const { return RunnerRightVector; }

	// 지정 라인의 현재 전방 진행 거리 기준 월드 위치를 계산한다.
	FVector GetLaneWorldLocation(int32 LaneIndex) const;

	// 기본 흔들림과 플레이어 보정 입력을 받아 실제 캡슐 좌우 밀림에 반영한다.
	void SetBalanceMovementInput(float InBaseInstabilityInput, float InPlayerCorrectionInput);

	// 현재 라인 중심에서 실제 캡슐이 밀려난 좌우 거리
	UFUNCTION(BlueprintPure, Category = "ProjectG | Runner Movement")
	float GetBalanceOffset() const { return BalanceOffset; }

	// 현재 밸런스 흔들림으로 발생 중인 좌우 속도
	UFUNCTION(BlueprintPure, Category = "ProjectG | Runner Movement")
	float GetBalanceVelocity() const { return BalanceVelocity; }

protected:
	// CharacterMovement의 기본 이동 이후 라인 이동 보간을 같은 Movement 계층에서 처리한다.
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	// 라인 변경 진행도와 수평 캡슐 이동을 갱신한다.
	void UpdateLaneMovement(float DeltaSeconds);

	// 줄타기 중심 흔들림을 실제 라인 중심 기준 좌우 오프셋으로 갱신한다.
	void UpdateBalanceMovement(float DeltaSeconds);

	// 지정한 라인 인덱스로 이동을 시작한다.
	void StartLaneChange(int32 NewLaneIndex);

	// 라인 점프가 실제 바닥에 착지했을 때 안정화 시간과 버퍼 입력을 처리한다.
	void CompleteLaneJumpLanding();

	// 설정된 라인 점프 높이에 도달하기 위한 초기 Z 속도를 계산한다.
	float CalculateLaneJumpVelocity() const;

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

	// BeginPlay에서 캐시한 3라인 이동 설정
	FPGLaneSettings CachedLaneSettings;

	// BeginPlay에서 캐시한 자이로 중심 제어 설정
	FPGGyroSettings CachedGyroSettings;

	// BeginPlay에서 캐시한 실제 중심 이동 설정
	FPGBalanceSettings CachedBalanceSettings;

	// 시작 시 중앙 라인으로 잡은 월드 위치
	FVector LaneOriginLocation = FVector::ZeroVector;

	// 시작 시 러너가 바라보던 수평 전방 축
	FVector RunnerForwardVector = FVector::ForwardVector;

	// 시작 시 러너의 수평 오른쪽 축
	FVector RunnerRightVector = FVector::RightVector;

	// 현재 실제 위치한 라인 인덱스
	int32 CurrentLaneIndex = 0;

	// 라인 이동 중 도착해야 할 목표 라인 인덱스
	int32 TargetLaneIndex = 0;

	// 라인 이동 중 미리 입력된 다음 이동 방향
	int32 BufferedLaneDirection = 0;

	// 라인 이동 시작 시점의 중앙 라인 기준 좌우 오프셋
	float LaneStartOffset = 0.0f;

	// 라인 이동 목표 지점의 중앙 라인 기준 좌우 오프셋
	float LaneTargetOffset = 0.0f;

	// 현재 라인 중심에서 실제 플레이어 캡슐이 밀려난 좌우 거리
	float BalanceOffset = 0.0f;

	// 밸런스 흔들림으로 발생 중인 좌우 속도
	float BalanceVelocity = 0.0f;

	// 기본 중심 불안정성이 밀어내는 입력값
	float BaseInstabilityInput = 0.0f;

	// 플레이어가 라인 중심을 회복하기 위해 넣는 보정 입력값
	float PlayerCorrectionInput = 0.0f;

	// 현재 라인 이동이 진행된 시간
	float LaneMoveElapsed = 0.0f;

	// 버퍼링된 라인 입력이 유효한 남은 시간
	float BufferedLaneInputRemainingTime = 0.0f;

	// 착지 직후 자이로 입력을 완화하는 남은 안정화 시간
	float LandingStabilizeRemainingTime = 0.0f;

	// 라인 이동 점프가 진행 중인지 여부
	bool bIsChangingLane = false;

	// 수평 라인 이동은 끝났지만 실제 캡슐 착지를 기다리는 중인지 여부
	bool bIsWaitingForLaneJumpLanding = false;
};
