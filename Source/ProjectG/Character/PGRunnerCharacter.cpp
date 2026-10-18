// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/PGRunnerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Cargo/PGCargoStackComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Environment/PGRunnerEnvironmentComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameModes/PGGameModeBase.h"
#include "Audio/PGAudioStatics.h"
#include "Movement/PGRunnerMovementComponent.h"
#include "RunProgress/PGRunProgressComponent.h"
#include "Settings/PGGameSettings.h"
#include "Shop/PGCharacterSkinDefinition.h"
#include "Sound/SoundBase.h"
#include "User/PGUserDataSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGRunnerCharacter, Log, All);

APGRunnerCharacter::APGRunnerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UPGRunnerMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

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

	RunProgressComponent = CreateDefaultSubobject<UPGRunProgressComponent>(TEXT("RunProgress"));

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

	// 바구니는 머리 소켓에 붙어 있어 이번 프레임 위치가 애니메이션 평가 뒤에야 정해진다.
	// 적재물 속도를 바구니의 이번 프레임 이동량에 정확히 맞추려면 러너 Tick이 그 뒤에 돌아야 한다.
	// 이때 애니메이션이 물리 계산 전에 끝나야 속도가 같은 프레임 물리에 반영되므로 DefaultEngine.ini의 tick.AnimationDelaysEndGroup=0과 짝을 이룬다.
	AddTickPrerequisiteComponent(GetMesh());

	ApplyGameSettings();
	HomePreviewTransform = GetActorTransform();
	bHasHomePreviewTransform = true;
	StartLocationZ = GetActorLocation().Z;
	RunStartLocation = GetActorLocation();
	RunStartForwardVector = GetActorForwardVector().GetSafeNormal();
	RunStartTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	bIsRunActive = false;
	bIsRunCountdownActive = false;
	bIsGameplayInputLocked = true;

	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();

	if (!RunnerMovementComponent)
	{
		RunnerMovementComponent = Cast<UPGRunnerMovementComponent>(GetCharacterMovement());
	}

	if (RunnerMovementComponent)
	{
		// 시작 위치와 방향을 러너 기준 트랙 좌표계로 고정해 월드 X/Y축 의존성을 제거한다.
		RunnerMovementComponent->InitializeRunnerMovement(CachedJumpSettings, CachedTouchControlSettings, CachedBalanceSettings, GetActorLocation(), GetActorForwardVector(), GetActorRightVector());
	}

	if (CargoStackComponent)
	{
		// CargoStackComponent는 플레이어에 붙은 적재물 스폰/대기/속도 보정 앵커로 사용한다.
		CargoStackComponent->OnCargoLost.AddUObject(this, &APGRunnerCharacter::HandleCargoLost);
		CargoStackComponent->OnCargoDelivered.AddUObject(this, &APGRunnerCharacter::HandleCargoDelivered);
		CargoStackComponent->InitializeCargoStack(CachedCargoSettings);
	}

	if (RunnerEnvironmentComponent)
	{
		RunnerEnvironmentComponent->InitializeEnvironment(CachedEnvironmentSettings);
		RunnerEnvironmentComponent->SetRunActive(false);
	}

	if (UWorld* World = GetWorld())
	{
		if (APGGameModeBase* GameMode = World->GetAuthGameMode<APGGameModeBase>())
		{
			// 추락, 적재물 손실 등 모든 실패 사유는 동일한 런 실패 이벤트로 수렴한다.
			GameMode->OnRunCountdownStarted.AddDynamic(this, &APGRunnerCharacter::HandleRunCountdownStarted);
			GameMode->OnRunStarted.AddDynamic(this, &APGRunnerCharacter::HandleRunStarted);
			GameMode->OnRunFailed.AddDynamic(this, &APGRunnerCharacter::HandleRunFailed);

			if (GameMode->GetRunState() == EPGRunState::Countdown)
			{
				HandleRunCountdownStarted(Settings ? Settings->HomeFlow.RunStartCountdownDuration : 3.0f);
			}
			else if (GameMode->GetRunState() == EPGRunState::Running)
			{
				HandleRunStarted();
			}
		}
	}

	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		InitialMeshRelativeRotation = MeshComponent->GetRelativeRotation();
	}
}

void APGRunnerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DetachAndDestroyBowl();
	Super::EndPlay(EndPlayReason);
}

void APGRunnerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bIsRunActive)
	{
		return;
	}

	if (bIsRunCountdownActive)
	{
		// 카운트다운은 화면 연출 중에도 러너가 앞으로 나아가게 하되, 중심 입력과 적재물 흔들림은 시작 전까지 잠근다.
		BaseInstabilityInput = 0.0f;
		BalanceInput = 0.0f;
		FlickBalanceInput = 0.0f;
		FlickBalanceInputRemainingTime = 0.0f;
		if (RunnerMovementComponent)
		{
			RunnerMovementComponent->SetBalanceMovementInput(0.0f, 0.0f);
		}

		UpdateForwardMovement();
		UpdateBodyLean(DeltaSeconds);
		return;
	}

	CheckFallFailure();
	if (!bIsRunActive)
	{
		return;
	}

	if (RunProgressComponent)
	{
		RunProgressComponent->UpdateRunProgress(GetActorLocation());
	}

	UpdateBaseInstability(DeltaSeconds);
	if (FlickBalanceInputRemainingTime > 0.0f)
	{
		FlickBalanceInputRemainingTime = FMath::Max(0.0f, FlickBalanceInputRemainingTime - DeltaSeconds);
		if (FlickBalanceInputRemainingTime <= KINDA_SMALL_NUMBER)
		{
			FlickBalanceInputRemainingTime = 0.0f;
			FlickBalanceInput = 0.0f;
		}
	}

	if (RunnerEnvironmentComponent)
	{
		RunnerEnvironmentComponent->UpdateEnvironment(DeltaSeconds, GetLandingScaledBalanceInput());
	}
	if (RunnerMovementComponent)
	{
		// 환경 효과는 플레이어 보정 입력만 가공한다. 기본 불안정성은 별도 축으로 유지해 눈바람이 자동 실패를 만들지 않게 한다.
		const float PlayerCorrectionInput = (RunnerEnvironmentComponent ? RunnerEnvironmentComponent->GetProcessedBalanceInput() : GetLandingScaledBalanceInput()) + FlickBalanceInput;
		RunnerMovementComponent->SetBalanceMovementInput(BaseInstabilityInput, PlayerCorrectionInput);
	}

	UpdateForwardMovement();

	// 몸 기울기는 메시를 돌려 바구니까지 옮기므로, 적재물이 바구니의 최종 위치를 보도록 먼저 갱신한다.
	UpdateBodyLean(DeltaSeconds);
	if (CargoStackComponent)
	{
		const bool bIsAirborne = RunnerMovementComponent && RunnerMovementComponent->IsFalling();
		CargoStackComponent->UpdateCargoReaction(DeltaSeconds, GetCargoBalanceInputForPhysics(), bIsAirborne);
	}
}

bool APGRunnerCharacter::RequestJump()
{
	if (!bIsRunActive || bIsGameplayInputLocked)
	{
		return false;
	}

	if (RunnerMovementComponent && RunnerMovementComponent->RequestJump())
	{
		// 사운드는 입력 요청이 아니라 실제 점프가 실행된 경우에만 재생한다.
		PlayJumpSound();
		OnJumpStarted.Broadcast();
		return true;
	}

	return false;
}

void APGRunnerCharacter::RequestFlickJump(float DirectionSign)
{
	if (!RequestJump())
	{
		return;
	}

	const float ClampedDirection = FMath::Clamp(DirectionSign, -1.0f, 1.0f);
	if (FMath::IsNearlyZero(ClampedDirection))
	{
		return;
	}

	// 좌우 플릭은 점프가 실제로 시작된 경우에만 기존 Balance 이동 위에 짧은 보정으로 얹는다.
	// 시각적 몸 기울기도 같은 시간 동안 유지해 순간이동처럼 보이지 않게 한다.
	FlickBalanceInput = ClampedDirection;
	FlickBalanceInputRemainingTime = FMath::Max(0.0f, CachedTouchControlSettings.FlickLateralMoveDuration);
	if (RunnerMovementComponent)
	{
		RunnerMovementComponent->ApplyFlickLateralStep(ClampedDirection);
	}
}

void APGRunnerCharacter::SetBalanceInput(float InBalanceInput)
{
	if (!bIsRunActive || bIsGameplayInputLocked)
	{
		BalanceInput = 0.0f;
		return;
	}

	const float MaxBalanceInput = FMath::Max(0.0f, CachedTouchControlSettings.MaxBalanceInput);
	BalanceInput = FMath::Clamp(InBalanceInput, -MaxBalanceInput, MaxBalanceInput);
}

float APGRunnerCharacter::GetLandingScaledBalanceInput() const
{
	// 착지 직후에만 조작 영향력을 잠깐 줄여 급격한 흔들림을 완화한다.
	// 손가락을 멈춘 채 착지하면 입력 이벤트가 오지 않으므로, 입력 시점이 아니라 사용하는 프레임마다 적용한다.
	const bool bIsLandingStabilizing = RunnerMovementComponent && RunnerMovementComponent->GetLandingStabilizeRemainingTime() > 0.0f;
	return BalanceInput * (bIsLandingStabilizing ? 0.5f : 1.0f);
}

float APGRunnerCharacter::GetRunElapsedTime() const
{
	if (RunProgressComponent)
	{
		return RunProgressComponent->GetElapsedTimeSeconds();
	}

	const UWorld* World = GetWorld();
	return World ? FMath::Max(0.0f, World->GetTimeSeconds() - RunStartTimeSeconds) : 0.0f;
}

float APGRunnerCharacter::GetRunDistanceMeters() const
{
	if (RunProgressComponent)
	{
		return RunProgressComponent->GetDisplayedDistanceMeters();
	}

	const FVector RunDelta = GetActorLocation() - RunStartLocation;
	const float DistanceCm = FVector::DotProduct(RunDelta, RunStartForwardVector);
	return FMath::Max(0.0f, DistanceCm * 0.01f);
}

int32 APGRunnerCharacter::GetRunScore() const
{
	return RunProgressComponent ? RunProgressComponent->GetScore() : 0;
}

int32 APGRunnerCharacter::GetRunReceipts() const
{
	return RunProgressComponent ? RunProgressComponent->GetReceipts() : 0;
}

int32 APGRunnerCharacter::GetDeliveredCargoCount() const
{
	return CargoStackComponent ? CargoStackComponent->GetDeliveredCargoCount() : 0;
}

bool APGRunnerCharacter::ApplyCharacterSkin(const UPGCharacterSkinDefinition* SkinDefinition)
{
	USkeletalMeshComponent* MeshComponent = GetMesh();
	USkeletalMesh* LoadedMesh = SkinDefinition ? SkinDefinition->SkeletalMesh.Get() : nullptr;
	if (!MeshComponent || !LoadedMesh)
	{
		return false;
	}

	// 선택된 스킨 리소스가 준비된 뒤에만 교체합니다. 로드 중에는 기존 프리뷰 메시를 유지합니다.
	MeshComponent->SetSkeletalMesh(LoadedMesh);
	if (UClass* LoadedAnimClass = SkinDefinition->AnimClass.Get())
	{
		MeshComponent->SetAnimInstanceClass(LoadedAnimClass);
	}

	// 캐릭터별 피벗/스케일은 DCC에서 통일하고, BP_Runner의 Mesh 상대 Transform을 단일 기준으로 사용한다.
	CurrentCharacterSkinDefinition = const_cast<UPGCharacterSkinDefinition*>(SkinDefinition);
	InitialMeshRelativeRotation = MeshComponent->GetRelativeRotation();
	CurrentBodyRoll = 0.0f;
	return true;
}

void APGRunnerCharacter::PrepareTransformForRunStart(float ForwardOffset)
{
	// Home/Shop에서는 자유로운 프리뷰 회전을 쓰지만, 실제 러닝은 월드 0 회전(+X) 기준으로 시작한다.
	// Retry는 카운트다운이 짧은 만큼 덜 달리므로 그 거리를 앞에서 출발시켜 GO 지점을 첫 START와 같게 맞춘다. 시작 구간 바닥은 평탄하다는 전제라 Z는 그대로 둔다.
	const FVector CurrentLocation = GetActorLocation() + FVector(ForwardOffset, 0.0f, 0.0f);
	const FRotator RunStartRotation = FRotator::ZeroRotator;

	SetActorLocationAndRotation(CurrentLocation, RunStartRotation, false, nullptr, ETeleportType::TeleportPhysics);
	if (Controller)
	{
		Controller->SetControlRotation(RunStartRotation);
	}

	BalanceInput = 0.0f;
	BaseInstabilityInput = 0.0f;
	FlickBalanceInput = 0.0f;
	FlickBalanceInputRemainingTime = 0.0f;
	BalanceElapsedTime = 0.0f;
	CurrentBodyRoll = 0.0f;
	StartLocationZ = CurrentLocation.Z;
	RunStartLocation = CurrentLocation;
	RunStartForwardVector = GetActorForwardVector().GetSafeNormal();
	RunStartTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		MeshComponent->SetRelativeRotation(InitialMeshRelativeRotation);
	}

	if (RunnerMovementComponent)
	{
		// 회전이 바뀐 직후 이동 컴포넌트의 전방/우측 기준도 다시 잡아야 자동 전진과 Balance 이동이 같은 축을 쓴다.
		RunnerMovementComponent->InitializeRunnerMovement(CachedJumpSettings, CachedTouchControlSettings, CachedBalanceSettings, CurrentLocation, GetActorForwardVector(), GetActorRightVector());
		RunnerMovementComponent->SetBalanceMovementInput(0.0f, 0.0f);
		RunnerMovementComponent->StopMovementImmediately();
	}
}

void APGRunnerCharacter::RestoreHomePreviewTransform()
{
	if (!bHasHomePreviewTransform)
	{
		return;
	}

	// 현재는 Home 복귀가 레벨 리로드 기반이지만, 향후 무로딩 복귀로 바뀌어도 Home 프리뷰 자세가 유지되도록 명시 복구한다.
	SetActorTransform(HomePreviewTransform, false, nullptr, ETeleportType::TeleportPhysics);
	if (Controller)
	{
		Controller->SetControlRotation(HomePreviewTransform.GetRotation().Rotator());
	}

	BalanceInput = 0.0f;
	BaseInstabilityInput = 0.0f;
	FlickBalanceInput = 0.0f;
	FlickBalanceInputRemainingTime = 0.0f;
	BalanceElapsedTime = 0.0f;
	CurrentBodyRoll = 0.0f;

	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		MeshComponent->SetRelativeRotation(InitialMeshRelativeRotation);
	}
}

void APGRunnerCharacter::HandleCargoLost(int32 CargoIndex)
{
	if (RunProgressComponent)
	{
		RunProgressComponent->StopRunProgress(GetActorLocation());
	}

	if (UWorld* World = GetWorld())
	{
		if (APGGameModeBase* GameMode = World->GetAuthGameMode<APGGameModeBase>())
		{
			// 현재 적재물은 모두 핵심 적재물이므로 하나라도 손실되면 즉시 실패한다.
			GameMode->RequestRunFailed(TEXT("CargoLost"));
		}
	}
}

void APGRunnerCharacter::HandleCargoDelivered(int32 CargoInstanceId, int32 Score, int32 Receipts, FVector DeliveryLocation)
{
	if (RunProgressComponent)
	{
		RunProgressComponent->AddScore(Score, TEXT("CargoDelivered"));
		RunProgressComponent->AddReceipts(Receipts, TEXT("CargoDelivered"));
	}
}

void APGRunnerCharacter::PlayJumpSound()
{
	if (!CurrentCharacterSkinDefinition)
	{
		return;
	}

	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	const TArray<TSoftObjectPtr<USoundBase>>* JumpSounds = nullptr;
	if (Settings)
	{
		JumpSounds = CurrentCharacterSkinDefinition->Gender == EPGCharacterGender::Female
			? &Settings->SFX.FemaleJumpSounds
			: &Settings->SFX.MaleJumpSounds;
	}

	if (!JumpSounds || JumpSounds->IsEmpty())
	{
		return;
	}

	TArray<USoundBase*> LoadedSounds;
	LoadedSounds.Reserve(JumpSounds->Num());
	for (const TSoftObjectPtr<USoundBase>& JumpSound : *JumpSounds)
	{
		// 런타임 예열된 사운드만 사용한다. 여기서 동기 로드하지 않아 Android 첫 점프 hitch를 만들지 않는다.
		if (USoundBase* LoadedSound = JumpSound.Get())
		{
			LoadedSounds.Add(LoadedSound);
		}
	}

	if (!LoadedSounds.IsEmpty())
	{
		UPGAudioStatics::PlaySFXAtLocation(this, LoadedSounds[FMath::RandRange(0, LoadedSounds.Num() - 1)], GetActorLocation());
	}
}

void APGRunnerCharacter::HandleRunCountdownStarted(float Duration)
{
	if (!IsPlayerControlled())
	{
		return;
	}

	SetActorTickEnabled(true);

	bIsRunActive = true;
	bIsRunCountdownActive = true;
	bIsGameplayInputLocked = true;
	BalanceInput = 0.0f;
	BaseInstabilityInput = 0.0f;
	FlickBalanceInput = 0.0f;
	FlickBalanceInputRemainingTime = 0.0f;
	BalanceElapsedTime = 0.0f;
	CurrentBodyRoll = 0.0f;
	StartLocationZ = GetActorLocation().Z;

	if (RunnerEnvironmentComponent)
	{
		RunnerEnvironmentComponent->SetRunActive(false);
		RunnerEnvironmentComponent->ClearEnvironmentEffects();
	}

	if (RunnerMovementComponent)
	{
		RunnerMovementComponent->SetBalanceMovementInput(0.0f, 0.0f);
	}

	if (RunProgressComponent)
	{
		// 카운트다운 중 이동 거리는 점수/거리 계산에 포함하지 않는다.
		RunProgressComponent->StopRunProgress(GetActorLocation());
	}

	AttachBowlForRun();
}

void APGRunnerCharacter::HandleRunStarted()
{
	if (!IsPlayerControlled())
	{
		return;
	}

	SetActorTickEnabled(true);

	bIsRunActive = true;
	bIsRunCountdownActive = false;
	bIsGameplayInputLocked = false;
	BalanceInput = 0.0f;
	BaseInstabilityInput = 0.0f;
	FlickBalanceInput = 0.0f;
	FlickBalanceInputRemainingTime = 0.0f;
	BalanceElapsedTime = 0.0f;
	StartLocationZ = GetActorLocation().Z;
	RunStartLocation = GetActorLocation();
	RunStartForwardVector = (RunnerMovementComponent ? RunnerMovementComponent->GetRunnerForwardVector() : GetActorForwardVector()).GetSafeNormal();
	RunStartTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	if (RunProgressComponent)
	{
		// 실제 점수/거리 기준점은 GO 이후 현재 위치와 전방 벡터로 다시 잡는다.
		const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
		RunProgressComponent->InitializeRunProgress(Settings ? Settings->RunProgress : FPGRunProgressSettings(), RunStartLocation, RunStartForwardVector, RunStartTimeSeconds);
	}

	if (RunnerEnvironmentComponent)
	{
		RunnerEnvironmentComponent->SetRunActive(true);
	}

	// 첫 적재물도 CargoDeliverySubsystem의 일반 드롭 흐름으로 처리한다.
}

void APGRunnerCharacter::HandleRunFailed(FName FailureReason)
{
	if (!bIsRunActive)
	{
		return;
	}

	// 실패가 확정되면 추가 입력/전진/환경 효과가 누적되지 않도록 러너 상태를 즉시 고정한다.
	if (RunProgressComponent)
	{
		RunProgressComponent->StopRunProgress(GetActorLocation());
	}

	bIsRunActive = false;
	bIsRunCountdownActive = false;
	bIsGameplayInputLocked = true;
	BalanceInput = 0.0f;
	BaseInstabilityInput = 0.0f;
	FlickBalanceInput = 0.0f;
	FlickBalanceInputRemainingTime = 0.0f;
	BalanceElapsedTime = 0.0f;
	CurrentBodyRoll = 0.0f;

	if (RunnerEnvironmentComponent)
	{
		RunnerEnvironmentComponent->SetRunActive(false);
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		if (UPGRunnerMovementComponent* RunnerMovement = Cast<UPGRunnerMovementComponent>(Movement))
		{
			RunnerMovement->StopRunnerMovementForFailure();
		}
		else
		{
			Movement->StopMovementImmediately();
			Movement->DisableMovement();
		}
	}

	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		// 실패 순간 자세로 멈춰 적재물이 쏟아지는 원인이 보이게 한다. 메시와 바구니는 결과 화면이 뜰 때 HideForRunResult가 정리한다.
		MeshComponent->bPauseAnims = true;
	}

	UpdateBodyLean(0.0f);
	SetActorTickEnabled(false);
}

void APGRunnerCharacter::HideForRunResult()
{
	DetachAndDestroyBowl();

	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		MeshComponent->SetHiddenInGame(true, true);
		MeshComponent->SetVisibility(false, true);
	}
}

void APGRunnerCharacter::ApplyGameSettings()
{
	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	CachedRunnerSettings = Settings->Runner;
	CachedBalanceSettings = Settings->Balance;
	CachedJumpSettings = Settings->Jump;
	CachedTouchControlSettings = Settings->TouchControl;
	CachedCameraSettings = Settings->Camera;
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

void APGRunnerCharacter::AttachBowlForRun()
{
	if (IsValid(AttachedBowlActor))
	{
		return;
	}

	USkeletalMeshComponent* MeshComponent = GetMesh();
	UWorld* World = GetWorld();
	if (!World || !MeshComponent || !BowlActorClass || BowlSocketName.IsNone())
	{
		return;
	}

	if (!MeshComponent->DoesSocketExist(BowlSocketName))
	{
		UE_LOG(LogPGRunnerCharacter, Warning, TEXT("Bowl attach failed: socket '%s' was not found on mesh '%s'."), *BowlSocketName.ToString(), *GetNameSafe(MeshComponent->GetSkeletalMeshAsset()));
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.Instigator = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* NewBowlActor = World->SpawnActor<AActor>(BowlActorClass, FTransform::Identity, SpawnParameters);
	if (!NewBowlActor)
	{
		return;
	}

	NewBowlActor->AttachToComponent(
		MeshComponent,
		FAttachmentTransformRules(EAttachmentRule::SnapToTarget, EAttachmentRule::KeepRelative, EAttachmentRule::KeepRelative, true),
		BowlSocketName);
	NewBowlActor->SetActorEnableCollision(true);
	AttachedBowlActor = NewBowlActor;

	if (CargoStackComponent)
	{
		// 적재물 속도 보정이 캡슐이 아니라 실제로 적재물을 받치는 바구니의 움직임을 따르게 한다.
		CargoStackComponent->SetSupportComponent(NewBowlActor->GetRootComponent());
	}
}

void APGRunnerCharacter::DetachAndDestroyBowl()
{
	if (!IsValid(AttachedBowlActor))
	{
		AttachedBowlActor = nullptr;
		return;
	}

	if (CargoStackComponent)
	{
		CargoStackComponent->SetSupportComponent(nullptr);
	}

	AttachedBowlActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	AttachedBowlActor->Destroy();
	AttachedBowlActor = nullptr;
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
			if (RunProgressComponent)
			{
				RunProgressComponent->StopRunProgress(GetActorLocation());
			}

			GameMode->RequestRunFailed(TEXT("Fall"));
		}
	}
}

void APGRunnerCharacter::UpdateBaseInstability(float DeltaSeconds)
{
	BalanceElapsedTime += DeltaSeconds;

	// 결정론적 사인파를 사용해 같은 조건에서 반복 검증 가능한 기본 줄타기 흔들림을 만든다.
	const float Frequency = FMath::Max(0.0f, CachedBalanceSettings.BaseInstabilityFrequency);
	const float Amplitude = FMath::Max(0.0f, CachedBalanceSettings.BaseInstabilityAmplitude);
	BaseInstabilityInput = FMath::Sin(BalanceElapsedTime * Frequency * UE_TWO_PI) * Amplitude;
}

float APGRunnerCharacter::GetEffectiveBalanceInput() const
{
	const float EnvironmentBalanceInput = RunnerEnvironmentComponent ? RunnerEnvironmentComponent->GetProcessedBalanceInput() : GetLandingScaledBalanceInput();
	const float FinalBalanceInput = BaseInstabilityInput + EnvironmentBalanceInput + FlickBalanceInput;

	const float MaxBalanceInput = FMath::Max(0.0f, CachedTouchControlSettings.MaxBalanceInput);
	return FMath::Clamp(FinalBalanceInput, -MaxBalanceInput, MaxBalanceInput);
}

float APGRunnerCharacter::GetCargoBalanceInputForPhysics() const
{
	if (RunnerEnvironmentComponent && RunnerEnvironmentComponent->IsSnowstormActive())
	{
		// 눈바람 중에는 기본 흔들림은 그대로 두고, 플레이어 입력으로 인한 적재물 반응만 더 민감하게 만든다.
		return BaseInstabilityInput + RunnerEnvironmentComponent->GetProcessedCargoBalanceInput();
	}

	return GetEffectiveBalanceInput();
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
}

