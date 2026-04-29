// Copyright Epic Games, Inc. All Rights Reserved.

#include "Track/PGTrackSegment.h"

#include "Components/SceneComponent.h"
#include "Track/PGTrackEventMarkerComponent.h"

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
	ResetGeneratedEventMarkers();
}

void APGTrackSegment::DeactivateForPool()
{
	ResetGeneratedEventMarkers();
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);
	SetActorLocation(FVector(0.0f, 0.0f, -100000.0f), false, nullptr, ETeleportType::TeleportPhysics);
}

void APGTrackSegment::ResetGeneratedEventMarkers()
{
	TArray<UPGTrackEventMarkerComponent*> EventMarkers;
	GetComponents(EventMarkers);
	for (UPGTrackEventMarkerComponent* EventMarker : EventMarkers)
	{
		if (EventMarker)
		{
			EventMarker->ClearGeneratedEvent();
		}
	}
}
