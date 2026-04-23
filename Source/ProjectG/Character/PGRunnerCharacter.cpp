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

	// 메시가 없어도 캡슐 기준 이동 테스트가 가능하도록 기본 크기를 먼저 잡는다.
	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->bEnableCameraLag = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	// 러너는 항상 전방을 바라보므로 이동 방향에 따른 자동 회전은 끈다.
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

	// 시작 위치와 방향을 러너 기준 트랙 좌표계로 고정해 월드 X/Y축 의존을 제거한다.
	LaneOriginLocation = GetActorLocation();
	RunnerForwardVector = GetActorForwardVector();
	RunnerForwardVector.Z = 0.0f;
	RunnerForwardVector = RunnerForwardVector.GetSafeNormal(SMALL_NUMBER, FVector::ForwardVector);
	RunnerRightVector = GetActorRightVector();
	RunnerRightVector.Z = 0.0f;
	RunnerRightVector = RunnerRightVector.GetSafeNormal(SMALL_NUMBER, FVector::RightVector);
	LaneStartOffset = 0.0f;
	LaneTargetOffset = 0.0f;
	CurrentLaneIndex = GetCenterLaneIndex();
	TargetLaneIndex = CurrentLaneIndex;

	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		InitialMeshRelativeRotation = MeshComponent->GetRelativeRotation();
		InitialMeshRelativeLocation = MeshComponent->GetRelativeLocation();
	}
}

void APGRunnerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

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
	if (bIsChangingLane)
	{
		// 이동 중 입력은 버퍼에 저장해 착지 직후 다음 라인 이동으로 이어지게 한다.
		BufferedLaneDirection = NormalizedDirection;
		BufferedLaneInputRemainingTime = CachedLaneSettings.LaneInputBufferTime;
		return;
	}

	StartLaneChange(CurrentLaneIndex + NormalizedDirection);
}

void APGRunnerCharacter::SetBalanceInput(float InBalanceInput)
{
	const float ClampedInput = FMath::Clamp(InBalanceInput, -CachedGyroSettings.MaxTiltInput, CachedGyroSettings.MaxTiltInput);

	// 점프 중/착지 직후에는 조작 피로를 줄이기 위해 자이로 영향력을 완화한다.
	const float DampingScale = bIsChangingLane ? CachedGyroSettings.JumpTiltDampingScale : 1.0f;
	const float LandingScale = LandingStabilizeRemainingTime > 0.0f ? 0.5f : 1.0f;

	BalanceInput = ClampedInput * DampingScale * LandingScale;
}

void APGRunnerCharacter::ApplyGameSettings()
{
	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	CachedRunnerSettings = Settings->Runner;
	CachedLaneSettings = Settings->Lane;
	CachedGyroSettings = Settings->Gyro;
	CachedCameraSettings = Settings->Camera;
	CachedDebugSettings = Settings->Debug;

	// Project Settings 값은 BeginPlay에서 1회 적용한다. PIE 중 변경한 값은 재시작 후 반영한다.
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed = CachedRunnerSettings.ForwardSpeed;

	CameraBoom->TargetArmLength = CachedCameraSettings.DefaultArmLength;
	CameraBoom->SetRelativeRotation(FRotator(CachedCameraSettings.InitialPitch, 0.0f, 0.0f));
	CameraBoom->SocketOffset = CachedCameraSettings.SocketOffset;
	CameraBoom->CameraLagSpeed = CachedCameraSettings.LagSpeed;
	FollowCamera->SetFieldOfView(CachedCameraSettings.DefaultFOV);
}

void APGRunnerCharacter::UpdateForwardMovement()
{
	AddMovementInput(RunnerForwardVector, 1.0f);
}

void APGRunnerCharacter::UpdateLaneMovement(float DeltaSeconds)
{
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

	const float Duration = FMath::Max(CachedLaneSettings.LaneChangeDuration, KINDA_SMALL_NUMBER);
	LaneMoveElapsed += DeltaSeconds;

	// 캡슐은 수평 라인 이동만 담당하고, 점프 높이는 메시 상대 위치로만 표현한다.
	const float Alpha = FMath::Clamp(LaneMoveElapsed / Duration, 0.0f, 1.0f);
	const float SmoothAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f);
	const float JumpOffset = FMath::Sin(Alpha * UE_PI) * CachedLaneSettings.LaneJumpHeight;
	const float CurrentLaneOffset = FMath::Lerp(LaneStartOffset, LaneTargetOffset, SmoothAlpha);

	FVector NewLocation = GetLocationAtLaneOffset(CurrentLaneOffset);
	NewLocation.Z = GetActorLocation().Z;
	SetActorLocation(NewLocation, true);
	SetMeshJumpOffset(JumpOffset);

	if (Alpha >= 1.0f)
	{
		bIsChangingLane = false;
		CurrentLaneIndex = TargetLaneIndex;

		// 충돌 보정 등으로 생긴 좌우 오차를 제거해 다음 라인 계산의 기준을 안정화한다.
		FVector FinalLocation = GetLocationAtLaneOffset(LaneTargetOffset);
		FinalLocation.Z = GetActorLocation().Z;
		SetActorLocation(FinalLocation, true);
		SetMeshJumpOffset(0.0f);

		// 착지 직후 짧은 안정화 시간을 부여해 자이로와 점프 입력이 충돌하지 않게 한다.
		LandingStabilizeRemainingTime = CachedGyroSettings.LandingStabilizeTime;
		ConsumeBufferedLaneInput();
	}
}

void APGRunnerCharacter::UpdateBodyLean(float DeltaSeconds)
{
	const float TargetRoll = BalanceInput * CachedRunnerSettings.BodyLeanMaxRoll;

	// 중심 입력이 즉시 꺾이지 않고 몸이 따라가는 느낌이 나도록 보간한다.
	CurrentBodyRoll = FMath::FInterpTo(CurrentBodyRoll, TargetRoll, DeltaSeconds, CachedRunnerSettings.BodyLeanInterpSpeed);

	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		FRotator LeanRotation = InitialMeshRelativeRotation;
		LeanRotation.Roll += CurrentBodyRoll;
		MeshComponent->SetRelativeRotation(LeanRotation);
	}
}

void APGRunnerCharacter::DrawDebugLanes() const
{
	if (!CachedDebugSettings.bDrawLaneDebug)
	{
		return;
	}

	constexpr float DebugForwardLength = 1000.0f;
	constexpr float DebugBackLength = 200.0f;
	constexpr float DebugHeight = 8.0f;
	const FVector DebugUpOffset = FVector::UpVector * DebugHeight;

	for (int32 LaneIndex = 0; LaneIndex < CachedLaneSettings.LaneCount; ++LaneIndex)
	{
		// 목표 라인은 초록색으로 표시해 이동 중 도착 라인을 쉽게 확인한다.
		const FVector LaneLocation = GetLocationAtLaneOffset(GetLaneOffset(LaneIndex));
		const FVector Start = LaneLocation - RunnerForwardVector * DebugBackLength + DebugUpOffset;
		const FVector End = LaneLocation + RunnerForwardVector * DebugForwardLength + DebugUpOffset;
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

	// 이동 시작/목표 오프셋을 고정해 이동 중 기준값이 흔들리지 않도록 한다.
	TargetLaneIndex = ClampedLaneIndex;
	LaneStartOffset = GetCurrentLaneOffset();
	LaneTargetOffset = GetLaneOffset(TargetLaneIndex);
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

	// 버퍼 입력은 현재 착지한 라인을 기준으로 다시 판정한다.
	StartLaneChange(CurrentLaneIndex + Direction);
}

int32 APGRunnerCharacter::GetCenterLaneIndex() const
{
	return FMath::Clamp(CachedLaneSettings.LaneCount / 2, 0, FMath::Max(0, CachedLaneSettings.LaneCount - 1));
}

int32 APGRunnerCharacter::ClampLaneIndex(int32 LaneIndex) const
{
	return FMath::Clamp(LaneIndex, 0, FMath::Max(0, CachedLaneSettings.LaneCount - 1));
}

float APGRunnerCharacter::GetLaneOffset(int32 LaneIndex) const
{
	return static_cast<float>(LaneIndex - GetCenterLaneIndex()) * CachedLaneSettings.LaneWidth;
}

float APGRunnerCharacter::GetCurrentLaneOffset() const
{
	// 시작 중앙 라인에서 현재 캡슐까지의 벡터를 러너 오른쪽 축에 투영한다.
	return FVector::DotProduct(GetActorLocation() - LaneOriginLocation, RunnerRightVector);
}

FVector APGRunnerCharacter::GetLocationAtLaneOffset(float LaneOffset) const
{
	// 전방 진행 거리는 현재 위치에서 유지하고 좌우 오프셋만 라인 기준값으로 교체한다.
	const FVector OriginToActor = GetActorLocation() - LaneOriginLocation;
	const float ForwardDistance = FVector::DotProduct(OriginToActor, RunnerForwardVector);
	return LaneOriginLocation + RunnerForwardVector * ForwardDistance + RunnerRightVector * LaneOffset;
}

void APGRunnerCharacter::SetMeshJumpOffset(float JumpOffset)
{
	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		FVector MeshLocation = InitialMeshRelativeLocation;
		MeshLocation.Z += JumpOffset;
		MeshComponent->SetRelativeLocation(MeshLocation);
	}
}
