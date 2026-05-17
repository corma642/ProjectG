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

	return FMath::Sqrt(2.0f * Gravity * JumpHeight);
}
