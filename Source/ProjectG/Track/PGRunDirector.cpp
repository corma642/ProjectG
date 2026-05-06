// Copyright Epic Games, Inc. All Rights Reserved.

#include "Track/PGRunDirector.h"

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

	if (GetWorld())
	{
		for (TActorIterator<APGRunDirector> DirectorIt(GetWorld()); DirectorIt; ++DirectorIt)
		{
			if (*DirectorIt != this)
			{
				UE_LOG(LogPGRunDirector, Warning, TEXT("Multiple RunDirector actors exist in this world. Keep only one APGRunDirector to avoid duplicate track spawning. Current='%s', Other='%s'."), *GetNameSafe(this), *GetNameSafe(*DirectorIt));
				break;
			}
		}
	}

	const int32 Seed = bUseRandomSeedEachRun ? FMath::Rand() : RandomSeed;
	RandomStream.Initialize(Seed);

	InitializeRunnerReference();
	InitializeInitialSegment();
	ValidateTrackData();

	if (APGGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<APGGameModeBase>() : nullptr)
	{
		GameMode->OnRunCountdownStarted.AddDynamic(this, &APGRunDirector::HandleRunCountdownStarted);
		GameMode->OnRunStarted.AddDynamic(this, &APGRunDirector::HandleRunStarted);
		GameMode->OnRunFailed.AddDynamic(this, &APGRunDirector::HandleRunFailed);

		if (GameMode->GetRunState() == EPGRunState::Countdown)
		{
			HandleRunCountdownStarted(0.0f);
		}
		else if (GameMode->GetRunState() == EPGRunState::Running)
		{
			HandleRunStarted();
		}
	}
}

void APGRunDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APGGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<APGGameModeBase>() : nullptr)
	{
		GameMode->OnRunCountdownStarted.RemoveDynamic(this, &APGRunDirector::HandleRunCountdownStarted);
		GameMode->OnRunStarted.RemoveDynamic(this, &APGRunDirector::HandleRunStarted);
		GameMode->OnRunFailed.RemoveDynamic(this, &APGRunDirector::HandleRunFailed);
	}

	Super::EndPlay(EndPlayReason);
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

	if (bIsDifficultyActive)
	{
		RunElapsedTime += DeltaSeconds;
	}
	RemovePassedSegments();
	SpawnSegmentsIfNeeded();
	DrawRunDirectorDebug();
}

float APGRunDirector::GetDifficultyAlpha() const
{
	return DifficultyRampDuration > KINDA_SMALL_NUMBER ? FMath::Clamp(RunElapsedTime / DifficultyRampDuration, 0.0f, 1.0f) : 1.0f;
}

void APGRunDirector::HandleRunCountdownStarted(float Duration)
{
	bIsRunActive = true;
	bIsDifficultyActive = false;
	SpawnSegmentsIfNeeded();
}

void APGRunDirector::HandleRunStarted()
{
	bIsRunActive = true;
	bIsDifficultyActive = true;
	RunElapsedTime = 0.0f;
}

void APGRunDirector::HandleRunFailed(FName FailureReason)
{
	bIsRunActive = false;
	bIsDifficultyActive = false;
}

void APGRunDirector::InitializeRunnerReference()
{
	RunnerCharacter = Cast<APGRunnerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
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
