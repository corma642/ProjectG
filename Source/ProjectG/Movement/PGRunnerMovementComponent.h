// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Settings/PGGameSettings.h"
#include "PGRunnerMovementComponent.generated.h"

/**
 * 자동 전진, 수직 점프, 좌우 Balance 이동을 담당하는 러너 이동 컴포넌트입니다.
 *
 * 실제 이동 라인 인덱스는 사용하지 않습니다. 러너 시작 기준 전방/우측 축을 기준으로
 * 전진 거리를 보존하고, 기울임 입력과 외부 충격이 만든 BalanceOffset만 좌우 위치에 반영합니다.
 */
UCLASS()
class PROJECTG_API UPGRunnerMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UPGRunnerMovementComponent();

	// 런 시작 기준 위치/축과 Project Settings의 점프, 터치, Balance 설정값을 적용합니다.
	void InitializeRunnerMovement(const FPGJumpSettings& InJumpSettings, const FPGTouchControlSettings& InTouchControlSettings, const FPGBalanceSettings& InBalanceSettings, const FVector& InOriginLocation, const FVector& InRunnerForwardVector, const FVector& InRunnerRightVector);

	// 지면 상태와 쿨타임을 만족하면 수직 점프를 실행합니다. 실제 점프가 시작된 경우에만 true를 반환합니다.
	bool RequestJump();

	// 까마귀 무리 같은 이동형 위험 Actor가 Balance 속도에 순간 충격을 더할 때 사용합니다.
	void ApplyExternalBalanceImpulse(float ImpulseVelocity, float MaxAbsVelocity);

	// 좌우 플릭 점프가 성공했을 때 기존 Balance 이동 위에 짧은 측면 이동 보정을 더합니다.
	void ApplyFlickLateralStep(float DirectionSign);

	// 착지 직후 입력 완화가 남은 시간입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Runner Movement")
	float GetLandingStabilizeRemainingTime() const { return LandingStabilizeRemainingTime; }

	// 러너 시작 기준 전방 축입니다. 자동 전진과 위험 Actor 스폰 기준으로 사용합니다.
	FVector GetRunnerForwardVector() const { return RunnerForwardVector; }

	// 러너 시작 기준 오른쪽 축입니다. Balance 이동과 가상 위험 구역 배치 기준으로 사용합니다.
	FVector GetRunnerRightVector() const { return RunnerRightVector; }

	// 기본 흔들림과 플레이어 보정 입력을 받아 다음 Tick의 좌우 Balance 이동에 반영합니다.
	void SetBalanceMovementInput(float InBaseInstabilityInput, float InPlayerCorrectionInput);

	// 실패 확정 시 이동 모드를 끄고 점프/Balance 관련 잔여 상태를 정리합니다.
	void StopRunnerMovementForFailure();

	// 시작 기준 축에서 현재 캡슐이 좌우로 벗어난 거리입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Runner Movement")
	float GetBalanceOffset() const { return BalanceOffset; }

	// Balance 이동으로 발생 중인 좌우 속도입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Runner Movement")
	float GetBalanceVelocity() const { return BalanceVelocity; }

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void UpdateLateralMovement();
	void UpdateBalanceMovement(float DeltaSeconds);
	void UpdateAirborneState();
	float CalculateJumpVelocity() const;

	FPGJumpSettings CachedJumpSettings;
	FPGTouchControlSettings CachedTouchControlSettings;
	FPGBalanceSettings CachedBalanceSettings;

	FVector OriginLocation = FVector::ZeroVector;
	FVector RunnerForwardVector = FVector::ForwardVector;
	FVector RunnerRightVector = FVector::RightVector;

	float BalanceOffset = 0.0f;
	float BalanceVelocity = 0.0f;
	float BaseInstabilityInput = 0.0f;
	float PlayerCorrectionInput = 0.0f;
	float FlickLateralRemainingTime = 0.0f;
	float FlickLateralVelocity = 0.0f;
	float JumpCooldownRemainingTime = 0.0f;
	float LandingStabilizeRemainingTime = 0.0f;
	bool bWasAirborneLastFrame = false;
};
