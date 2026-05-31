// Copyright Epic Games, Inc. All Rights Reserved.

#include "Track/PGRunDirector.h"

#include "Character/PGRunnerCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameModes/PGGameModeBase.h"

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
				// Director가 둘 이상이면 각자 세그먼트를 생성해 트랙이 겹친다.
				// 자동 복구하지 않고 경고만 남겨 레벨 배치 문제를 바로 드러낸다.
				UE_LOG(LogPGRunDirector, Warning, TEXT("Multiple RunDirector actors exist in this world. Keep only one APGRunDirector to avoid duplicate track spawning. Current='%s', Other='%s'."), *GetNameSafe(this), *GetNameSafe(*DirectorIt));
				SetActorTickEnabled(false);
				return;
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
		// 카운트다운 동안에는 트랙을 미리 깔 수 있지만 난이도 시간은 Running부터 증가시킨다.
		RunElapsedTime += DeltaSeconds;
	}
	RemovePassedSegments();
	SpawnSegmentsIfNeeded();
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
	if (APGGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<APGGameModeBase>() : nullptr)
	{
		RunnerCharacter = GameMode->GetPlayableRunnerCharacter();
	}
}

void APGRunDirector::InitializeInitialSegment()
{
	if (!InitialSegment)
	{
		// START는 맵 계약 검증에서 차단되지만 BeginPlay 초기화 자체는 안전하게 끝냅니다.
		// 임의 TrackSegment fallback은 잘못 구성된 신규 맵을 숨기므로 사용하지 않습니다.
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

		// 잘못된 세그먼트 길이/설정으로 무한 루프가 생기는 것을 막는다.
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
		// 가중치 필터가 모두 탈락해도 런이 끊기는 것은 더 치명적이다.
		// 명시 fallback 또는 최근 정상 세그먼트로 최소한의 진행로를 이어 붙인다.
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
		// 연속 등장 제한과 쿨다운은 선택된 데이터 에셋 엔트리 기준으로만 기록한다.
		// fallback은 안전장치라 난이도 통계에 섞지 않는다.
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
				// 런 중 생성된 세그먼트는 Destroy 대신 풀로 돌려 다시 활성화한다.
				// 세그먼트 안에 배치된 영수증 픽업도 Activate/Deactivate 알림으로 재사용된다.
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
			// 풀에서 꺼낸 세그먼트는 새 Transform을 적용하고, 내부 풀링 액터들이 리셋될 수 있게 알림을 보낸다.
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
		// 풀 상한을 넘기면 모바일 메모리 증가를 막기 위해 실제 Destroy한다.
		Segment->Destroy();
		return;
	}

	// 비활성화는 세그먼트와 하위 수집 액터를 숨기고 충돌을 끄는 재사용 준비 단계다.
	Segment->DeactivateForPool();
	SegmentPool.Segments.Add(Segment);
}

int32 APGRunDirector::SelectSegmentEntryIndex()
{
	const float DifficultyAlpha = GetDifficultyAlpha();
	float TotalWeight = 0.0f;

	// 첫 패스는 현재 난이도, 쿨다운, 연속 제한을 통과한 후보의 총 가중치를 계산한다.
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

	// 두 번째 패스에서 같은 필터를 다시 적용해 누적 가중치에 도달한 엔트리를 선택한다.
	// 후보 배열을 따로 만들지 않아 Tick 중 임시 할당을 피한다.
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
		// 가장 우선하는 fallback은 에디터에서 명시한 안정 세그먼트다.
		return SegmentSet->FallbackSegmentClass;
	}

	if (LastValidSegmentClass)
	{
		// 최근에 실제로 생성에 성공한 클래스는 모바일 패키징에서도 로드/스폰 가능성이 확인된 후보다.
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
