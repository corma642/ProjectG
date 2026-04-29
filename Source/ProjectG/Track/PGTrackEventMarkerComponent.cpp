// Copyright Epic Games, Inc. All Rights Reserved.

#include "Track/PGTrackEventMarkerComponent.h"

#include "Environment/PGRunnerEnvironmentComponent.h"
#include "GameFramework/Pawn.h"

UPGTrackEventMarkerComponent::UPGTrackEventMarkerComponent()
{
	InitBoxExtent(FVector(120.0f, 220.0f, 160.0f));
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionObjectType(ECC_WorldDynamic);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	SetGenerateOverlapEvents(true);
	bHiddenInGame = true;
}

void UPGTrackEventMarkerComponent::BeginPlay()
{
	Super::BeginPlay();

	OnComponentBeginOverlap.AddDynamic(this, &UPGTrackEventMarkerComponent::HandleMarkerBeginOverlap);
}

void UPGTrackEventMarkerComponent::ConfigureGeneratedEvent(EPGTrackEventType InEventType, float DifficultyAlpha, int32 EventSeed)
{
	GeneratedEventType = bAllowGeneratedEvents ? InEventType : EPGTrackEventType::None;
	GeneratedEventState = GeneratedEventType == EPGTrackEventType::None ? EPGTrackEventState::Inactive : EPGTrackEventState::Preview;
	bHasTriggered = false;

	FRandomStream EventRandom(EventSeed);
	GeneratedWindDirection = EventRandom.RandRange(0, 1) == 0 ? -1.0f : 1.0f;

	const float ClampedDifficulty = FMath::Clamp(DifficultyAlpha, 0.0f, 1.0f);
	GeneratedWindStrength = WindStrength * FMath::Lerp(0.75f, 1.25f, ClampedDifficulty);
}

void UPGTrackEventMarkerComponent::ClearGeneratedEvent()
{
	GeneratedEventType = EPGTrackEventType::None;
	GeneratedEventState = EPGTrackEventState::Inactive;
	bHasTriggered = false;
	GeneratedWindDirection = 1.0f;
	GeneratedWindStrength = WindStrength;
}

void UPGTrackEventMarkerComponent::ArmGeneratedEvent()
{
	if (GeneratedEventType != EPGTrackEventType::None && GeneratedEventState == EPGTrackEventState::Preview)
	{
		GeneratedEventState = EPGTrackEventState::Armed;
	}
}

void UPGTrackEventMarkerComponent::HandleMarkerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bHasTriggered || GeneratedEventType == EPGTrackEventType::None || !OtherActor)
	{
		return;
	}

	if (!OtherActor->IsA<APawn>())
	{
		return;
	}

	bHasTriggered = true;
	GeneratedEventState = EPGTrackEventState::Triggered;
	ApplyGeneratedEvent(OtherActor);
}

void UPGTrackEventMarkerComponent::ApplyGeneratedEvent(AActor* RunnerActor)
{
	UPGRunnerEnvironmentComponent* EnvironmentComponent = RunnerActor ? RunnerActor->FindComponentByClass<UPGRunnerEnvironmentComponent>() : nullptr;
	if (!EnvironmentComponent)
	{
		return;
	}

	switch (GeneratedEventType)
	{
	case EPGTrackEventType::Wind:
		EnvironmentComponent->ApplyWind(GeneratedWindDirection, GeneratedWindStrength, WindDuration);
		break;
	case EPGTrackEventType::Ice:
		EnvironmentComponent->ApplyIce(IceDuration, IceInputLagScale);
		break;
	case EPGTrackEventType::CargoDrop:
		EnvironmentComponent->RequestCargoDrop();
		break;
	case EPGTrackEventType::None:
	default:
		break;
	}
}
