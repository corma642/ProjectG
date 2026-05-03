// Copyright Epic Games, Inc. All Rights Reserved.

#include "RunProgress/PGRunProgressComponent.h"

UPGRunProgressComponent::UPGRunProgressComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPGRunProgressComponent::InitializeRunProgress(const FPGRunProgressSettings& InSettings, const FVector& InStartLocation, const FVector& InForwardVector, float InStartTimeSeconds)
{
	CachedSettings = InSettings;
	StartLocation = InStartLocation;
	StartForwardVector = InForwardVector.GetSafeNormal();
	if (StartForwardVector.IsNearlyZero())
	{
		StartForwardVector = FVector::ForwardVector;
	}

	StartTimeSeconds = InStartTimeSeconds;
	RawDistanceMeters = 0.0f;
	DisplayedDistanceMeters = 0.0f;
	Score = 0;
	bIsProgressActive = true;

	OnDisplayedDistanceChanged.Broadcast(DisplayedDistanceMeters);
}

void UPGRunProgressComponent::UpdateRunProgress(const FVector& CurrentLocation)
{
	if (!bIsProgressActive)
	{
		return;
	}

	RawDistanceMeters = CalculateRawDistanceMeters(CurrentLocation);
	const float NewDisplayedDistanceMeters = QuantizeDistanceMeters(RawDistanceMeters);
	if (!FMath::IsNearlyEqual(NewDisplayedDistanceMeters, DisplayedDistanceMeters))
	{
		DisplayedDistanceMeters = NewDisplayedDistanceMeters;
		RecalculateScore();
		OnDisplayedDistanceChanged.Broadcast(DisplayedDistanceMeters);
	}
}

void UPGRunProgressComponent::StopRunProgress(const FVector& CurrentLocation)
{
	if (bIsProgressActive)
	{
		UpdateRunProgress(CurrentLocation);
		bIsProgressActive = false;
	}
}

float UPGRunProgressComponent::GetElapsedTimeSeconds() const
{
	const UWorld* World = GetWorld();
	return World ? FMath::Max(0.0f, World->GetTimeSeconds() - StartTimeSeconds) : 0.0f;
}

float UPGRunProgressComponent::CalculateRawDistanceMeters(const FVector& CurrentLocation) const
{
	const FVector RunDelta = CurrentLocation - StartLocation;
	const float DistanceCm = FVector::DotProduct(RunDelta, StartForwardVector);
	return FMath::Max(0.0f, DistanceCm * 0.01f);
}

float UPGRunProgressComponent::QuantizeDistanceMeters(float InRawDistanceMeters) const
{
	const float StepMeters = FMath::Max(1.0f, CachedSettings.DistanceUpdateStepMeters);
	return FMath::FloorToFloat(InRawDistanceMeters / StepMeters) * StepMeters;
}

void UPGRunProgressComponent::RecalculateScore()
{
	const float ScoreValue = DisplayedDistanceMeters * static_cast<float>(FMath::Max(0, CachedSettings.ScorePerMeter)) * FMath::Max(0.0f, CachedSettings.ScoreMultiplier);
	Score = FMath::RoundToInt(ScoreValue);
}
