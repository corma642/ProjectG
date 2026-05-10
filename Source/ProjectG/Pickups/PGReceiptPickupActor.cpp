// Copyright Epic Games, Inc. All Rights Reserved.

#include "Pickups/PGReceiptPickupActor.h"

#include "Character/PGRunnerCharacter.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameModes/PGGameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "RunProgress/PGRunProgressComponent.h"
#include "Sound/SoundBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGReceiptPickup, Log, All);

APGReceiptPickupActor::APGReceiptPickupActor()
{
	PrimaryActorTick.bCanEverTick = false;

	PickupRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PickupRoot"));
	SetRootComponent(PickupRoot);

	PickupTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("PickupTrigger"));
	PickupTrigger->SetupAttachment(PickupRoot);
	PickupTrigger->SetSphereRadius(50.0f);
	PickupTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupTrigger->SetCollisionObjectType(ECC_WorldDynamic);
	PickupTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	PickupTrigger->SetGenerateOverlapEvents(true);

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(PickupRoot);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetGenerateOverlapEvents(false);
}

void APGReceiptPickupActor::BeginPlay()
{
	Super::BeginPlay();

	if (PickupTrigger)
	{
		PickupTrigger->OnComponentBeginOverlap.AddDynamic(this, &APGReceiptPickupActor::HandlePickupOverlap);
	}

	SetPickupActive(!bCollected);
}

void APGReceiptPickupActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (PickupTrigger)
	{
		PickupTrigger->OnComponentBeginOverlap.RemoveDynamic(this, &APGReceiptPickupActor::HandlePickupOverlap);
	}

	Super::EndPlay(EndPlayReason);
}

void APGReceiptPickupActor::OnTrackSegmentActivatedFromPool_Implementation()
{
	bCollected = false;
	SetPickupActive(true);
}

void APGReceiptPickupActor::OnTrackSegmentDeactivatedForPool_Implementation()
{
	SetPickupActive(false);
}

void APGReceiptPickupActor::HandlePickupOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bCollected || !CanCollectFromActor(OtherActor))
	{
		return;
	}

	if (ReceiptAmount <= 0)
	{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
		UE_LOG(LogPGReceiptPickup, Warning, TEXT("ReceiptPickup '%s' has invalid ReceiptAmount=%d."), *GetNameSafe(this), ReceiptAmount);
#endif
		return;
	}

	APGRunnerCharacter* RunnerCharacter = Cast<APGRunnerCharacter>(OtherActor);
	UPGRunProgressComponent* RunProgressComponent = RunnerCharacter ? RunnerCharacter->GetRunProgressComponent() : nullptr;
	if (!RunProgressComponent)
	{
		return;
	}

	RunProgressComponent->AddReceipts(ReceiptAmount, TEXT("ReceiptPickup"));
	bCollected = true;
	SetPickupActive(false);
	PlayPickupFeedback();
}

void APGReceiptPickupActor::SetPickupActive(bool bActive)
{
	if (PickupTrigger)
	{
		PickupTrigger->SetCollisionEnabled(bActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		PickupTrigger->SetGenerateOverlapEvents(bActive);
	}

	if (VisualMesh)
	{
		VisualMesh->SetVisibility(bActive, true);
		VisualMesh->SetHiddenInGame(!bActive, true);
	}
}

void APGReceiptPickupActor::PlayPickupFeedback() const
{
	const FVector FeedbackLocation = GetActorLocation();
	const FRotator FeedbackRotation = GetActorRotation();

	if (PickupVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, PickupVFX, FeedbackLocation, FeedbackRotation);
	}

	if (PickupSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, PickupSound, FeedbackLocation);
	}
}

bool APGReceiptPickupActor::CanCollectFromActor(const AActor* OtherActor) const
{
	if (!OtherActor)
	{
		return false;
	}

	const APGRunnerCharacter* RunnerCharacter = Cast<APGRunnerCharacter>(OtherActor);
	if (!RunnerCharacter || !GetWorld())
	{
		return false;
	}

	const APGGameModeBase* GameMode = GetWorld()->GetAuthGameMode<APGGameModeBase>();
	return GameMode && GameMode->GetRunState() == EPGRunState::Running;
}
