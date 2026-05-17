// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Settings/PGGameSettings.h"
#include "PGRunnerMovementComponent.generated.h"

/**
 * Go Deliver! 러너의 자동 전진 외 좌우 밸런스 이동과 수직 점프를 담당하는 이동 컴포넌트입니다.
 * 라인 인덱스 이동은 사용하지 않고, 기울기 입력으로 생기는 좌우 오프셋만 관리합니다.
 */
UCLASS()
class PROJECTG_API UPGRunnerMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UPGRunnerMovementComponent();

	// Project Settings에서 캐시한 점프/자이로/밸런스 설정과 시작 기준 축을 받아 이동 상태를 초기화합니다.
	void InitializeRunnerMovement(const FPGJumpSettings& InJumpSettings, const FPGGyroSettings& InGyroSettings, const FPGBalanceSettings& InBalanceSettings, const FVector& InOriginLocation, const FVector& InRunnerForwardVector, const FVector& InRunnerRightVector);

	// 지면 상태와 쿨타임을 만족하면 수직 점프를 실행합니다. 실제 점프가 시작된 경우에만 true를 반환합니다.
	bool RequestJump();

	// 착지 직후 자이로 입력을 완화해야 하는 남은 시간입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Runner Movement")
	float GetLandingStabilizeRemainingTime() const { return LandingStabilizeRemainingTime; }

	// 러너 기준 수평 전방 축
	FVector GetRunnerForwardVector() const { return RunnerForwardVector; }

	// 러너 기준 수평 오른쪽 축
	FVector GetRunnerRightVector() const { return RunnerRightVector; }

	// 기본 흔들림과 플레이어 보정 입력을 받아 실제 캡슐 좌우 이동에 반영합니다.
	void SetBalanceMovementInput(float InBaseInstabilityInput, float InPlayerCorrectionInput);

	// 런 실패가 확정되면 이동 모드를 끄고 점프/밸런스 상태를 정리합니다.
	void StopRunnerMovementForFailure();

	// 시작 기준선에서 현재 캡슐이 좌우로 벗어난 거리입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Runner Movement")
	float GetBalanceOffset() const { return BalanceOffset; }

	// 밸런스 이동으로 발생 중인 좌우 속도입니다.
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
	FPGGyroSettings CachedGyroSettings;
	FPGBalanceSettings CachedBalanceSettings;

	FVector OriginLocation = FVector::ZeroVector;
	FVector RunnerForwardVector = FVector::ForwardVector;
	FVector RunnerRightVector = FVector::RightVector;

	float BalanceOffset = 0.0f;
	float BalanceVelocity = 0.0f;
	float BaseInstabilityInput = 0.0f;
	float PlayerCorrectionInput = 0.0f;
	float JumpCooldownRemainingTime = 0.0f;
	float LandingStabilizeRemainingTime = 0.0f;
	bool bWasAirborneLastFrame = false;
};
