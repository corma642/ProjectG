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
	/** 러너 전용 이동 모드와 불필요한 기본 회전 동작을 설정합니다. */
	UPGRunnerMovementComponent();

	/** 런 시작 위치·정규 축과 점프/터치/Balance 설정을 적용하고 누적 이동 상태를 reset합니다. */
	void InitializeRunnerMovement(const FPGJumpSettings& InJumpSettings, const FPGTouchControlSettings& InTouchControlSettings, const FPGBalanceSettings& InBalanceSettings, const FVector& InOriginLocation, const FVector& InRunnerForwardVector, const FVector& InRunnerRightVector);

	/** 지면 상태와 쿨타임을 만족하면 수직 점프를 실행하고, 실제 점프가 시작된 경우에만 true를 반환합니다. */
	bool RequestJump();

	/**
	 * 이동형 위험 Actor의 cm/s 순간 충격을 Balance 속도에 더합니다. 속도 제한은 다음 Tick에 적용됩니다.
	 * MaxAbsVelocity가 그 시점의 일반 Balance 최대 속도 이하면 일반 상한까지만 밀리고,
	 * 그보다 크면 MaxAbsVelocity까지 밀린 뒤 감쇠로 줄어들 때까지 초과 속도가 유지됩니다.
	 */
	void ApplyExternalBalanceImpulse(float ImpulseVelocity, float MaxAbsVelocity);

	/**
	 * 외부 충격 허용 상한(cm/s)을 반영해 Balance 속도를 제한하고, 다음 Tick에 넘길 허용 상한을 갱신합니다.
	 * 허용 상한은 입력으로 다시 커지지 않습니다. 매 Tick 감쇠 속도만큼 줄고, 실제 속도가 그보다 낮으면 그 값으로 내려가며,
	 * 속도가 일반 상한 안으로 돌아오면 0으로 해제됩니다. 감쇠가 0이면 실제 속도를 따라서만 내려갑니다.
	 */
	static float ClampBalanceVelocity(float Velocity, float NormalMaxSpeed, float DampingSpeed, float DeltaSeconds, float& InOutImpulseSpeedLimit);

	/** 점프 성공 직후 -1~1 방향 부호에 따라 짧은 측면 이동 보정을 더합니다. */
	void ApplyFlickLateralStep(float DirectionSign);

	/** 착지 직후 입력 완화가 남은 시간을 초 단위로 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Runner Movement")
	float GetLandingStabilizeRemainingTime() const { return LandingStabilizeRemainingTime; }

	/** 자동 전진과 위험 Actor 스폰 기준인 런 시작 월드 전방 단위 벡터를 반환합니다. */
	FVector GetRunnerForwardVector() const { return RunnerForwardVector; }

	/** Balance 이동과 위험 구역 배치 기준인 런 시작 월드 오른쪽 단위 벡터를 반환합니다. */
	FVector GetRunnerRightVector() const { return RunnerRightVector; }

	/** -1~1 기본 흔들림과 플레이어 보정 입력을 다음 CharacterMovement Tick에 반영합니다. */
	void SetBalanceMovementInput(float InBaseInstabilityInput, float InPlayerCorrectionInput);

	/** 실패 확정 시 이동 모드를 끄고 점프·Balance 잔여 상태를 정리합니다. */
	void StopRunnerMovementForFailure();

	/** 시작 기준 축에서 현재 캡슐이 좌우로 벗어난 거리를 cm 단위로 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Runner Movement")
	float GetBalanceOffset() const { return BalanceOffset; }

	/** Balance 이동으로 발생 중인 좌우 속도를 cm/s 단위로 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Runner Movement")
	float GetBalanceVelocity() const { return BalanceVelocity; }

protected:
	/** CharacterMovement의 표준 Tick에서 자동 전진과 Balance 오프셋을 순서대로 반영합니다. */
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
	// 외부 충격 직후 일반 Balance 최대 속도를 넘겨도 되는 일시 상한(cm/s). 감쇠로 줄어들며 0이면 일반 상한만 적용합니다.
	float ExternalImpulseSpeedLimit = 0.0f;
	float BaseInstabilityInput = 0.0f;
	float PlayerCorrectionInput = 0.0f;
	float FlickLateralRemainingTime = 0.0f;
	float FlickLateralVelocity = 0.0f;
	float JumpCooldownRemainingTime = 0.0f;
	float LandingStabilizeRemainingTime = 0.0f;
	bool bWasAirborneLastFrame = false;
};
