// Copyright Epic Games, Inc. All Rights Reserved.

#include "Movement/PGRunnerMovementComponent.h"

UPGRunnerMovementComponent::UPGRunnerMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UPGRunnerMovementComponent::InitializeRunnerMovement(const FPGJumpSettings& InJumpSettings, const FPGGyroSettings& InGyroSettings, const FPGBalanceSettings& InBalanceSettings, const FVector& InOriginLocation, const FVector& InRunnerForwardVector, const FVector& InRunnerRightVector)
{
	CachedJumpSettings = InJumpSettings;
	CachedGyroSettings = InGyroSettings;
	CachedBalanceSettings = InBalanceSettings;
	OriginLocation = InOriginLocation;

	RunnerForwardVector = InRunnerForwardVector;
	RunnerForwardVector.Z = 0.0f;
	RunnerForwardVector = RunnerForwardVector.GetSafeNormal(SMALL_NUMBER, FVector::ForwardVector);

	// 러너가 회전된 상태로 배치되어도 Balance 이동은 런 시작 시점의 수평 전방/우측 축을 기준으로 누적한다.
	RunnerRightVector = InRunnerRightVector;
	RunnerRightVector.Z = 0.0f;
	RunnerRightVector = RunnerRightVector.GetSafeNormal(SMALL_NUMBER, FVector::RightVector);

	BalanceOffset = 0.0f;
	BalanceVelocity = 0.0f;
	BaseInstabilityInput = 0.0f;
	PlayerCorrectionInput = 0.0f;
	JumpCooldownRemainingTime = 0.0f;
	LandingStabilizeRemainingTime = 0.0f;
	bWasAirborneLastFrame = IsFalling();
}

bool UPGRunnerMovementComponent::RequestJump()
{
	// 점프는 지면 상태와 쿨타임을 모두 만족할 때만 실행한다. 입력 버퍼를 두지 않아 터치 조작이 예측 가능하게 유지된다.
	if (!UpdatedComponent || MovementMode == MOVE_None || JumpCooldownRemainingTime > 0.0f || !IsMovingOnGround())
	{
		return false;
	}

	const float JumpVelocity = CalculateJumpVelocity();
	if (JumpVelocity <= 0.0f)
	{
		return false;
	}

	Velocity.Z = FMath::Max(Velocity.Z, JumpVelocity);
	SetMovementMode(MOVE_Falling);
	JumpCooldownRemainingTime = FMath::Max(0.0f, CachedJumpSettings.JumpCooldownDuration);
	bWasAirborneLastFrame = true;
	return true;
}

void UPGRunnerMovementComponent::SetBalanceMovementInput(float InBaseInstabilityInput, float InPlayerCorrectionInput)
{
	BaseInstabilityInput = FMath::Clamp(InBaseInstabilityInput, -1.0f, 1.0f);
	PlayerCorrectionInput = FMath::Clamp(InPlayerCorrectionInput, -1.0f, 1.0f);
}

void UPGRunnerMovementComponent::StopRunnerMovementForFailure()
{
	StopMovementImmediately();
	DisableMovement();

	BalanceVelocity = 0.0f;
	BaseInstabilityInput = 0.0f;
	PlayerCorrectionInput = 0.0f;
	JumpCooldownRemainingTime = 0.0f;
	LandingStabilizeRemainingTime = 0.0f;
	bWasAirborneLastFrame = false;
}

void UPGRunnerMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (MovementMode == MOVE_None)
	{
		return;
	}

	if (JumpCooldownRemainingTime > 0.0f)
	{
		JumpCooldownRemainingTime = FMath::Max(0.0f, JumpCooldownRemainingTime - DeltaTime);
	}

	if (LandingStabilizeRemainingTime > 0.0f)
	{
		LandingStabilizeRemainingTime = FMath::Max(0.0f, LandingStabilizeRemainingTime - DeltaTime);
	}

	UpdateBalanceMovement(DeltaTime);
	UpdateLateralMovement();
	UpdateAirborneState();
}

void UPGRunnerMovementComponent::UpdateLateralMovement()
{
	if (!UpdatedComponent)
	{
		return;
	}

	const FVector CurrentLocation = UpdatedComponent->GetComponentLocation();
	const FVector OriginToActor = CurrentLocation - OriginLocation;
	const float ForwardDistance = FVector::DotProduct(OriginToActor, RunnerForwardVector);

	// 전방 진행 거리는 자동 전진이 담당하고, 이 컴포넌트는 기준선에서 우측으로 얼마나 벗어났는지만 적용한다.
	FVector TargetLocation = OriginLocation + RunnerForwardVector * ForwardDistance + RunnerRightVector * BalanceOffset;
	TargetLocation.Z = CurrentLocation.Z;

	const FVector Delta = TargetLocation - CurrentLocation;
	if (!Delta.IsNearlyZero())
	{
		FHitResult Hit;
		SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);
	}
}

void UPGRunnerMovementComponent::UpdateBalanceMovement(float DeltaSeconds)
{
	if (!UpdatedComponent || DeltaSeconds <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const bool bIsAirborne = IsFalling();
	const float AirborneControlScale = bIsAirborne ? FMath::Max(0.0f, CachedJumpSettings.AirborneBalanceControlScale) : 1.0f;
	constexpr float AirborneDampingScale = 0.2f;
	const float DampingScale = bIsAirborne ? AirborneDampingScale : 1.0f;

	// 공중에서는 좌우 보정을 더 강하게 먹이고 감쇠를 줄여, 점프 후 기울여 착지 지점을 고르는 조작감을 만든다.
	const float DriftAcceleration = BaseInstabilityInput * FMath::Max(0.0f, CachedBalanceSettings.BalanceDriftAcceleration);
	const float CorrectionAcceleration = PlayerCorrectionInput * FMath::Max(0.0f, CachedBalanceSettings.PlayerCorrectionAcceleration) * AirborneControlScale;
	const float NetAcceleration = DriftAcceleration + CorrectionAcceleration;

	BalanceVelocity += NetAcceleration * DeltaSeconds;

	const float DampingSpeed = FMath::Max(0.0f, CachedBalanceSettings.BalanceCenteringDamping) * DampingScale;
	if (DampingSpeed > 0.0f)
	{
		BalanceVelocity = FMath::FInterpTo(BalanceVelocity, 0.0f, DeltaSeconds, DampingSpeed);
	}

	const float MaxSpeed = FMath::Max(0.0f, CachedBalanceSettings.BalanceMaxSpeed) * AirborneControlScale;
	BalanceVelocity = FMath::Clamp(BalanceVelocity, -MaxSpeed, MaxSpeed);

	BalanceOffset += BalanceVelocity * DeltaSeconds;
}

void UPGRunnerMovementComponent::UpdateAirborneState()
{
	const bool bIsAirborne = IsFalling();
	if (bWasAirborneLastFrame && !bIsAirborne)
	{
		// 착지 직후에는 센서/키 입력이 튀기 쉬우므로 짧게 안정화 시간을 둔다.
		LandingStabilizeRemainingTime = CachedGyroSettings.LandingStabilizeTime;
	}

	bWasAirborneLastFrame = bIsAirborne;
}

float UPGRunnerMovementComponent::CalculateJumpVelocity() const
{
	const float JumpHeight = FMath::Max(0.0f, CachedJumpSettings.JumpHeight);
	if (JumpHeight <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	const float Gravity = FMath::Abs(GetGravityZ());
	if (Gravity <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	// 목표 점프 높이를 만족하는 초기 속도를 중력값에서 역산한다.
	return FMath::Sqrt(2.0f * Gravity * JumpHeight);
}
