// Copyright Epic Games, Inc. All Rights Reserved.

#include "Movement/PGRunnerMovementComponent.h"

UPGRunnerMovementComponent::UPGRunnerMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UPGRunnerMovementComponent::InitializeRunnerMovement(const FPGJumpSettings& InJumpSettings, const FPGTouchControlSettings& InTouchControlSettings, const FPGBalanceSettings& InBalanceSettings, const FVector& InOriginLocation, const FVector& InRunnerForwardVector, const FVector& InRunnerRightVector)
{
	CachedJumpSettings = InJumpSettings;
	CachedTouchControlSettings = InTouchControlSettings;
	CachedBalanceSettings = InBalanceSettings;
	OriginLocation = InOriginLocation;

	RunnerForwardVector = InRunnerForwardVector;
	RunnerForwardVector.Z = 0.0f;
	RunnerForwardVector = RunnerForwardVector.GetSafeNormal(SMALL_NUMBER, FVector::ForwardVector);

	// Home 배치 회전과 관계없이, 런 시작 시점에 정리된 수평 전방/우측 축을 Balance 기준으로 고정합니다.
	RunnerRightVector = InRunnerRightVector;
	RunnerRightVector.Z = 0.0f;
	RunnerRightVector = RunnerRightVector.GetSafeNormal(SMALL_NUMBER, FVector::RightVector);

	BalanceOffset = 0.0f;
	BalanceVelocity = 0.0f;
	BaseInstabilityInput = 0.0f;
	PlayerCorrectionInput = 0.0f;
	FlickLateralRemainingTime = 0.0f;
	FlickLateralVelocity = 0.0f;
	JumpCooldownRemainingTime = 0.0f;
	LandingStabilizeRemainingTime = 0.0f;
	bWasAirborneLastFrame = IsFalling();
}

bool UPGRunnerMovementComponent::RequestJump()
{
	// 점프는 지면 상태와 쿨타임을 모두 만족할 때만 실행합니다.
	// 입력 버퍼를 두지 않아 터치 조작의 예측 불가능한 연속 점프를 막습니다.
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

void UPGRunnerMovementComponent::ApplyExternalBalanceImpulse(float ImpulseVelocity, float MaxAbsVelocity)
{
	if (MovementMode == MOVE_None)
	{
		return;
	}

	BalanceVelocity += ImpulseVelocity;

	const float VelocityLimit = FMath::Max(0.0f, MaxAbsVelocity);
	if (VelocityLimit > 0.0f)
	{
		// 위험 Actor가 여러 번 맞더라도 Balance 속도가 비정상적으로 누적되지 않도록 상한을 둡니다.
		BalanceVelocity = FMath::Clamp(BalanceVelocity, -VelocityLimit, VelocityLimit);
	}
}

void UPGRunnerMovementComponent::ApplyFlickLateralStep(float DirectionSign)
{
	if (MovementMode == MOVE_None)
	{
		return;
	}

	const float ClampedDirection = FMath::Clamp(DirectionSign, -1.0f, 1.0f);
	const float Duration = FMath::Max(0.01f, CachedTouchControlSettings.FlickLateralMoveDuration);
	const float Distance = FMath::Max(0.0f, CachedTouchControlSettings.FlickLateralStepDistance);
	if (FMath::IsNearlyZero(ClampedDirection) || Distance <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// 플릭은 라인 텔레포트가 아니라 기존 Balance 이동 위에 잠깐 속도를 더하는 보정입니다.
	// 그래서 캐릭터는 몸 기울기와 관성 이동을 유지하면서 지정 거리만큼 자연스럽게 이동합니다.
	FlickLateralRemainingTime = Duration;
	FlickLateralVelocity = ClampedDirection * (Distance / Duration);
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
	FlickLateralRemainingTime = 0.0f;
	FlickLateralVelocity = 0.0f;
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

	// CharacterMovement의 전진 이동은 유지하고, 이 컴포넌트는 시작 기준선에서의 좌우 이탈만 보정합니다.
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

	// 공중에서는 짧은 체공 시간 안에 회피가 가능해야 하므로 플레이어 보정과 최대 좌우 속도를 함께 강화합니다.
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

	if (FlickLateralRemainingTime > 0.0f)
	{
		const float StepDeltaTime = FMath::Min(DeltaSeconds, FlickLateralRemainingTime);
		BalanceOffset += FlickLateralVelocity * StepDeltaTime;
		FlickLateralRemainingTime = FMath::Max(0.0f, FlickLateralRemainingTime - DeltaSeconds);
		if (FlickLateralRemainingTime <= KINDA_SMALL_NUMBER)
		{
			FlickLateralRemainingTime = 0.0f;
			FlickLateralVelocity = 0.0f;
		}
	}
}

void UPGRunnerMovementComponent::UpdateAirborneState()
{
	const bool bIsAirborne = IsFalling();
	if (bWasAirborneLastFrame && !bIsAirborne)
	{
		// 착지 직후에는 센서/키 입력이 튀기 쉬우므로 짧게 안정화 시간을 둡니다.
		LandingStabilizeRemainingTime = CachedJumpSettings.LandingInputDampingDuration;
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

	// 목표 점프 높이에 도달하는 초기 속도를 중력값으로부터 계산합니다.
	return FMath::Sqrt(2.0f * Gravity * JumpHeight);
}
