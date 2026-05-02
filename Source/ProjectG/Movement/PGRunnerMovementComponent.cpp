// Copyright Epic Games, Inc. All Rights Reserved.

#include "Movement/PGRunnerMovementComponent.h"

UPGRunnerMovementComponent::UPGRunnerMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UPGRunnerMovementComponent::InitializeRunnerMovement(const FPGLaneSettings& InLaneSettings, const FPGGyroSettings& InGyroSettings, const FPGBalanceSettings& InBalanceSettings, const FVector& InLaneOriginLocation, const FVector& InRunnerForwardVector, const FVector& InRunnerRightVector)
{
	CachedLaneSettings = InLaneSettings;
	CachedGyroSettings = InGyroSettings;
	CachedBalanceSettings = InBalanceSettings;
	LaneOriginLocation = InLaneOriginLocation;

	RunnerForwardVector = InRunnerForwardVector;
	RunnerForwardVector.Z = 0.0f;
	RunnerForwardVector = RunnerForwardVector.GetSafeNormal(SMALL_NUMBER, FVector::ForwardVector);

	RunnerRightVector = InRunnerRightVector;
	RunnerRightVector.Z = 0.0f;
	RunnerRightVector = RunnerRightVector.GetSafeNormal(SMALL_NUMBER, FVector::RightVector);

	CurrentLaneIndex = GetCenterLaneIndex();
	TargetLaneIndex = CurrentLaneIndex;
	BufferedLaneDirection = 0;
	LaneStartOffset = 0.0f;
	LaneTargetOffset = 0.0f;
	BalanceOffset = 0.0f;
	BalanceVelocity = 0.0f;
	BaseInstabilityInput = 0.0f;
	PlayerCorrectionInput = 0.0f;
	LaneMoveElapsed = 0.0f;
	BufferedLaneInputRemainingTime = 0.0f;
	LandingStabilizeRemainingTime = 0.0f;
	bIsChangingLane = false;
	bIsWaitingForLaneJumpLanding = false;
}

void UPGRunnerMovementComponent::RequestLaneChange(int32 Direction)
{
	if (Direction == 0)
	{
		return;
	}

	const int32 NormalizedDirection = Direction > 0 ? 1 : -1;
	if (bIsChangingLane || bIsWaitingForLaneJumpLanding)
	{
		// 이동 중 입력은 버퍼에 저장해 착지 직후 다음 라인 이동으로 이어지게 한다.
		BufferedLaneDirection = NormalizedDirection;
		BufferedLaneInputRemainingTime = CachedLaneSettings.LaneInputBufferTime;
		return;
	}

	StartLaneChange(CurrentLaneIndex + NormalizedDirection);
}

FVector UPGRunnerMovementComponent::GetLaneWorldLocation(int32 LaneIndex) const
{
	return GetLocationAtLaneOffset(GetLaneOffset(LaneIndex));
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

	BufferedLaneDirection = 0;
	BufferedLaneInputRemainingTime = 0.0f;
	LaneMoveElapsed = 0.0f;
	LandingStabilizeRemainingTime = 0.0f;
	bIsChangingLane = false;
	bIsWaitingForLaneJumpLanding = false;

	BalanceVelocity = 0.0f;
	BaseInstabilityInput = 0.0f;
	PlayerCorrectionInput = 0.0f;
}

void UPGRunnerMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (MovementMode == MOVE_None)
	{
		return;
	}

	UpdateBalanceMovement(DeltaTime);
	UpdateLaneMovement(DeltaTime);

	if (bIsWaitingForLaneJumpLanding && IsMovingOnGround())
	{
		CompleteLaneJumpLanding();
	}

	if (LandingStabilizeRemainingTime > 0.0f)
	{
		LandingStabilizeRemainingTime = FMath::Max(0.0f, LandingStabilizeRemainingTime - DeltaTime);
	}
}

void UPGRunnerMovementComponent::UpdateLaneMovement(float DeltaSeconds)
{
	if (!UpdatedComponent)
	{
		return;
	}

	if (!bIsChangingLane)
	{
		const float BalancedLaneOffset = GetLaneOffset(CurrentLaneIndex) + BalanceOffset;
		const FVector CurrentLocation = UpdatedComponent->GetComponentLocation();
		FVector TargetLocation = GetLocationAtLaneOffset(BalancedLaneOffset);
		TargetLocation.Z = CurrentLocation.Z;

		const FVector Delta = TargetLocation - CurrentLocation;
		if (!Delta.IsNearlyZero())
		{
			FHitResult Hit;
			SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);
		}

		if (BufferedLaneInputRemainingTime > 0.0f)
		{
			BufferedLaneInputRemainingTime = FMath::Max(0.0f, BufferedLaneInputRemainingTime - DeltaSeconds);
			if (BufferedLaneInputRemainingTime <= 0.0f)
			{
				BufferedLaneDirection = 0;
			}
		}
		return;
	}

	const float Duration = FMath::Max(CachedLaneSettings.LaneChangeDuration, KINDA_SMALL_NUMBER);
	LaneMoveElapsed += DeltaSeconds;

	// Z 이동은 CharacterMovement의 Falling/Walking 흐름에 맡기고, 여기서는 러너 기준 수평 라인 보간만 처리한다.
	const float Alpha = FMath::Clamp(LaneMoveElapsed / Duration, 0.0f, 1.0f);
	const float SmoothAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f);
	const float CurrentLaneOffset = FMath::Lerp(LaneStartOffset, LaneTargetOffset, SmoothAlpha) + BalanceOffset;

	const FVector CurrentLocation = UpdatedComponent->GetComponentLocation();
	FVector TargetLocation = GetLocationAtLaneOffset(CurrentLaneOffset);
	TargetLocation.Z = CurrentLocation.Z;

	const FVector Delta = TargetLocation - CurrentLocation;
	if (!Delta.IsNearlyZero())
	{
		FHitResult Hit;
		SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);
	}

	if (Alpha >= 1.0f)
	{
		CurrentLaneIndex = TargetLaneIndex;

		// 충돌 보정 등으로 생긴 좌우 오차를 Movement 계층에서 한 번 더 줄인다.
		const FVector FinishLocation = UpdatedComponent->GetComponentLocation();
		FVector FinalLaneLocation = GetLocationAtLaneOffset(LaneTargetOffset + BalanceOffset);
		FinalLaneLocation.Z = FinishLocation.Z;

		const FVector FinalDelta = FinalLaneLocation - FinishLocation;
		if (!FinalDelta.IsNearlyZero())
		{
			FHitResult Hit;
			SafeMoveUpdatedComponent(FinalDelta, UpdatedComponent->GetComponentQuat(), true, Hit);
		}

		bIsChangingLane = false;

		if (IsMovingOnGround())
		{
			CompleteLaneJumpLanding();
		}
		else
		{
			bIsWaitingForLaneJumpLanding = true;
		}
	}
}

void UPGRunnerMovementComponent::UpdateBalanceMovement(float DeltaSeconds)
{
	if (!UpdatedComponent || DeltaSeconds <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float DriftAcceleration = BaseInstabilityInput * FMath::Max(0.0f, CachedBalanceSettings.BalanceDriftAcceleration);
	const float CorrectionAcceleration = PlayerCorrectionInput * CachedBalanceSettings.PlayerCorrectionScale * FMath::Max(0.0f, CachedBalanceSettings.PlayerCorrectionAcceleration);
	const float NetAcceleration = DriftAcceleration + CorrectionAcceleration;

	// 기본 흔들림은 실제 캡슐을 밀어내고, 플레이어 입력은 반대 방향 가속도로 중심 회복을 시도한다.
	BalanceVelocity += NetAcceleration * DeltaSeconds;

	const float DampingSpeed = FMath::Max(0.0f, CachedBalanceSettings.BalanceCenteringDamping);
	if (DampingSpeed > 0.0f)
	{
		BalanceVelocity = FMath::FInterpTo(BalanceVelocity, 0.0f, DeltaSeconds, DampingSpeed);
	}

	const float MaxSpeed = FMath::Max(0.0f, CachedBalanceSettings.BalanceMaxSpeed);
	BalanceVelocity = FMath::Clamp(BalanceVelocity, -MaxSpeed, MaxSpeed);

	const float MaxOffset = FMath::Max(0.0f, CachedBalanceSettings.BalanceMaxOffset);
	BalanceOffset = FMath::Clamp(BalanceOffset + BalanceVelocity * DeltaSeconds, -MaxOffset, MaxOffset);

	if ((BalanceOffset >= MaxOffset && BalanceVelocity > 0.0f) || (BalanceOffset <= -MaxOffset && BalanceVelocity < 0.0f))
	{
		// 라인 경계 안에서 흔들리는 감각만 만들고, 밸런스 흔들림만으로 라인 변경을 강제하지 않는다.
		BalanceVelocity = 0.0f;
	}
}

void UPGRunnerMovementComponent::StartLaneChange(int32 NewLaneIndex)
{
	const int32 ClampedLaneIndex = ClampLaneIndex(NewLaneIndex);
	if (ClampedLaneIndex == CurrentLaneIndex || IsFalling())
	{
		return;
	}

	// 이동 시작/목표 오프셋을 고정해 이동 중 기준값이 흔들리지 않도록 한다.
	TargetLaneIndex = ClampedLaneIndex;
	LaneStartOffset = GetLaneOffset(CurrentLaneIndex);
	LaneTargetOffset = GetLaneOffset(TargetLaneIndex);
	LaneMoveElapsed = 0.0f;
	bIsChangingLane = true;
	bIsWaitingForLaneJumpLanding = false;

	const float JumpVelocity = CalculateLaneJumpVelocity();
	if (JumpVelocity > 0.0f)
	{
		// 실제 캡슐을 띄워 끊긴 길/낙하 판정이 CharacterMovement의 기본 바닥 판정과 일관되게 동작하도록 한다.
		Velocity.Z = FMath::Max(Velocity.Z, JumpVelocity);
		SetMovementMode(MOVE_Falling);
	}
}

void UPGRunnerMovementComponent::CompleteLaneJumpLanding()
{
	bIsWaitingForLaneJumpLanding = false;
	LandingStabilizeRemainingTime = CachedGyroSettings.LandingStabilizeTime;
	ConsumeBufferedLaneInput();
}

float UPGRunnerMovementComponent::CalculateLaneJumpVelocity() const
{
	const float JumpHeight = FMath::Max(0.0f, CachedLaneSettings.LaneJumpHeight);
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

void UPGRunnerMovementComponent::ConsumeBufferedLaneInput()
{
	if (BufferedLaneDirection == 0 || BufferedLaneInputRemainingTime <= 0.0f)
	{
		BufferedLaneDirection = 0;
		BufferedLaneInputRemainingTime = 0.0f;
		return;
	}

	const int32 Direction = BufferedLaneDirection;
	BufferedLaneDirection = 0;
	BufferedLaneInputRemainingTime = 0.0f;

	// 버퍼 입력은 현재 착지한 라인을 기준으로 다시 판정한다.
	StartLaneChange(CurrentLaneIndex + Direction);
}

int32 UPGRunnerMovementComponent::GetCenterLaneIndex() const
{
	return FMath::Clamp(CachedLaneSettings.LaneCount / 2, 0, FMath::Max(0, CachedLaneSettings.LaneCount - 1));
}

int32 UPGRunnerMovementComponent::ClampLaneIndex(int32 LaneIndex) const
{
	return FMath::Clamp(LaneIndex, 0, FMath::Max(0, CachedLaneSettings.LaneCount - 1));
}

float UPGRunnerMovementComponent::GetLaneOffset(int32 LaneIndex) const
{
	return static_cast<float>(LaneIndex - GetCenterLaneIndex()) * CachedLaneSettings.LaneWidth;
}

float UPGRunnerMovementComponent::GetCurrentLaneOffset() const
{
	if (!UpdatedComponent)
	{
		return 0.0f;
	}

	// 시작 중앙 라인에서 현재 캡슐까지의 벡터를 러너 오른쪽 축에 투영한다.
	return FVector::DotProduct(UpdatedComponent->GetComponentLocation() - LaneOriginLocation, RunnerRightVector);
}

FVector UPGRunnerMovementComponent::GetLocationAtLaneOffset(float LaneOffset) const
{
	if (!UpdatedComponent)
	{
		return LaneOriginLocation + RunnerRightVector * LaneOffset;
	}

	// 전방 진행 거리는 현재 위치에서 유지하고 좌우 오프셋만 라인 기준값으로 교체한다.
	const FVector OriginToActor = UpdatedComponent->GetComponentLocation() - LaneOriginLocation;
	const float ForwardDistance = FVector::DotProduct(OriginToActor, RunnerForwardVector);
	return LaneOriginLocation + RunnerForwardVector * ForwardDistance + RunnerRightVector * LaneOffset;
}
