// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/PGRunnerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "DrawDebugHelpers.h"
#include "Settings/PGGameSettings.h"

APGRunnerCharacter::APGRunnerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->bEnableCameraLag = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = false;
	Movement->bUseControllerDesiredRotation = false;
	Movement->GravityScale = 1.0f;
	Movement->AirControl = 0.35f;
	Movement->BrakingFrictionFactor = 0.0f;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
}

void APGRunnerCharacter::BeginPlay()
{
	Super::BeginPlay();

	ApplyGameSettings();

	CenterLaneY = GetActorLocation().Y;
	LaneStartY = CenterLaneY;
	LaneTargetY = CenterLaneY;
	LaneStartZ = GetActorLocation().Z;
	CurrentLaneIndex = GetCenterLaneIndex();
	TargetLaneIndex = CurrentLaneIndex;

	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		InitialMeshRelativeRotation = MeshComponent->GetRelativeRotation();
	}
}

void APGRunnerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ApplyGameSettings();
	UpdateForwardMovement();
	UpdateLaneMovement(DeltaSeconds);
	UpdateBodyLean(DeltaSeconds);
	DrawDebugLanes();

	if (LandingStabilizeRemainingTime > 0.0f)
	{
		LandingStabilizeRemainingTime = FMath::Max(0.0f, LandingStabilizeRemainingTime - DeltaSeconds);
	}
}

void APGRunnerCharacter::RequestLaneChange(int32 Direction)
{
	if (Direction == 0)
	{
		return;
	}

	const int32 NormalizedDirection = Direction > 0 ? 1 : -1;
	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();

	if (bIsChangingLane)
	{
		BufferedLaneDirection = NormalizedDirection;
		BufferedLaneInputRemainingTime = Settings->Lane.LaneInputBufferTime;
		return;
	}

	StartLaneChange(CurrentLaneIndex + NormalizedDirection);
}

void APGRunnerCharacter::SetBalanceInput(float InBalanceInput)
{
	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	const float ClampedInput = FMath::Clamp(InBalanceInput, -Settings->Gyro.MaxTiltInput, Settings->Gyro.MaxTiltInput);
	const float DampingScale = bIsChangingLane ? Settings->Gyro.JumpTiltDampingScale : 1.0f;
	const float LandingScale = LandingStabilizeRemainingTime > 0.0f ? 0.5f : 1.0f;

	BalanceInput = ClampedInput * DampingScale * LandingScale;
}

void APGRunnerCharacter::ApplyGameSettings()
{
	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed = Settings->Runner.ForwardSpeed;

	CameraBoom->TargetArmLength = Settings->Camera.DefaultArmLength;
	CameraBoom->SetRelativeRotation(FRotator(Settings->Camera.InitialPitch, 0.0f, 0.0f));
	CameraBoom->SocketOffset = Settings->Camera.SocketOffset;
	CameraBoom->CameraLagSpeed = Settings->Camera.LagSpeed;
	FollowCamera->SetFieldOfView(Settings->Camera.DefaultFOV);
}

void APGRunnerCharacter::UpdateForwardMovement()
{
	AddMovementInput(FVector::ForwardVector, 1.0f);
}

void APGRunnerCharacter::UpdateLaneMovement(float DeltaSeconds)
{
	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();

	if (!bIsChangingLane)
	{
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

	const float Duration = FMath::Max(Settings->Lane.LaneChangeDuration, KINDA_SMALL_NUMBER);
	LaneMoveElapsed += DeltaSeconds;

	const float Alpha = FMath::Clamp(LaneMoveElapsed / Duration, 0.0f, 1.0f);
	const float SmoothAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f);
	const float JumpOffset = FMath::Sin(Alpha * UE_PI) * Settings->Lane.LaneJumpHeight;

	FVector NewLocation = GetActorLocation();
	NewLocation.Y = FMath::Lerp(LaneStartY, LaneTargetY, SmoothAlpha);
	NewLocation.Z = LaneStartZ + JumpOffset;
	SetActorLocation(NewLocation, true);

	if (Alpha >= 1.0f)
	{
		bIsChangingLane = false;
		CurrentLaneIndex = TargetLaneIndex;

		FVector FinalLocation = GetActorLocation();
		FinalLocation.Y = LaneTargetY;
		FinalLocation.Z = LaneStartZ;
		SetActorLocation(FinalLocation, true);

		LandingStabilizeRemainingTime = Settings->Gyro.LandingStabilizeTime;
		ConsumeBufferedLaneInput();
	}
}

void APGRunnerCharacter::UpdateBodyLean(float DeltaSeconds)
{
	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	const float TargetRoll = BalanceInput * Settings->Runner.BodyLeanMaxRoll;
	CurrentBodyRoll = FMath::FInterpTo(CurrentBodyRoll, TargetRoll, DeltaSeconds, Settings->Runner.BodyLeanInterpSpeed);

	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		FRotator LeanRotation = InitialMeshRelativeRotation;
		LeanRotation.Roll += CurrentBodyRoll;
		MeshComponent->SetRelativeRotation(LeanRotation);
	}
}

void APGRunnerCharacter::DrawDebugLanes() const
{
	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	if (!Settings->Debug.bDrawLaneDebug)
	{
		return;
	}

	constexpr float DebugForwardLength = 1000.0f;
	constexpr float DebugHeight = 8.0f;
	const FVector ActorLocation = GetActorLocation();

	for (int32 LaneIndex = 0; LaneIndex < Settings->Lane.LaneCount; ++LaneIndex)
	{
		const float LaneY = CenterLaneY + GetLaneOffset(LaneIndex);
		const FVector Start(ActorLocation.X - 200.0f, LaneY, ActorLocation.Z + DebugHeight);
		const FVector End(ActorLocation.X + DebugForwardLength, LaneY, ActorLocation.Z + DebugHeight);
		const FColor LaneColor = LaneIndex == TargetLaneIndex ? FColor::Green : FColor::Silver;
		DrawDebugLine(GetWorld(), Start, End, LaneColor, false, 0.0f, 0, 2.0f);
	}
}

void APGRunnerCharacter::StartLaneChange(int32 NewLaneIndex)
{
	const int32 ClampedLaneIndex = ClampLaneIndex(NewLaneIndex);
	if (ClampedLaneIndex == CurrentLaneIndex)
	{
		return;
	}

	TargetLaneIndex = ClampedLaneIndex;
	LaneStartY = GetActorLocation().Y;
	LaneTargetY = CenterLaneY + GetLaneOffset(TargetLaneIndex);
	LaneStartZ = GetActorLocation().Z;
	LaneMoveElapsed = 0.0f;
	bIsChangingLane = true;
}

void APGRunnerCharacter::ConsumeBufferedLaneInput()
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
	StartLaneChange(CurrentLaneIndex + Direction);
}

int32 APGRunnerCharacter::GetCenterLaneIndex() const
{
	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	return FMath::Clamp(Settings->Lane.LaneCount / 2, 0, FMath::Max(0, Settings->Lane.LaneCount - 1));
}

int32 APGRunnerCharacter::ClampLaneIndex(int32 LaneIndex) const
{
	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	return FMath::Clamp(LaneIndex, 0, FMath::Max(0, Settings->Lane.LaneCount - 1));
}

float APGRunnerCharacter::GetLaneOffset(int32 LaneIndex) const
{
	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	return static_cast<float>(LaneIndex - GetCenterLaneIndex()) * Settings->Lane.LaneWidth;
}
