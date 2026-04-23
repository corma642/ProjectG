// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/PGRunnerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "DrawDebugHelpers.h"
#include "Settings/PGGameSettings.h"

APGRunnerCharacter::APGRunnerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UPGRunnerMovementComponent>(ACharacter::CharacterMovementComponentName))
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

	CargoStackComponent = CreateDefaultSubobject<UPGCargoStackComponent>(TEXT("CargoStack"));
	CargoStackComponent->SetupAttachment(RootComponent);

	RunnerMovementComponent = CastChecked<UPGRunnerMovementComponent>(GetCharacterMovement());
	UCharacterMovementComponent* Movement = RunnerMovementComponent;
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

	if (!RunnerMovementComponent)
	{
		RunnerMovementComponent = Cast<UPGRunnerMovementComponent>(GetCharacterMovement());
	}

	if (RunnerMovementComponent)
	{
		// 시작 위치와 방향을 러너 기준 트랙 좌표계로 고정해 월드 X/Y축 의존을 제거한다.
		RunnerMovementComponent->InitializeRunnerMovement(CachedLaneSettings, CachedGyroSettings, GetActorLocation(), GetActorForwardVector(), GetActorRightVector());
	}

	if (CargoStackComponent)
	{
		// 실제 계란 메시가 없어도 설정 기반 적재 슬롯을 먼저 구성해 Sprint 2 핵심 반응을 검증한다.
		CargoStackComponent->InitializeCargoStack(CachedCargoSettings);
	}

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
	const float VisualJumpOffset = RunnerMovementComponent ? RunnerMovementComponent->GetVisualJumpOffset() : 0.0f;
	SetMeshJumpOffset(VisualJumpOffset);
	if (CargoStackComponent)
	{
		const bool bIsChangingLane = RunnerMovementComponent && RunnerMovementComponent->IsChangingLane();
		CargoStackComponent->UpdateCargoReaction(DeltaSeconds, BalanceInput, VisualJumpOffset, bIsChangingLane);
	}
	UpdateBodyLean(DeltaSeconds);
	DrawDebugLanes();
}

void APGRunnerCharacter::RequestLaneChange(int32 Direction)
{
	if (Direction == 0)
	{
		return;
	}

	if (RunnerMovementComponent)
	{
		RunnerMovementComponent->RequestLaneChange(Direction);
	}
}

void APGRunnerCharacter::SetBalanceInput(float InBalanceInput)
{
	const float ClampedInput = FMath::Clamp(InBalanceInput, -CachedGyroSettings.MaxTiltInput, CachedGyroSettings.MaxTiltInput);

	// 점프 중/착지 직후에는 조작 피로를 줄이기 위해 자이로 영향력을 완화한다.
	const bool bIsChangingLane = RunnerMovementComponent && RunnerMovementComponent->IsChangingLane();
	const bool bIsLandingStabilizing = RunnerMovementComponent && RunnerMovementComponent->GetLandingStabilizeRemainingTime() > 0.0f;
	const float DampingScale = bIsChangingLane ? CachedGyroSettings.JumpTiltDampingScale : 1.0f;
	const float LandingScale = bIsLandingStabilizing ? 0.5f : 1.0f;

	BalanceInput = ClampedInput * DampingScale * LandingScale;
}

int32 APGRunnerCharacter::GetCurrentLaneIndex() const
{
	return RunnerMovementComponent ? RunnerMovementComponent->GetCurrentLaneIndex() : 0;
}

int32 APGRunnerCharacter::GetTargetLaneIndex() const
{
	return RunnerMovementComponent ? RunnerMovementComponent->GetTargetLaneIndex() : 0;
}

void APGRunnerCharacter::ApplyGameSettings()
{
	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	CachedRunnerSettings = Settings->Runner;
	CachedLaneSettings = Settings->Lane;
	CachedGyroSettings = Settings->Gyro;
	CachedCameraSettings = Settings->Camera;
	CachedDebugSettings = Settings->Debug;
	CachedCargoSettings = Settings->Cargo;

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
	const FVector ForwardVector = RunnerMovementComponent ? RunnerMovementComponent->GetRunnerForwardVector() : GetActorForwardVector();
	AddMovementInput(ForwardVector, 1.0f);
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

	if (!RunnerMovementComponent)
	{
		return;
	}

	for (int32 LaneIndex = 0; LaneIndex < CachedLaneSettings.LaneCount; ++LaneIndex)
	{
		// 목표 라인은 초록색으로 표시해 이동 중 도착 라인을 쉽게 확인한다.
		const FVector LaneLocation = RunnerMovementComponent->GetLaneWorldLocation(LaneIndex);
		const FVector ForwardVector = RunnerMovementComponent->GetRunnerForwardVector();
		const FVector Start = LaneLocation - ForwardVector * DebugBackLength + DebugUpOffset;
		const FVector End = LaneLocation + ForwardVector * DebugForwardLength + DebugUpOffset;
		const FColor LaneColor = LaneIndex == RunnerMovementComponent->GetTargetLaneIndex() ? FColor::Green : FColor::Silver;
		DrawDebugLine(GetWorld(), Start, End, LaneColor, false, 0.0f, 0, 2.0f);
	}
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
