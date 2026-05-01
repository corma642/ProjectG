// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/PGRunnerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
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
	// 러너는 항상 전방을 바라보며 이동하므로 입력 방향 회전을 사용하지 않는다.
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
		// 시작 위치와 방향을 러너 기준 트랙 좌표계로 고정해 월드 X/Y축 의존성을 제거한다.
		RunnerMovementComponent->InitializeRunnerMovement(CachedLaneSettings, CachedGyroSettings, CachedBalanceSettings, GetActorLocation(), GetActorForwardVector(), GetActorRightVector());
	}

	if (CargoStackComponent)
	{
		// CargoStackComponent는 플레이어에 붙은 계란 스폰/손실 판정 앵커로 사용한다.
		CargoStackComponent->OnCargoLost.AddUObject(this, &APGRunnerCharacter::HandleCargoLost);
		CargoStackComponent->OnInitialCargoIntroStarted.AddUObject(this, &APGRunnerCharacter::HandleInitialCargoIntroStarted);
		CargoStackComponent->OnInitialCargoIntroFinished.AddUObject(this, &APGRunnerCharacter::HandleInitialCargoIntroFinished);
		CargoStackComponent->InitializeCargoStack(CachedCargoSettings);
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
			// 추락, 계란 손실 등 모든 실패 사유는 동일한 런 실패 이벤트로 수렴한다.
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

	if (bIsPlayerInputLockedByCargoIntro)
	{
		BaseInstabilityInput = 0.0f;
	}
	else
	{
		UpdateBaseInstability(DeltaSeconds);
	}
	if (RunnerEnvironmentComponent)
	{
		RunnerEnvironmentComponent->UpdateEnvironment(DeltaSeconds, bIsPlayerInputLockedByCargoIntro ? 0.0f : BalanceInput);
	}
	if (RunnerMovementComponent)
	{
		const float PlayerCorrectionInput = bIsPlayerInputLockedByCargoIntro ? 0.0f : (RunnerEnvironmentComponent ? RunnerEnvironmentComponent->GetProcessedBalanceInput() : BalanceInput);
		RunnerMovementComponent->SetBalanceMovementInput(BaseInstabilityInput, PlayerCorrectionInput);
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
	if (!bIsRunActive || bIsPlayerInputLockedByCargoIntro || Direction == 0)
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

	if (bIsPlayerInputLockedByCargoIntro)
	{
		BalanceInput = 0.0f;
		return;
	}

	const float ClampedInput = FMath::Clamp(InBalanceInput, -CachedGyroSettings.MaxTiltInput, CachedGyroSettings.MaxTiltInput);

	// 점프 중이나 착지 직후에는 조작 영향력을 줄여 급격한 흔들림을 완화한다.
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

void APGRunnerCharacter::HandleCargoLost(int32 CargoIndex)
{
	if (UWorld* World = GetWorld())
	{
		if (APGGameModeBase* GameMode = World->GetAuthGameMode<APGGameModeBase>())
		{
			// 모든 계란은 핵심 적재물이므로 하나라도 손실되면 즉시 실패한다.
			GameMode->RequestRunFailed(TEXT("CargoLost"));
		}
	}
}

void APGRunnerCharacter::HandleRunFailed(FName FailureReason)
{
	if (!bIsRunActive)
	{
		return;
	}

	// 실패가 확정되면 추가 입력/전진/환경 효과가 누적되지 않도록 러너 상태를 즉시 고정한다.
	bIsRunActive = false;
	bIsPlayerInputLockedByCargoIntro = false;
	BalanceInput = 0.0f;
	BaseInstabilityInput = 0.0f;
	BalanceElapsedTime = 0.0f;
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

void APGRunnerCharacter::HandleInitialCargoIntroStarted()
{
	if (!bIsRunActive)
	{
		return;
	}

	// 시작 연출 중에는 러너의 자동 전진은 유지하고 플레이어 조작만 막아 첫 실패 원인을 제거한다.
	bIsPlayerInputLockedByCargoIntro = true;
	BalanceInput = 0.0f;
}

void APGRunnerCharacter::HandleInitialCargoIntroFinished()
{
	bIsPlayerInputLockedByCargoIntro = false;
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

	// Project Settings 값은 BeginPlay에서 1회 적용한다. PIE 중 변경한 값은 재시작 후 반영된다.
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

void APGRunnerCharacter::UpdateBaseInstability(float DeltaSeconds)
{
	if (!CachedBalanceSettings.bEnableBaseInstability)
	{
		BaseInstabilityInput = 0.0f;
		return;
	}

	BalanceElapsedTime += DeltaSeconds;

	// 결정론적 사인파를 사용해 같은 조건에서 반복 검증 가능한 기본 줄타기 흔들림을 만든다.
	const float Frequency = FMath::Max(0.0f, CachedBalanceSettings.BaseInstabilityFrequency);
	const float Amplitude = FMath::Max(0.0f, CachedBalanceSettings.BaseInstabilityAmplitude);
	BaseInstabilityInput = FMath::Sin(BalanceElapsedTime * Frequency * UE_TWO_PI) * Amplitude;
}

float APGRunnerCharacter::GetEffectiveBalanceInput() const
{
	const float EnvironmentBalanceInput = RunnerEnvironmentComponent ? RunnerEnvironmentComponent->GetProcessedBalanceInput() : BalanceInput;
	const float FinalBalanceInput = BaseInstabilityInput + EnvironmentBalanceInput * CachedBalanceSettings.PlayerCorrectionScale;

	// 환경 효과와 기본 불안정성을 모두 합산한 뒤 자이로 최대 입력 설정으로 최종 제한한다.
	return FMath::Clamp(FinalBalanceInput, -CachedGyroSettings.MaxTiltInput, CachedGyroSettings.MaxTiltInput);
}

void APGRunnerCharacter::UpdateBodyLean(float DeltaSeconds)
{
	const float EffectiveBalanceInput = GetEffectiveBalanceInput();
	const float TargetRoll = -EffectiveBalanceInput * CachedRunnerSettings.BodyLeanMaxRoll;

	// 중심 입력이 즉시 꺾이지 않고 몸이 따라가는 느낌이 나도록 보간한다.
	CurrentBodyRoll = FMath::FInterpTo(CurrentBodyRoll, TargetRoll, DeltaSeconds, CachedRunnerSettings.BodyLeanInterpSpeed);

	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		// BP에서 메시가 Z축 회전되어 있어도 캡슐 전방축 기준으로 좌우 기울기가 보이게 한다.
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
	const float BalanceOffset = RunnerMovementComponent ? RunnerMovementComponent->GetBalanceOffset() : 0.0f;
	const float BalanceVelocity = RunnerMovementComponent ? RunnerMovementComponent->GetBalanceVelocity() : 0.0f;
	GEngine->AddOnScreenDebugMessage(
		reinterpret_cast<uint64>(this) + 1,
		0.0f,
		FColor::Yellow,
		FString::Printf(TEXT("Balance Base: %.2f | Player: %.2f | Final: %.2f | Offset: %.1f | Speed: %.1f"), BaseInstabilityInput, EnvironmentBalanceInput, GetEffectiveBalanceInput(), BalanceOffset, BalanceVelocity));
#endif
}

void APGRunnerCharacter::DrawDebugLanes() const
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	if (!CachedDebugSettings.bDrawLaneDebug || !RunnerMovementComponent)
	{
		return;
	}

	constexpr float DebugForwardLength = 1000.0f;
	constexpr float DebugBackLength = 200.0f;
	constexpr float DebugHeight = 8.0f;
	const FVector DebugUpOffset = FVector::UpVector * DebugHeight;

	for (int32 LaneIndex = 0; LaneIndex < CachedLaneSettings.LaneCount; ++LaneIndex)
	{
		// 목표 라인은 초록색으로 표시해 라인 보간 상태를 빠르게 확인한다.
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
