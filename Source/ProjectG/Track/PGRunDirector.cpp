// Copyright Epic Games, Inc. All Rights Reserved.

#include "Track/PGRunDirector.h"

#include "Character/PGRunnerCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameModes/PGGameModeBase.h"
#include "Track/PGTrackSegment.h"
#include "Track/PGTrackSegmentSet.h"
#include "Utilities/PGWeightedRandom.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGRunDirector, Log, All);

APGRunDirector::APGRunDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
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
	CachedDifficultySettings = GetDefault<UPGGameSettings>()->Difficulty;

	InitializeRunnerReference();
	InitializeInitialSegment();
	ValidateTrackData();

	if (APGGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<APGGameModeBase>() : nullptr)
	{
		GameMode->OnRunCountdownStarted.AddDynamic(this, &APGRunDirector::HandleRunCountdownStarted);
		GameMode->OnRunStarted.AddDynamic(this, &APGRunDirector::HandleRunStarted);
		GameMode->OnRunFailed.AddDynamic(this, &APGRunDirector::HandleRunFailed);
		GameMode->OnRunContinueCountdownStarted.AddDynamic(this, &APGRunDirector::HandleRunCountdownStarted);
		GameMode->OnRunResumed.AddDynamic(this, &APGRunDirector::HandleRunResumed);

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
		GameMode->OnRunContinueCountdownStarted.RemoveDynamic(this, &APGRunDirector::HandleRunCountdownStarted);
		GameMode->OnRunResumed.RemoveDynamic(this, &APGRunDirector::HandleRunResumed);
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
	return CachedDifficultySettings.GetDifficultyAlpha(RunElapsedTime);
}

void APGRunDirector::HandleRunCountdownStarted(float Duration)
{
	bIsRunActive = true;
	bIsDifficultyActive = false;
	SetActorTickEnabled(true);
	SpawnSegmentsIfNeeded();
}

void APGRunDirector::HandleRunStarted()
{
	bIsRunActive = true;
	bIsDifficultyActive = true;
	RunElapsedTime = 0.0f;
	SetActorTickEnabled(true);
}

void APGRunDirector::HandleRunFailed(FName FailureReason)
{
	bIsRunActive = false;
	bIsDifficultyActive = false;
	SetActorTickEnabled(false);
}

void APGRunDirector::HandleRunResumed()
{
	// 이어하기는 난이도 시간을 이어 간다. 새 런처럼 0으로 되돌리지 않는다.
	bIsRunActive = true;
	bIsDifficultyActive = true;
	SetActorTickEnabled(true);
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
	// InitialSegment가 없으면 START는 맵 계약 검증에서 차단된다. 임의 TrackSegment fallback은 잘못 구성된
	// 신규 맵을 숨기므로 두지 않고, BeginPlay 초기화만 Actor 위치 기준으로 안전하게 끝낸다.
	if (InitialSegment)
	{
		RunOrigin = InitialSegment->GetSegmentStartWorldLocation();
		RunForward = InitialSegment->GetSegmentForwardVector().GetSafeNormal(UE_SMALL_NUMBER, GetActorForwardVector());

		if (bReplaceInitialSegment)
		{
			// 튜토리얼은 시작 세그먼트의 틈을 피해 정해진 길을 원점부터 깐다. 세그먼트는 숨기되 맵 검증용 참조는 남긴다.
			HideInitialSegment();
			return;
		}

		FPGActiveTrackSegment ActiveSegment;
		ActiveSegment.Segment = InitialSegment;
		ActiveSegment.EndProgress = GetProgressAtLocation(InitialSegment->GetSegmentEndWorldLocation());
		ActiveSegment.bPlacedInLevel = true;
		ActiveSegments.Add(ActiveSegment);
		return;
	}

	RunOrigin = GetActorLocation();
	RunForward = GetActorForwardVector().GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
}

void APGRunDirector::SetForcedSegmentSequence(const TArray<TSubclassOf<APGTrackSegment>>& Sequence, bool bInReplaceInitialSegment)
{
	ForcedSegmentSequence = Sequence;
	ForcedSegmentIndex = 0;
	bReplaceInitialSegment = bInReplaceInitialSegment;

	// BeginPlay가 이미 지났으면 활성 목록에 들어간 시작 세그먼트를 바로 빼고 숨긴다. 전이면 InitializeInitialSegment가 처리한다.
	if (bReplaceInitialSegment && HasActorBegunPlay() && InitialSegment)
	{
		ActiveSegments.RemoveAll([this](const FPGActiveTrackSegment& ActiveSegment)
		{
			return ActiveSegment.Segment == InitialSegment;
		});
		HideInitialSegment();
	}
}

void APGRunDirector::HideInitialSegment()
{
	if (InitialSegment)
	{
		// 풀 비활성화와 같은 처리라 안의 영수증 픽업도 함께 꺼진다.
		InitialSegment->DeactivateForPool();
	}
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

	int32 EntryIndex = INDEX_NONE;
	const FPGTrackSegmentEntry* SegmentEntry = nullptr;
	TSubclassOf<APGTrackSegment> SegmentClass = nullptr;
	if (ForcedSegmentSequence.IsValidIndex(ForcedSegmentIndex))
	{
		// 튜토리얼 구간은 가중치 선택 대신 정해진 순서를 쓴다. 쿨다운·연속 등장 통계에는 넣지 않는다.
		SegmentClass = ForcedSegmentSequence[ForcedSegmentIndex++];
	}
	else
	{
		EntryIndex = SelectSegmentEntryIndex();
		SegmentEntry = SegmentSet->SegmentEntries.IsValidIndex(EntryIndex) ? &SegmentSet->SegmentEntries[EntryIndex] : nullptr;
		SegmentClass = SegmentEntry ? SegmentEntry->SegmentClass : nullptr;
	}
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
	else
	{
		// fallback 세그먼트가 끼어들면 연속 등장이 끊긴 것이므로 카운터를 비운다.
		// 비우지 않으면 유효 후보가 하나뿐인 구간에서 그 후보가 계속 제외돼 fallback만 반복된다.
		LastSelectedEntryIndex = INDEX_NONE;
		ConsecutiveEntryCount = 0;
	}

	if (SegmentEntry)
	{
		// 연속 등장 제한과 쿨다운은 선택된 데이터 에셋 엔트리 기준으로만 기록한다.
		// fallback은 안전장치라 난이도 통계에 섞지 않는다.
		LastSpawnedSegmentByEntry.FindOrAdd(EntryIndex) = SegmentIndex;
		LastValidSegmentClass = SegmentClass.Get();
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
		APGTrackSegment* Segment = SegmentPool.Segments.Pop(EAllowShrinking::No).Get();
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
	SegmentPool.Segments.RemoveAllSwap([](const TWeakObjectPtr<APGTrackSegment>& Segment)
	{
		return !Segment.IsValid();
	}, EAllowShrinking::No);
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

	// 같은 resolver를 두 번 순회해 후보 배열을 만들지 않는다. 세그먼트 생성 Tick에서의 임시 할당을 피하기 위한 경로다.
	return PGWeightedRandom::SelectIndex(
		SegmentSet->SegmentEntries.Num(),
		[this, DifficultyAlpha](int32 EntryIndex)
	{
		const FPGTrackSegmentEntry& Entry = SegmentSet->SegmentEntries[EntryIndex];
		if (!Entry.SegmentClass || Entry.MinDifficultyAlpha > DifficultyAlpha)
		{
			return 0.0f;
		}

		const int32* LastSpawnedSegment = LastSpawnedSegmentByEntry.Find(EntryIndex);
		if (LastSpawnedSegment && GeneratedSegmentCount - *LastSpawnedSegment <= Entry.CooldownSegments)
		{
			return 0.0f;
		}

		if (EntryIndex == LastSelectedEntryIndex && ConsecutiveEntryCount >= FMath::Max(1, Entry.MaxConsecutiveCount))
		{
			return 0.0f;
		}

		return FMath::Max(0.0f, Entry.BaseWeight + Entry.DifficultyWeight * DifficultyAlpha);
	}, RandomStream);
}

TSubclassOf<APGTrackSegment> APGRunDirector::ResolveFallbackSegmentClass() const
{
	if (SegmentSet && SegmentSet->FallbackSegmentClass)
	{
		// 가장 우선하는 fallback은 에디터에서 명시한 안정 세그먼트다.
		return SegmentSet->FallbackSegmentClass;
	}

	if (UClass* LastValidClass = LastValidSegmentClass.Get())
	{
		// 최근에 실제로 생성에 성공한 클래스는 모바일 패키징에서도 로드/스폰 가능성이 확인된 후보입니다.
		return LastValidClass;
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
