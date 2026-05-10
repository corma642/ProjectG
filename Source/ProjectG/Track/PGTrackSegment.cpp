// Copyright Epic Games, Inc. All Rights Reserved.

#include "Track/PGTrackSegment.h"

#include "Components/SceneComponent.h"
#include "Track/PGTrackPoolableInterface.h"

APGTrackSegment::APGTrackSegment()
{
	PrimaryActorTick.bCanEverTick = false;

	SegmentRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SegmentRoot"));
	SetRootComponent(SegmentRoot);

	StartPoint = CreateDefaultSubobject<USceneComponent>(TEXT("StartPoint"));
	StartPoint->SetupAttachment(SegmentRoot);
	StartPoint->SetRelativeLocation(FVector::ZeroVector);

	EndPoint = CreateDefaultSubobject<USceneComponent>(TEXT("EndPoint"));
	EndPoint->SetupAttachment(SegmentRoot);
	EndPoint->SetRelativeLocation(FVector(SegmentLength, 0.0f, 0.0f));
}

void APGTrackSegment::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (EndPoint)
	{
		EndPoint->SetRelativeLocation(FVector(FMath::Max(100.0f, SegmentLength), 0.0f, 0.0f));
	}
}

FVector APGTrackSegment::GetSegmentStartWorldLocation() const
{
	return StartPoint ? StartPoint->GetComponentLocation() : GetActorLocation();
}

FVector APGTrackSegment::GetSegmentEndWorldLocation() const
{
	return EndPoint ? EndPoint->GetComponentLocation() : GetActorLocation() + GetActorForwardVector() * SegmentLength;
}

FVector APGTrackSegment::GetSegmentForwardVector() const
{
	const FVector StartLocation = GetSegmentStartWorldLocation();
	const FVector EndLocation = GetSegmentEndWorldLocation();
	const FVector SegmentDirection = EndLocation - StartLocation;
	return SegmentDirection.GetSafeNormal(UE_SMALL_NUMBER, GetActorForwardVector());
}

void APGTrackSegment::ActivateFromPool(const FTransform& SegmentTransform)
{
	SetActorTransform(SegmentTransform, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);

	TArray<AActor*> AttachedActors;
	GetAttachedActors(AttachedActors, true);
	for (AActor* AttachedActor : AttachedActors)
	{
		if (AttachedActor && AttachedActor->GetClass()->ImplementsInterface(UPGTrackPoolableInterface::StaticClass()))
		{
			IPGTrackPoolableInterface::Execute_OnTrackSegmentActivatedFromPool(AttachedActor);
		}
	}
}

void APGTrackSegment::DeactivateForPool()
{
	TArray<AActor*> AttachedActors;
	GetAttachedActors(AttachedActors, true);
	for (AActor* AttachedActor : AttachedActors)
	{
		if (AttachedActor && AttachedActor->GetClass()->ImplementsInterface(UPGTrackPoolableInterface::StaticClass()))
		{
			IPGTrackPoolableInterface::Execute_OnTrackSegmentDeactivatedForPool(AttachedActor);
		}
	}

	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);
	SetActorLocation(FVector(0.0f, 0.0f, -100000.0f), false, nullptr, ETeleportType::TeleportPhysics);
}
