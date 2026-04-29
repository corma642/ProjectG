// Copyright Epic Games, Inc. All Rights Reserved.

#include "Track/PGRunDirector.h"

#include "Cargo/PGCargoStackComponent.h"
#include "Character/PGRunnerCharacter.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameModes/PGGameModeBase.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGRunDirector, Log, All);

APGRunDirector::APGRunDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
}

void APGRunDirector::BeginPlay()
{
	Super::BeginPlay();

	const int32 Seed = bUseRandomSeedEachRun ? FMath::Rand() : RandomSeed;
	RandomStream.Initialize(Seed);

	InitializeRunnerReference();
	InitializeInitialSegment();
	ValidateTrackData();

	if (APGGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<APGGameModeBase>() : nullptr)
	{
		GameMode->OnRunFailed.AddDynamic(this, &APGRunDirector::HandleRunFailed);
	}

	SpawnSegmentsIfNeeded();
}

void APGRunDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bIsRunActive)
	{
		return;
	}

	if (!RunnerCharacter)
	{
		InitializeRunnerReference();
		if (!RunnerCharacter)
		{
			return;
		}
	}

	RunElapsedTime += DeltaSeconds;
	RemovePassedSegments();
	SpawnSegmentsIfNeeded();
	DrawRunDirectorDebug();
}

float APGRunDirector::GetDifficultyAlpha() const
{
	return DifficultyRampDuration > KINDA_SMALL_NUMBER ? FMath::Clamp(RunElapsedTime / DifficultyRampDuration, 0.0f, 1.0f) : 1.0f;
}

void APGRunDirector::HandleRunFailed(FName FailureReason)
{
	bIsRunActive = false;
}

void APGRunDirector::InitializeRunnerReference()
{
	RunnerCharacter = Cast<APGRunnerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	CargoStackComponent = RunnerCharacter ? RunnerCharacter->FindComponentByClass<UPGCargoStackComponent>() : nullptr;
}

void APGRunDirector::InitializeInitialSegment()
{
	if (!InitialSegment)
	{
		for (TActorIterator<APGTrackSegment> SegmentIt(GetWorld()); SegmentIt; ++SegmentIt)
		{
			InitialSegment = *SegmentIt;
			break;
		}
	}

	if (InitialSegment)
	{
		RunOrigin = InitialSegment->GetSegmentStartWorldLocation();
		RunForward = InitialSegment->GetSegmentForwardVector().GetSafeNormal(UE_SMALL_NUMBER, GetActorForwardVector());

		FPGActiveTrackSegment ActiveSegment;
		ActiveSegment.Segment = InitialSegment;
		ActiveSegment.EntryIndex = INDEX_NONE;
		ActiveSegment.StartProgress = GetProgressAtLocation(InitialSegment->GetSegmentStartWorldLocation());
		ActiveSegment.EndProgress = GetProgressAtLocation(InitialSegment->GetSegmentEndWorldLocation());
		ActiveSegment.bPlacedInLevel = true;
		ActiveSegments.Add(ActiveSegment);
		return;
	}

	RunOrigin = GetActorLocation();
	RunForward = GetActorForwardVector().GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
}

void APGRunDirector::ValidateTrackData() const
{
	if (!SegmentSet)
	{
		UE_LOG(LogPGRunDirector, Warning, TEXT("RunDirector '%s' has no SegmentSet. Infinite track generation will wait until a SegmentSet is assigned."), *GetNameSafe(this));
		return;
	}

	if (SegmentSet->SegmentEntries.IsEmpty())
	{
		UE_LOG(LogPGRunDirector, Warning, TEXT("SegmentSet '%s' has no SegmentEntries."), *GetNameSafe(SegmentSet));
		return;
	}

	bool bHasPositiveWeight = false;
	bool bHasFallbackCandidate = SegmentSet->FallbackSegmentClass != nullptr;

	for (int32 EntryIndex = 0; EntryIndex < SegmentSet->SegmentEntries.Num(); ++EntryIndex)
	{
		const FPGTrackSegmentEntry& Entry = SegmentSet->SegmentEntries[EntryIndex];
		if (!Entry.SegmentClass)
		{
			UE_LOG(LogPGRunDirector, Warning, TEXT("SegmentSet '%s' entry %d has no SegmentClass."), *GetNameSafe(SegmentSet), EntryIndex);
			continue;
		}

		const APGTrackSegment* SegmentDefault = Entry.SegmentClass->GetDefaultObject<APGTrackSegment>();
		if (!SegmentDefault || SegmentDefault->GetSegmentLength() <= KINDA_SMALL_NUMBER)
		{
			UE_LOG(LogPGRunDirector, Warning, TEXT("SegmentSet '%s' entry %d has invalid segment length."), *GetNameSafe(SegmentSet), EntryIndex);
		}

		if (Entry.BaseWeight > 0.0f || Entry.DifficultyWeight > 0.0f)
		{
			bHasPositiveWeight = true;
		}

		if (Entry.bCanBeFallback)
		{
			bHasFallbackCandidate = true;
		}

		if (Entry.bAllowGeneratedEvents && SegmentDefault && SegmentDefault->AllowsGeneratedEvents())
		{
			TArray<UPGTrackEventMarkerComponent*> EventMarkers;
			SegmentDefault->GetComponents(EventMarkers);
			if (EventMarkers.IsEmpty())
			{
				UE_LOG(LogPGRunDirector, Warning, TEXT("SegmentSet '%s' entry %d allows generated events but its segment class has no PGTrackEventMarkerComponent."), *GetNameSafe(SegmentSet), EntryIndex);
			}
		}
	}

	if (!bHasPositiveWeight)
	{
		UE_LOG(LogPGRunDirector, Warning, TEXT("SegmentSet '%s' has no entry with positive BaseWeight or DifficultyWeight."), *GetNameSafe(SegmentSet));
	}

	if (!bHasFallbackCandidate)
	{
		UE_LOG(LogPGRunDirector, Warning, TEXT("SegmentSet '%s' has no explicit fallback class and no fallback-capable entry."), *GetNameSafe(SegmentSet));
	}
}

void APGRunDirector::SpawnSegmentsIfNeeded()
{
	const float PlayerProgress = GetPlayerProgress();
	int32 GuardCount = 0;

	while (GuardCount < 16)
	{
		const bool bNeedsMoreCount = ActiveSegments.Num() < TargetActiveSegmentCount;
		const float FrontProgress = ActiveSegments.IsEmpty() ? PlayerProgress : ActiveSegments.Last().EndProgress;
		const bool bNeedsMoreDistance = FrontProgress - PlayerProgress < SpawnAheadDistance;

		if (!bNeedsMoreCount && !bNeedsMoreDistance)
		{
			break;
		}

		if (!SpawnNextSegment())
		{
			break;
		}

		++GuardCount;
	}
}

bool APGRunDirector::SpawnNextSegment()
{
	if (!SegmentSet || SegmentSet->SegmentEntries.IsEmpty() || !GetWorld())
	{
		return false;
	}

	const int32 EntryIndex = SelectSegmentEntryIndex();
	const FPGTrackSegmentEntry* SegmentEntry = SegmentSet->SegmentEntries.IsValidIndex(EntryIndex) ? &SegmentSet->SegmentEntries[EntryIndex] : nullptr;
	TSubclassOf<APGTrackSegment> SegmentClass = SegmentEntry ? SegmentEntry->SegmentClass : nullptr;
	if (!SegmentClass)
	{
		SegmentClass = ResolveFallbackSegmentClass();
		SegmentEntry = nullptr;
	}

	if (!SegmentClass)
	{
		UE_LOG(LogPGRunDirector, Warning, TEXT("RunDirector '%s' could not resolve a valid track segment class."), *GetNameSafe(this));
		return false;
	}

	const FVector SpawnLocation = ActiveSegments.IsEmpty() || !ActiveSegments.Last().Segment ? RunOrigin : ActiveSegments.Last().Segment->GetSegmentEndWorldLocation();
	const FRotator SpawnRotation = RunForward.Rotation();
	const FTransform SpawnTransform(SpawnRotation, SpawnLocation, FVector::OneVector);

	APGTrackSegment* SpawnedSegment = AcquireSegment(SegmentClass, SpawnTransform);
	if (!SpawnedSegment)
	{
		return false;
	}

	const int32 SegmentIndex = GeneratedSegmentCount;

	FPGActiveTrackSegment ActiveSegment;
	ActiveSegment.Segment = SpawnedSegment;
	ActiveSegment.EntryIndex = SegmentEntry ? EntryIndex : INDEX_NONE;
	ActiveSegment.StartProgress = GetProgressAtLocation(SpawnedSegment->GetSegmentStartWorldLocation());
	ActiveSegment.EndProgress = GetProgressAtLocation(SpawnedSegment->GetSegmentEndWorldLocation());
	ActiveSegment.bPlacedInLevel = false;
	ActiveSegments.Add(ActiveSegment);

	if (SegmentEntry && EntryIndex == LastSelectedEntryIndex)
	{
		++ConsecutiveEntryCount;
	}
	else if (SegmentEntry)
	{
		LastSelectedEntryIndex = EntryIndex;
		ConsecutiveEntryCount = 1;
	}

	if (SegmentEntry)
	{
		LastSpawnedSegmentByEntry.FindOrAdd(EntryIndex) = SegmentIndex;
		LastValidSegmentClass = SegmentClass;
		ConfigureGeneratedEvents(SpawnedSegment, *SegmentEntry, SegmentIndex);
	}
	else
	{
		SpawnedSegment->ResetGeneratedEventMarkers();
	}

	++GeneratedSegmentCount;
	return true;
}

void APGRunDirector::RemovePassedSegments()
{
	const float PlayerProgress = GetPlayerProgress();

	for (int32 SegmentIndex = ActiveSegments.Num() - 1; SegmentIndex >= 0; --SegmentIndex)
	{
		const FPGActiveTrackSegment& ActiveSegment = ActiveSegments[SegmentIndex];
		if (PlayerProgress - ActiveSegment.EndProgress <= DespawnBehindDistance)
		{
			continue;
		}

		if (APGTrackSegment* Segment = ActiveSegment.Segment)
		{
			if (ActiveSegment.bPlacedInLevel)
			{
				if (bDestroyPassedPlacedSegments)
				{
					Segment->Destroy();
				}
			}
			else
			{
				ReturnSegmentToPool(Segment);
			}
		}

		ActiveSegments.RemoveAt(SegmentIndex);
	}
}

APGTrackSegment* APGRunDirector::AcquireSegment(TSubclassOf<APGTrackSegment> SegmentClass, const FTransform& SpawnTransform)
{
	if (!SegmentClass || !GetWorld())
	{
		return nullptr;
	}

	FPGPooledTrackSegments& SegmentPool = SegmentPoolByClass.FindOrAdd(SegmentClass);
	while (!SegmentPool.Segments.IsEmpty())
	{
		APGTrackSegment* Segment = SegmentPool.Segments.Pop(EAllowShrinking::No);
		if (Segment && !Segment->IsActorBeingDestroyed())
		{
			Segment->ActivateFromPool(SpawnTransform);
			return Segment;
		}
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	APGTrackSegment* SpawnedSegment = GetWorld()->SpawnActor<APGTrackSegment>(SegmentClass, SpawnTransform, SpawnParameters);
	if (SpawnedSegment)
	{
		SpawnedSegment->ActivateFromPool(SpawnTransform);
	}
	return SpawnedSegment;
}

void APGRunDirector::ReturnSegmentToPool(APGTrackSegment* Segment)
{
	if (!Segment)
	{
		return;
	}

	TSubclassOf<APGTrackSegment> SegmentClass = Segment->GetClass();
	FPGPooledTrackSegments& SegmentPool = SegmentPoolByClass.FindOrAdd(SegmentClass);
	if (SegmentPool.Segments.Num() >= MaxPooledSegmentsPerClass)
	{
		Segment->Destroy();
		return;
	}

	Segment->DeactivateForPool();
	SegmentPool.Segments.Add(Segment);
}

void APGRunDirector::ConfigureGeneratedEvents(APGTrackSegment* Segment, const FPGTrackSegmentEntry& SegmentEntry, int32 SegmentIndex)
{
	if (!Segment)
	{
		return;
	}

	TArray<UPGTrackEventMarkerComponent*> EventMarkers;
	Segment->GetComponents(EventMarkers);

	for (UPGTrackEventMarkerComponent* EventMarker : EventMarkers)
	{
		if (EventMarker)
		{
			EventMarker->ClearGeneratedEvent();
		}
	}

	if (!Segment->AllowsGeneratedEvents() || !SegmentEntry.bAllowGeneratedEvents || EventMarkers.IsEmpty())
	{
		return;
	}

	const float DifficultyAlpha = GetDifficultyAlpha();
	const float SegmentProgress = GetProgressAtLocation(Segment->GetSegmentStartWorldLocation());
	if (!CanAssignGeneratedEvent(SegmentIndex, SegmentProgress))
	{
		return;
	}

	const float EventChance = FMath::Lerp(BaseEventChance, MaxEventChance, DifficultyAlpha);
	if (RandomStream.FRand() > EventChance)
	{
		return;
	}

	TArray<UPGTrackEventMarkerComponent*> AvailableMarkers;
	for (UPGTrackEventMarkerComponent* EventMarker : EventMarkers)
	{
		if (EventMarker && EventMarker->CanReceiveGeneratedEvent())
		{
			AvailableMarkers.Add(EventMarker);
		}
	}

	if (AvailableMarkers.IsEmpty())
	{
		return;
	}

	const EPGTrackEventType SelectedEventType = SelectEventType(DifficultyAlpha);
	if (SelectedEventType == EPGTrackEventType::None)
	{
		return;
	}

	const int32 MarkerIndex = RandomStream.RandRange(0, AvailableMarkers.Num() - 1);
	AvailableMarkers[MarkerIndex]->ConfigureGeneratedEvent(SelectedEventType, DifficultyAlpha, RandomStream.RandRange(1, MAX_int32));
	LastEventSegmentIndex = SegmentIndex;
	LastEventTime = RunElapsedTime;
	LastEventProgress = SegmentProgress;
}

int32 APGRunDirector::SelectSegmentEntryIndex()
{
	const float DifficultyAlpha = GetDifficultyAlpha();
	float TotalWeight = 0.0f;

	for (int32 EntryIndex = 0; EntryIndex < SegmentSet->SegmentEntries.Num(); ++EntryIndex)
	{
		const FPGTrackSegmentEntry& Entry = SegmentSet->SegmentEntries[EntryIndex];
		if (!Entry.SegmentClass || Entry.MinDifficultyAlpha > DifficultyAlpha)
		{
			continue;
		}

		const int32* LastSpawnedSegment = LastSpawnedSegmentByEntry.Find(EntryIndex);
		if (LastSpawnedSegment && GeneratedSegmentCount - *LastSpawnedSegment <= Entry.CooldownSegments)
		{
			continue;
		}

		if (EntryIndex == LastSelectedEntryIndex && ConsecutiveEntryCount >= FMath::Max(1, Entry.MaxConsecutiveCount))
		{
			continue;
		}

		const float Weight = FMath::Max(0.0f, Entry.BaseWeight + Entry.DifficultyWeight * DifficultyAlpha);
		if (Weight <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		TotalWeight += Weight;
	}

	if (TotalWeight <= KINDA_SMALL_NUMBER)
	{
		return INDEX_NONE;
	}

	const float Roll = RandomStream.FRandRange(0.0f, TotalWeight);
	float AccumulatedWeight = 0.0f;

	for (int32 EntryIndex = 0; EntryIndex < SegmentSet->SegmentEntries.Num(); ++EntryIndex)
	{
		const FPGTrackSegmentEntry& Entry = SegmentSet->SegmentEntries[EntryIndex];
		if (!Entry.SegmentClass || Entry.MinDifficultyAlpha > DifficultyAlpha)
		{
			continue;
		}

		const int32* LastSpawnedSegment = LastSpawnedSegmentByEntry.Find(EntryIndex);
		if (LastSpawnedSegment && GeneratedSegmentCount - *LastSpawnedSegment <= Entry.CooldownSegments)
		{
			continue;
		}

		if (EntryIndex == LastSelectedEntryIndex && ConsecutiveEntryCount >= FMath::Max(1, Entry.MaxConsecutiveCount))
		{
			continue;
		}

		const float Weight = FMath::Max(0.0f, Entry.BaseWeight + Entry.DifficultyWeight * DifficultyAlpha);
		if (Weight <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		AccumulatedWeight += Weight;
		if (Roll <= AccumulatedWeight)
		{
			return EntryIndex;
		}
	}

	return INDEX_NONE;
}

TSubclassOf<APGTrackSegment> APGRunDirector::ResolveFallbackSegmentClass() const
{
	if (SegmentSet && SegmentSet->FallbackSegmentClass)
	{
		return SegmentSet->FallbackSegmentClass;
	}

	if (LastValidSegmentClass)
	{
		return LastValidSegmentClass;
	}

	if (!SegmentSet)
	{
		return nullptr;
	}

	for (const FPGTrackSegmentEntry& Entry : SegmentSet->SegmentEntries)
	{
		if (Entry.SegmentClass && Entry.bCanBeFallback)
		{
			return Entry.SegmentClass;
		}
	}

	for (const FPGTrackSegmentEntry& Entry : SegmentSet->SegmentEntries)
	{
		if (Entry.SegmentClass)
		{
			return Entry.SegmentClass;
		}
	}

	return nullptr;
}

bool APGRunDirector::CanAssignGeneratedEvent(int32 SegmentIndex, float SegmentProgress) const
{
	if (SegmentIndex < SafeInitialGeneratedSegments)
	{
		return false;
	}

	if (RunElapsedTime < SafeInitialRunTime)
	{
		return false;
	}

	if (SegmentProgress < SafeInitialDistance)
	{
		return false;
	}

	if (LastEventSegmentIndex != INDEX_NONE && SegmentIndex - LastEventSegmentIndex <= MinSegmentsBetweenEvents)
	{
		return false;
	}

	if (RunElapsedTime - LastEventTime < MinSecondsBetweenEvents)
	{
		return false;
	}

	if (SegmentProgress - LastEventProgress < MinDistanceBetweenEvents)
	{
		return false;
	}

	return true;
}

EPGTrackEventType APGRunDirector::SelectEventType(float DifficultyAlpha) const
{
	struct FWeightedEventType
	{
		EPGTrackEventType EventType = EPGTrackEventType::None;
		float Weight = 0.0f;
	};

	TArray<FWeightedEventType> Candidates;

	if (DifficultyAlpha >= WindMinDifficulty)
	{
		Candidates.Add({ EPGTrackEventType::Wind, 1.0f + DifficultyAlpha });
	}

	if (DifficultyAlpha >= IceMinDifficulty)
	{
		Candidates.Add({ EPGTrackEventType::Ice, 0.75f + DifficultyAlpha });
	}

	if (DifficultyAlpha >= CargoDropMinDifficulty)
	{
		if (IsCargoDropAvailable())
		{
			Candidates.Add({ EPGTrackEventType::CargoDrop, 1.25f });
		}
	}

	if (Candidates.IsEmpty())
	{
		return EPGTrackEventType::None;
	}

	float TotalWeight = 0.0f;
	for (const FWeightedEventType& Candidate : Candidates)
	{
		TotalWeight += Candidate.Weight;
	}

	const float Roll = RandomStream.FRandRange(0.0f, TotalWeight);
	float AccumulatedWeight = 0.0f;
	for (const FWeightedEventType& Candidate : Candidates)
	{
		AccumulatedWeight += Candidate.Weight;
		if (Roll <= AccumulatedWeight)
		{
			return Candidate.EventType;
		}
	}

	return Candidates.Last().EventType;
}

bool APGRunDirector::IsCargoDropAvailable() const
{
	if (!CargoStackComponent)
	{
		return false;
	}

	return CargoStackComponent->GetReservedCargoCount() < CargoStackComponent->GetMaxCargoCount();
}

float APGRunDirector::GetPlayerProgress() const
{
	return RunnerCharacter ? GetProgressAtLocation(RunnerCharacter->GetActorLocation()) : 0.0f;
}

float APGRunDirector::GetProgressAtLocation(const FVector& WorldLocation) const
{
	return FVector::DotProduct(WorldLocation - RunOrigin, RunForward);
}

void APGRunDirector::DrawRunDirectorDebug() const
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	if (!bDrawRunDirectorDebug || !GetWorld())
	{
		return;
	}

	const FVector DebugLocation = RunnerCharacter ? RunnerCharacter->GetActorLocation() + FVector::UpVector * 260.0f : GetActorLocation();
	const FString DebugText = FString::Printf(TEXT("RunDirector Segments:%d Difficulty:%.2f"), ActiveSegments.Num(), GetDifficultyAlpha());
	DrawDebugString(GetWorld(), DebugLocation, DebugText, nullptr, FColor::Green, 0.0f, true);

	for (const FPGActiveTrackSegment& ActiveSegment : ActiveSegments)
	{
		if (const APGTrackSegment* Segment = ActiveSegment.Segment)
		{
			DrawDebugLine(GetWorld(), Segment->GetSegmentStartWorldLocation(), Segment->GetSegmentEndWorldLocation(), FColor::Green, false, 0.0f, 0, 3.0f);
		}
	}
#endif
}
