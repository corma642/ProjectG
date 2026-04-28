// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/PGRunnerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "DrawDebugHelpers.h"
#include "GameModes/PGGameModeBase.h"
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

	RunnerEnvironmentComponent = CreateDefaultSubobject<UPGRunnerEnvironmentComponent>(TEXT("RunnerEnvironment"));
	RunnerEnvironmentComponent->SetupAttachment(RootComponent);

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
	StartLocationZ = GetActorLocation().Z;
	bIsRunActive = true;

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
		CargoStackComponent->OnCargoLost.AddDynamic(this, &APGRunnerCharacter::HandleCargoLost);
	}

	if (RunnerEnvironmentComponent)
	{
		RunnerEnvironmentComponent->InitializeEnvironment(CachedEnvironmentSettings, CargoStackComponent);
		RunnerEnvironmentComponent->SetRunActive(true);
	}

	if (UWorld* World = GetWorld())
	{
		if (APGGameModeBase* GameMode = World->GetAuthGameMode<APGGameModeBase>())
		{
			// 실패 원인이 추락이든 핵심 적재물 손실이든 러너 정지는 같은 이벤트에서 처리한다.
			GameMode->OnRunFailed.AddDynamic(this, &APGRunnerCharacter::HandleRunFailed);
		}
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

	if (!bIsRunActive)
	{
		return;
	}

	CheckFallFailure();
	if (!bIsRunActive)
	{
		return;
	}

	UpdateCargoPenalty(DeltaSeconds);
	UpdateBaseInstability(DeltaSeconds);
	if (RunnerEnvironmentComponent)
	{
		RunnerEnvironmentComponent->UpdateEnvironment(DeltaSeconds, BalanceInput);
	}

	UpdateForwardMovement();
	const float VisualJumpOffset = RunnerMovementComponent ? RunnerMovementComponent->GetVisualJumpOffset() : 0.0f;
	SetMeshJumpOffset(0.0f);
	if (CargoStackComponent)
	{
		const bool bIsChangingLane = RunnerMovementComponent && RunnerMovementComponent->IsChangingLane();
		CargoStackComponent->UpdateCargoReaction(DeltaSeconds, GetEffectiveBalanceInput(), VisualJumpOffset, bIsChangingLane);
	}
	UpdateBodyLean(DeltaSeconds);
	DrawDebugLanes();
}

void APGRunnerCharacter::RequestLaneChange(int32 Direction)
{
	if (!bIsRunActive)
	{
		return;
	}

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
	if (!bIsRunActive)
	{
		BalanceInput = 0.0f;
		return;
	}

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

void APGRunnerCharacter::HandleCargoLost(int32 SlotIndex, bool bWasCoreCargo)
{
	if (!bWasCoreCargo)
	{
		StartBonusCargoPenalty(SlotIndex);
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (APGGameModeBase* GameMode = World->GetAuthGameMode<APGGameModeBase>())
		{
			// 기본 시작 계란은 핵심 적재물로 취급하므로 손실 시 즉시 실패 흐름으로 넘긴다.
			GameMode->RequestRunFailed(TEXT("CoreCargoLost"));
		}
	}
}

void APGRunnerCharacter::HandleRunFailed(FName FailureReason)
{
	if (!bIsRunActive)
	{
		return;
	}

	// 실패가 확정되면 추가 입력/전진/적재 반응이 누적되지 않도록 러너 상태를 즉시 고정한다.
	bIsRunActive = false;
	BalanceInput = 0.0f;
	BaseInstabilityInput = 0.0f;
	BalanceElapsedTime = 0.0f;
	CargoPenaltyRemainingTime = 0.0f;
	CurrentBodyRoll = 0.0f;

	if (RunnerEnvironmentComponent)
	{
		RunnerEnvironmentComponent->SetRunActive(false);
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}

	SetMeshJumpOffset(0.0f);
	UpdateBodyLean(0.0f);
}

void APGRunnerCharacter::ApplyGameSettings()
{
	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	CachedRunnerSettings = Settings->Runner;
	CachedBalanceSettings = Settings->Balance;
	CachedLaneSettings = Settings->Lane;
	CachedGyroSettings = Settings->Gyro;
	CachedCameraSettings = Settings->Camera;
	CachedDebugSettings = Settings->Debug;
	CachedCargoSettings = Settings->Cargo;
	CachedEnvironmentSettings = Settings->Environment;

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

void APGRunnerCharacter::CheckFallFailure()
{
	const float FallFailZ = StartLocationZ - CachedRunnerSettings.FallFailZOffset;
	if (GetActorLocation().Z >= FallFailZ)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (APGGameModeBase* GameMode = World->GetAuthGameMode<APGGameModeBase>())
		{
			// 추락 실패도 GameMode의 단일 실패 흐름으로 전달해 UI/정산 확장 지점을 하나로 유지한다.
			GameMode->RequestRunFailed(TEXT("Fall"));
		}
	}
}

void APGRunnerCharacter::StartBonusCargoPenalty(int32 LostSlotIndex)
{
	const float PenaltyDuration = FMath::Max(0.0f, CachedCargoSettings.BonusCargoPenaltyDuration);
	CargoPenaltyRemainingTime = PenaltyDuration;
	CargoPenaltyKickDirection = LostSlotIndex % 2 == 0 ? 1.0f : -1.0f;

	if (PenaltyDuration > 0.0f)
	{
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			// 보너스 화물 손실은 즉사 대신 짧은 감속으로 체감시키고, 종료 시 원래 속도로 복구한다.
			const float SpeedScale = FMath::Clamp(CachedCargoSettings.BonusCargoPenaltySpeedScale, 0.1f, 1.0f);
			Movement->MaxWalkSpeed = CachedRunnerSettings.ForwardSpeed * SpeedScale;
		}
	}

	if (UWorld* World = GetWorld())
	{
		if (APGGameModeBase* GameMode = World->GetAuthGameMode<APGGameModeBase>())
		{
			GameMode->RequestCargoPenalty(LostSlotIndex);
		}
	}
}

void APGRunnerCharacter::UpdateCargoPenalty(float DeltaSeconds)
{
	if (CargoPenaltyRemainingTime <= 0.0f)
	{
		return;
	}

	CargoPenaltyRemainingTime = FMath::Max(0.0f, CargoPenaltyRemainingTime - DeltaSeconds);
	if (CargoPenaltyRemainingTime <= 0.0f)
	{
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->MaxWalkSpeed = CachedRunnerSettings.ForwardSpeed;
		}
	}
}

void APGRunnerCharacter::UpdateBaseInstability(float DeltaSeconds)
{
	if (!CachedBalanceSettings.bEnableBaseInstability)
	{
		BaseInstabilityInput = 0.0f;
		return;
	}

	BalanceElapsedTime += DeltaSeconds;

	// 기본 불안정성은 결정적인 사인파로 시작해 같은 조건에서 반복 검증 가능한 줄타기 흔들림을 만든다.
	const float Frequency = FMath::Max(0.0f, CachedBalanceSettings.BaseInstabilityFrequency);
	const float Amplitude = FMath::Max(0.0f, CachedBalanceSettings.BaseInstabilityAmplitude);
	BaseInstabilityInput = FMath::Sin(BalanceElapsedTime * Frequency * UE_TWO_PI) * Amplitude;
}

float APGRunnerCharacter::GetEffectiveBalanceInput() const
{
	const float EnvironmentBalanceInput = RunnerEnvironmentComponent ? RunnerEnvironmentComponent->GetProcessedBalanceInput() : BalanceInput;
	float FinalBalanceInput = BaseInstabilityInput + EnvironmentBalanceInput * CachedBalanceSettings.PlayerCorrectionScale;

	if (CargoPenaltyRemainingTime > 0.0f)
	{
		const float Duration = FMath::Max(KINDA_SMALL_NUMBER, CachedCargoSettings.BonusCargoPenaltyDuration);
		const float PenaltyAlpha = CargoPenaltyRemainingTime / Duration;
		const float PenaltyKick = CargoPenaltyKickDirection * CachedCargoSettings.BonusCargoPenaltyBalanceKick * PenaltyAlpha;
		FinalBalanceInput += PenaltyKick;
	}

	// 환경 효과와 보너스 적재물 페널티를 모두 합산한 뒤 자이로 최대 입력 설정을 최종 제한으로 적용한다.
	return FMath::Clamp(FinalBalanceInput, -CachedGyroSettings.MaxTiltInput, CachedGyroSettings.MaxTiltInput);
}

void APGRunnerCharacter::UpdateBodyLean(float DeltaSeconds)
{
	const float EffectiveBalanceInput = GetEffectiveBalanceInput();
	const float TargetRoll = EffectiveBalanceInput * CachedRunnerSettings.BodyLeanMaxRoll;

	// 중심 입력이 즉시 꺾이지 않고 몸이 따라가는 느낌이 나도록 보간한다.
	CurrentBodyRoll = FMath::FInterpTo(CurrentBodyRoll, TargetRoll, DeltaSeconds, CachedRunnerSettings.BodyLeanInterpSpeed);

	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		// 메시가 BP에서 Z축 회전되어 있어도 러너 캡슐의 전방축 기준으로 좌우 기울기가 보이도록 parent-space 축 회전을 적용한다.
		const FQuat InitialMeshQuat = InitialMeshRelativeRotation.Quaternion();
		const FQuat LeanQuat(FVector::ForwardVector, FMath::DegreesToRadians(CurrentBodyRoll));
		MeshComponent->SetRelativeRotation((LeanQuat * InitialMeshQuat).Rotator());
	}

	DrawBalanceDebug();
}

void APGRunnerCharacter::DrawBalanceDebug() const
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	if (!CachedBalanceSettings.bDrawBalanceDebug || !GEngine)
	{
		return;
	}

	const float EnvironmentBalanceInput = RunnerEnvironmentComponent ? RunnerEnvironmentComponent->GetProcessedBalanceInput() : BalanceInput;
	GEngine->AddOnScreenDebugMessage(
		reinterpret_cast<uint64>(this) + 1,
		0.0f,
		FColor::Yellow,
		FString::Printf(TEXT("Balance Base: %.2f | Player: %.2f | Final: %.2f"), BaseInstabilityInput, EnvironmentBalanceInput, GetEffectiveBalanceInput()));
#endif
}

void APGRunnerCharacter::DrawDebugLanes() const
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
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
#endif
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
