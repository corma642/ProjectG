// Copyright Epic Games, Inc. All Rights Reserved.

#include "RunEvents/PGMovingRunEventHazardActor.h"

#include "Audio/PGAudioStatics.h"
#include "Camera/CameraShakeBase.h"
#include "Cargo/PGCargoStackComponent.h"
#include "Character/PGRunnerCharacter.h"
#include "Components/BoxComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameModes/PGGameModeBase.h"
#include "Movement/PGRunnerMovementComponent.h"

APGMovingRunEventHazardActor::APGMovingRunEventHazardActor()
{
	PrimaryActorTick.bCanEverTick = true;

	HazardRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(HazardRoot);

	OverlapBox = CreateDefaultSubobject<UBoxComponent>(TEXT("OverlapBox"));
	OverlapBox->SetupAttachment(HazardRoot);
	OverlapBox->SetBoxExtent(FVector(160.0f, 160.0f, 180.0f));
	OverlapBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	OverlapBox->SetCollisionObjectType(ECC_WorldDynamic);
	OverlapBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	OverlapBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	OverlapBox->SetGenerateOverlapEvents(true);
}

void APGMovingRunEventHazardActor::InitializeMovingHazard(const FPGMovingHazardSettings& InSettings, const FVector& InMovementVelocity, float InLifetime, float InStrength, float InPushDirectionSign)
{
	CachedSettings = InSettings;
	MovementVelocity = InMovementVelocity;
	RemainingLifetime = FMath::Max(0.1f, InLifetime);
	Strength = FMath::Max(0.0f, InStrength);
	PushDirectionSign = FMath::IsNearlyZero(InPushDirectionSign) ? 0.0f : FMath::Sign(InPushDirectionSign);
	LastHitWorldTime = -FLT_MAX;
}

void APGMovingRunEventHazardActor::BeginPlay()
{
	Super::BeginPlay();

	if (OverlapBox)
	{
		OverlapBox->OnComponentBeginOverlap.AddDynamic(this, &APGMovingRunEventHazardActor::HandleHazardOverlap);
	}
}

void APGMovingRunEventHazardActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (OverlapBox)
	{
		OverlapBox->OnComponentBeginOverlap.RemoveDynamic(this, &APGMovingRunEventHazardActor::HandleHazardOverlap);
	}

	Super::EndPlay(EndPlayReason);
}

void APGMovingRunEventHazardActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (DeltaSeconds <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// 위험 Actor는 짧은 수명 동안만 월드 이동합니다. 세그먼트 풀링 대상이 아니므로 수명 종료 시 제거합니다.
	AddActorWorldOffset(MovementVelocity * DeltaSeconds, true);

	RemainingLifetime -= DeltaSeconds;
	if (RemainingLifetime <= 0.0f)
	{
		Destroy();
	}
}

void APGMovingRunEventHazardActor::HandleHazardOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APGRunnerCharacter* RunnerCharacter = Cast<APGRunnerCharacter>(OtherActor);
	const float PushImpulseVelocity = FMath::Max(0.0f, CachedSettings.PushImpulseVelocity) * Strength;
	if (!RunnerCharacter || PushImpulseVelocity <= 0.0f)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const APGGameModeBase* GameMode = World->GetAuthGameMode<APGGameModeBase>();
	if (!GameMode || GameMode->GetRunState() != EPGRunState::Running)
	{
		return;
	}

	const float CurrentTime = World->GetTimeSeconds();
	if (CurrentTime - LastHitWorldTime < FMath::Max(0.0f, CachedSettings.RehitCooldown))
	{
		return;
	}

	UPGRunnerMovementComponent* RunnerMovement = Cast<UPGRunnerMovementComponent>(RunnerCharacter->GetCharacterMovement());
	if (!RunnerMovement)
	{
		return;
	}

	const float DirectionSign = ResolvePushDirectionSign(*RunnerCharacter);
	if (FMath::IsNearlyZero(DirectionSign))
	{
		return;
	}

	LastHitWorldTime = CurrentTime;
	RunnerMovement->ApplyExternalBalanceImpulse(DirectionSign * PushImpulseVelocity, FMath::Max(0.0f, CachedSettings.MaxPushBalanceVelocity));
	PlayHitFeedback(*RunnerCharacter, DirectionSign);
}

void APGMovingRunEventHazardActor::PlayHitFeedback(APGRunnerCharacter& RunnerCharacter, float DirectionSign) const
{
	// 소프트 참조는 Run 예열 그룹이 미리 로드하므로 여기서는 동기 로드 없이 Get만 사용한다.
	UPGAudioStatics::PlaySFX2D(this, CachedSettings.HitSound.Get());

	if (TSubclassOf<UCameraShakeBase> CameraShakeClass = CachedSettings.HitCameraShake.Get())
	{
		if (APlayerController* PlayerController = Cast<APlayerController>(RunnerCharacter.GetController()))
		{
			PlayerController->ClientStartCameraShake(CameraShakeClass);
		}
	}

	if (UPGCargoStackComponent* CargoStackComponent = RunnerCharacter.GetCargoStackComponent())
	{
		// 적재물도 러너와 같은 방향으로 밀어 '맞아서 흔들렸다'는 인과가 보이게 한다.
		CargoStackComponent->ApplyExternalLateralImpulse(DirectionSign * FMath::Max(0.0f, CachedSettings.HitCargoImpulseVelocity) * Strength);
	}
}

float APGMovingRunEventHazardActor::ResolvePushDirectionSign(const AActor& RunnerActor) const
{
	if (!FMath::IsNearlyZero(PushDirectionSign))
	{
		return PushDirectionSign;
	}

	const FVector ToRunner = RunnerActor.GetActorLocation() - GetActorLocation();
	const float LateralDot = FVector::DotProduct(ToRunner, RunnerActor.GetActorRightVector());
	if (FMath::Abs(LateralDot) <= KINDA_SMALL_NUMBER)
	{
		return FMath::RandBool() ? 1.0f : -1.0f;
	}

	// 중앙 구역은 러너가 위험 중심에서 벗어난 방향으로 한 번 더 밀어, 정면 충돌을 강한 회피 상황으로 바꿉니다.
	return FMath::Sign(LateralDot);
}
