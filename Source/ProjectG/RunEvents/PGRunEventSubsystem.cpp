// Copyright Epic Games, Inc. All Rights Reserved.

#include "RunEvents/PGRunEventSubsystem.h"

#include "Character/PGRunnerCharacter.h"
#include "Environment/PGRunnerEnvironmentComponent.h"
#include "GameModes/PGGameModeBase.h"
#include "Movement/PGRunnerMovementComponent.h"
#include "RunEvents/PGMovingRunEventHazardActor.h"
#include "Utilities/PGWeightedRandom.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGRunEventSubsystem, Log, All);

namespace
{
float GetMovingHazardLaneSign(EPGMovingHazardLane Lane)
{
	switch (Lane)
	{
	case EPGMovingHazardLane::Left:
		return -1.0f;
	case EPGMovingHazardLane::Right:
		return 1.0f;
	case EPGMovingHazardLane::Center:
	default:
		return 0.0f;
	}
}
}

void UPGRunEventSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	LoadSettings();

	const int32 Seed = CachedRunEventSettings.bUseRandomSeedEachRun ? FMath::Rand() : CachedRunEventSettings.RandomSeed;
	RandomStream.Initialize(Seed);

	InitializeRunnerReferences();
	BindGameMode();
}

void UPGRunEventSubsystem::Deinitialize()
{
	if (BoundGameMode && bIsBoundToGameMode)
	{
		BoundGameMode->OnRunStarted.RemoveDynamic(this, &UPGRunEventSubsystem::HandleRunStarted);
		BoundGameMode->OnRunFailed.RemoveDynamic(this, &UPGRunEventSubsystem::HandleRunFailed);
		BoundGameMode->OnRunResumed.RemoveDynamic(this, &UPGRunEventSubsystem::HandleRunResumed);
	}

	BoundGameMode = nullptr;
	bIsBoundToGameMode = false;
	ClearPendingEvent(true);
	if (ActiveEventType != EPGRunEventType::None)
	{
		OnRunEventCanceled.Broadcast(ActiveEventType);
		ActiveEventType = EPGRunEventType::None;
		ActiveEventEndTime = 0.0f;
	}
	DestroyActiveMovingHazards();

	Super::Deinitialize();
}

void UPGRunEventSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	BindGameMode();
}

bool UPGRunEventSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UPGRunEventSubsystem::Tick(float DeltaTime)
{
	if (!bIsBoundToGameMode)
	{
		BindGameMode();
		if (!bIsBoundToGameMode)
		{
			return;
		}
	}

	if (!bIsRunActive)
	{
		return;
	}

	TickAccumulator += DeltaTime;
	if (TickAccumulator < 0.2f)
	{
		return;
	}

	// 환경 이벤트는 초 단위 예약/전조가 핵심이라 매 프레임 처리할 필요가 없다.
	// 0.2초 단위로 묶어 모바일 Tick 비용을 줄이고, 누적 시간을 넘겨 예약 타이밍은 유지한다.
	const float ProcessDeltaSeconds = TickAccumulator;
	TickAccumulator = 0.0f;
	ProcessRunEventTick(ProcessDeltaSeconds);
}

TStatId UPGRunEventSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UPGRunEventSubsystem, STATGROUP_Tickables);
}

bool UPGRunEventSubsystem::IsTickable() const
{
	return !IsTemplate() && (bIsRunActive || !bIsBoundToGameMode);
}

float UPGRunEventSubsystem::GetDifficultyAlpha() const
{
	return CachedDifficultySettings.GetDifficultyAlpha(RunElapsedTime);
}

void UPGRunEventSubsystem::HandleRunStarted()
{
	// Retry/새 런 모두 이전 런의 예약/활성 이벤트를 완전히 비운 뒤 다시 스케줄한다.
	// 적재물 드롭은 별도 CargoDeliverySubsystem이 관리하므로 여기서는 Wind/Snowstorm/MovingHazard만 다룬다.
	bIsRunActive = true;
	RunElapsedTime = 0.0f;
	TickAccumulator = 0.0f;
	NextScheduleRetryTime = 0.0f;
	LastTriggeredTimeByEvent.Reset();
	ClearPendingEvent(false);
	if (ActiveEventType != EPGRunEventType::None)
	{
		OnRunEventCanceled.Broadcast(ActiveEventType);
		ActiveEventType = EPGRunEventType::None;
		ActiveEventEndTime = 0.0f;
	}
	DestroyActiveMovingHazards();
	InitializeRunnerReferences();
	ScheduleNextEvent();
}

void UPGRunEventSubsystem::HandleRunFailed(FName FailureReason)
{
	bIsRunActive = false;
	ClearPendingEvent(true);
	if (ActiveEventType != EPGRunEventType::None)
	{
		OnRunEventCanceled.Broadcast(ActiveEventType);
		ActiveEventType = EPGRunEventType::None;
		ActiveEventEndTime = 0.0f;
	}
	if (RunnerEnvironmentComponent)
	{
		RunnerEnvironmentComponent->ClearEnvironmentEffects();
	}
	DestroyActiveMovingHazards();
}

void UPGRunEventSubsystem::LoadSettings()
{
	if (const UPGGameSettings* Settings = GetDefault<UPGGameSettings>())
	{
		CachedRunEventSettings = Settings->RunEvent;
		CachedMovingHazardSettings = Settings->MovingHazard;
		CachedDifficultySettings = Settings->Difficulty;
		LoadedEventSet = CachedRunEventSettings.DefaultRunEventSet.Get();
	}
}

void UPGRunEventSubsystem::InitializeRunnerReferences()
{
	RunnerCharacter = BoundGameMode ? BoundGameMode->GetPlayableRunnerCharacter() : nullptr;
	RunnerEnvironmentComponent = RunnerCharacter ? RunnerCharacter->FindComponentByClass<UPGRunnerEnvironmentComponent>() : nullptr;
}

void UPGRunEventSubsystem::HandleRunResumed()
{
	// 실패 때 예약·활성 이벤트는 모두 취소됐다. 난이도 시간은 이어 가며 새 이벤트만 다시 예약한다.
	bIsRunActive = true;
	TickAccumulator = 0.0f;
	InitializeRunnerReferences();
	ScheduleNextEvent();
}

void UPGRunEventSubsystem::BindGameMode()
{
	if (bIsBoundToGameMode || !GetWorld())
	{
		return;
	}

	BoundGameMode = GetWorld()->GetAuthGameMode<APGGameModeBase>();
	if (BoundGameMode)
	{
		// WorldSubsystem 초기화가 GameMode BeginPlay보다 먼저 올 수 있으므로,
		// 바인딩은 Initialize/OnWorldBeginPlay/Tick에서 모두 재시도 가능한 형태로 둔다.
		BoundGameMode->OnRunStarted.AddDynamic(this, &UPGRunEventSubsystem::HandleRunStarted);
		BoundGameMode->OnRunFailed.AddDynamic(this, &UPGRunEventSubsystem::HandleRunFailed);
		BoundGameMode->OnRunResumed.AddDynamic(this, &UPGRunEventSubsystem::HandleRunResumed);
		bIsBoundToGameMode = true;

		if (BoundGameMode->GetRunState() == EPGRunState::Running)
		{
			HandleRunStarted();
		}
	}
}

void UPGRunEventSubsystem::ProcessRunEventTick(float DeltaSeconds)
{
	BindGameMode();

	if (!BoundGameMode || BoundGameMode->GetRunState() != EPGRunState::Running)
	{
		return;
	}

	RunElapsedTime += DeltaSeconds;

	if (ActiveEventType != EPGRunEventType::None && ActiveEventEndTime > 0.0f && RunElapsedTime >= ActiveEventEndTime)
	{
		FinishActiveEvent();
	}

	if (!RunnerEnvironmentComponent)
	{
		InitializeRunnerReferences();
	}

	if (PendingEventType == EPGRunEventType::None)
	{
		if (RunElapsedTime >= NextScheduleRetryTime)
		{
			ScheduleNextEvent();
		}
		return;
	}

	if (PendingEventState == EPGRunEventState::Scheduled && RunElapsedTime >= PendingEventFireTime - FMath::Max(0.0f, CachedRunEventSettings.PreviewLeadTime))
	{
		// 전조 UI/SFX는 실제 이벤트보다 먼저 알린다. 플레이어가 적재물과 중심을 준비할 시간을 주기 위한 단계다.
		PendingEventState = EPGRunEventState::Preview;
		OnRunEventPreviewStarted.Broadcast(PendingEventType, GetTimeUntilPendingEvent(), PendingEventDirection);
	}

	if (RunElapsedTime >= PendingEventFireTime)
	{
		TriggerPendingEvent();
		ScheduleNextEvent();
	}
}

void UPGRunEventSubsystem::ScheduleNextEvent()
{
	ClearPendingEvent(false);

	if (ActiveEventType != EPGRunEventType::None)
	{
		// 활성 이벤트가 겹치면 실패 원인이 불분명해진다. 현재 이벤트가 끝난 뒤 다시 예약한다.
		NextScheduleRetryTime = ActiveEventEndTime > 0.0f ? ActiveEventEndTime : RunElapsedTime + 1.0f;
		return;
	}

	if (RunElapsedTime < CachedRunEventSettings.SafeInitialRunTime)
	{
		// GO 직후에는 첫 적재물/조작 안정화가 우선이므로 환경 이벤트를 잠시 늦춘다.
		NextScheduleRetryTime = CachedRunEventSettings.SafeInitialRunTime;
		return;
	}

	FPGRunEventRule SelectedRule;
	if (!SelectEventRule(SelectedRule))
	{
		NextScheduleRetryTime = RunElapsedTime + 1.0f;
		return;
	}

	const float DifficultyAlpha = GetDifficultyAlpha();
	const float StartInterval = FMath::Max(CachedRunEventSettings.MinEventInterval, CachedRunEventSettings.BaseEventInterval);
	const float EndInterval = FMath::Max(0.5f, CachedRunEventSettings.MinEventInterval);

	// 난이도가 오를수록 예약 간격을 줄인다. 이벤트 강도/종류는 룰 데이터가 담당하고,
	// Subsystem은 언제 어떤 룰을 발동할지만 결정한다.
	PendingEventRule = SelectedRule;
	PendingEventType = SelectedRule.EventType;
	PendingEventState = EPGRunEventState::Scheduled;
	if (SelectedRule.EventType == EPGRunEventType::Wind)
	{
		PendingEventDirection = SelectedRule.bRandomizeDirection && RandomStream.RandRange(0, 1) == 0 ? -1.0f : 1.0f;
	}
	else if (SelectedRule.EventType == EPGRunEventType::MovingHazard)
	{
		PendingEventDirection = ResolveMovingHazardLaneSign(SelectedRule);
	}
	else
	{
		PendingEventDirection = 0.0f;
	}
	PendingEventFireTime = RunElapsedTime + FMath::Lerp(StartInterval, EndInterval, DifficultyAlpha);
}

#if !UE_BUILD_SHIPPING
bool UPGRunEventSubsystem::DebugScheduleEvent(EPGRunEventType EventType, float Direction)
{
	if (!BoundGameMode || BoundGameMode->GetRunState() != EPGRunState::Running)
	{
		return false;
	}

	const FPGRunEventRule* Rule = GetEffectiveEventRules().FindByPredicate([EventType](const FPGRunEventRule& Candidate)
	{
		return Candidate.EventType == EventType;
	});
	if (!Rule)
	{
		return false;
	}

	// 진행 중인 이벤트와 겹치면 연출이 섞이므로 먼저 끝내고, 전조 시간이 지난 뒤 발동하도록 예약한다.
	// FinishActiveEvent는 연출만 정리하므로 남은 바람/눈바람 효과도 함께 해제해, 화면에 원인이 보이지 않는 밀림이 남지 않게 한다.
	FinishActiveEvent();
	if (RunnerEnvironmentComponent)
	{
		RunnerEnvironmentComponent->ClearEnvironmentEffects();
	}
	ClearPendingEvent(true);
	PendingEventRule = *Rule;
	PendingEventType = EventType;
	PendingEventState = EPGRunEventState::Scheduled;
	// 바람은 방향이 0이면 효과 없이 연출만 나오므로 일반 예약처럼 항상 좌우 한쪽(-1/1)으로 맞춘다.
	PendingEventDirection = EventType == EPGRunEventType::Wind ? (Direction < 0.0f ? -1.0f : 1.0f) : FMath::Clamp(Direction, -1.0f, 1.0f);
	PendingEventFireTime = RunElapsedTime + FMath::Max(0.0f, CachedRunEventSettings.PreviewLeadTime);
	return true;
}
#endif

bool UPGRunEventSubsystem::SelectEventRule(FPGRunEventRule& OutRule)
{
	const TArray<FPGRunEventRule>& Rules = GetEffectiveEventRules();
	const float DifficultyAlpha = GetDifficultyAlpha();
	const int32 SelectedRuleIndex = PGWeightedRandom::SelectIndex(
		Rules.Num(),
		[this, &Rules, DifficultyAlpha](int32 RuleIndex)
	{
		const FPGRunEventRule& Rule = Rules[RuleIndex];
		if (!CanUseRule(Rule))
		{
			return 0.0f;
		}

		return FMath::Max(0.0f, Rule.BaseWeight + Rule.DifficultyWeight * DifficultyAlpha);
	}, RandomStream);

	if (!Rules.IsValidIndex(SelectedRuleIndex))
	{
		return false;
	}

	// 가중치는 난이도 알파를 반영한다. 같은 룰이라도 런 후반에는 DifficultyWeight가 선택 확률을 밀어 올린다.
	OutRule = Rules[SelectedRuleIndex];
	return true;
}

const TArray<FPGRunEventRule>& UPGRunEventSubsystem::GetEffectiveEventRules() const
{
	static const TArray<FPGRunEventRule> EmptyRules;

	if (!LoadedEventSet)
	{
		LoadedEventSet = CachedRunEventSettings.DefaultRunEventSet.Get();
	}

	if (!LoadedEventSet || LoadedEventSet->EventRules.IsEmpty())
	{
		if (!bWarnedMissingEventRules)
		{
			UE_LOG(LogPGRunEventSubsystem, Warning, TEXT("RunEvent scheduling disabled: DefaultRunEventSet is not set or contains no EventRules."));
			bWarnedMissingEventRules = true;
		}
		return EmptyRules;
	}

	return LoadedEventSet->EventRules;
}

bool UPGRunEventSubsystem::CanUseRule(const FPGRunEventRule& Rule) const
{
	if (Rule.EventType == EPGRunEventType::None)
	{
		return false;
	}

	if (RunElapsedTime < Rule.MinRunTime || GetDifficultyAlpha() < Rule.MinDifficultyAlpha)
	{
		return false;
	}

	if (const float* LastTriggeredTime = LastTriggeredTimeByEvent.Find(Rule.EventType))
	{
		if (RunElapsedTime - *LastTriggeredTime < FMath::Max(0.0f, Rule.CooldownSeconds))
		{
			return false;
		}
	}

	return Rule.BaseWeight + Rule.DifficultyWeight * GetDifficultyAlpha() > 0.0f;
}

void UPGRunEventSubsystem::TriggerPendingEvent()
{
	if (PendingEventType == EPGRunEventType::None)
	{
		return;
	}

	if (PendingEventType != EPGRunEventType::MovingHazard && !RunnerEnvironmentComponent)
	{
		// 러너 환경 컴포넌트를 못 찾은 경우에는 예정된 이벤트를 취소해 UI 전조가 남지 않게 한다.
		ClearPendingEvent(true);
		return;
	}

	bool bTriggered = true;

	switch (PendingEventType)
	{
	case EPGRunEventType::Wind:
	{
		RunnerEnvironmentComponent->ApplyWind(PendingEventDirection, PendingEventRule.Strength, PendingEventRule.Duration);
		break;
	}
	case EPGRunEventType::Snowstorm:
		RunnerEnvironmentComponent->ApplySnowstorm(PendingEventRule.Duration, PendingEventRule.Strength);
		break;
	case EPGRunEventType::MovingHazard:
		bTriggered = SpawnMovingHazard();
		break;
	case EPGRunEventType::None:
	default:
		break;
	}

	if (!bTriggered)
	{
		// 발동에 실패한 예약은 전조 연출을 취소해 다음 전조까지 경고가 남지 않게 한다.
		ClearPendingEvent(true);
		return;
	}

	// 트리거가 성공한 이벤트만 쿨다운과 Active 상태에 기록한다.
	// 실패한 예약까지 쿨다운에 넣으면 일정 시간 이벤트가 비는 문제가 생긴다.
	PendingEventState = EPGRunEventState::Triggered;
	LastTriggeredTimeByEvent.FindOrAdd(PendingEventType) = RunElapsedTime;
	OnRunEventTriggered.Broadcast(PendingEventType, PendingEventRule.Duration, PendingEventDirection);
	StartActiveEvent(PendingEventType, PendingEventRule.Duration);
}

void UPGRunEventSubsystem::ClearPendingEvent(bool bBroadcastCancel)
{
	const EPGRunEventType PreviousEventType = PendingEventType;
	const bool bWasPendingOrPreview = PendingEventType != EPGRunEventType::None && PendingEventState != EPGRunEventState::Triggered;
	PendingEventRule = FPGRunEventRule();
	PendingEventType = EPGRunEventType::None;
	PendingEventState = EPGRunEventState::Inactive;
	PendingEventDirection = 0.0f;
	PendingEventFireTime = 0.0f;

	if (bBroadcastCancel && bWasPendingOrPreview)
	{
		OnRunEventCanceled.Broadcast(PreviousEventType);
	}
}

void UPGRunEventSubsystem::StartActiveEvent(EPGRunEventType EventType, float Duration)
{
	if (EventType != EPGRunEventType::None && Duration > KINDA_SMALL_NUMBER)
	{
		ActiveEventType = EventType;
		ActiveEventEndTime = RunElapsedTime + Duration;
		return;
	}

	ActiveEventType = EPGRunEventType::None;
	ActiveEventEndTime = 0.0f;
	if (EventType != EPGRunEventType::None)
	{
		OnRunEventFinished.Broadcast(EventType);
	}
}

void UPGRunEventSubsystem::FinishActiveEvent()
{
	if (ActiveEventType == EPGRunEventType::None)
	{
		return;
	}

	const EPGRunEventType FinishedEventType = ActiveEventType;
	ActiveEventType = EPGRunEventType::None;
	ActiveEventEndTime = 0.0f;
	OnRunEventFinished.Broadcast(FinishedEventType);
}

float UPGRunEventSubsystem::ResolveMovingHazardLaneSign(const FPGRunEventRule& Rule)
{
	if (Rule.HazardLaneMode == EPGMovingHazardLaneMode::Fixed)
	{
		return GetMovingHazardLaneSign(Rule.FixedHazardLane);
	}

	return GetMovingHazardLaneSign(static_cast<EPGMovingHazardLane>(RandomStream.RandRange(0, 2)));
}

bool UPGRunEventSubsystem::SpawnMovingHazard()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	if (!RunnerCharacter)
	{
		InitializeRunnerReferences();
	}

	if (!RunnerCharacter)
	{
		return false;
	}

	TSubclassOf<APGMovingRunEventHazardActor> HazardClass = CachedMovingHazardSettings.HazardActorClass.Get();
	if (!HazardClass)
	{
		if (!bWarnedMissingMovingHazardClass)
		{
			UE_LOG(LogPGRunEventSubsystem, Warning, TEXT("MovingHazard RunEvent skipped: HazardActorClass is not assigned or not preloaded."));
			bWarnedMissingMovingHazardClass = true;
		}
		return false;
	}

	UPGRunnerMovementComponent* RunnerMovement = Cast<UPGRunnerMovementComponent>(RunnerCharacter->GetCharacterMovement());
	const FVector RunnerForward = RunnerMovement ? RunnerMovement->GetRunnerForwardVector() : RunnerCharacter->GetActorForwardVector();
	const FVector RunnerRight = RunnerMovement ? RunnerMovement->GetRunnerRightVector() : RunnerCharacter->GetActorRightVector();
	const float LaneSign = PendingEventDirection;
	const float CurrentBalanceOffset = RunnerMovement ? RunnerMovement->GetBalanceOffset() : 0.0f;
	const FVector TrackCenterLocation = RunnerCharacter->GetActorLocation() - RunnerRight * CurrentBalanceOffset;

	// MovingHazard의 좌/중/우는 러너의 현재 위치가 아니라 트랙 중심선 기준 가상 위험 구역입니다.
	// 러너가 이미 한쪽으로 치우친 상태에서 다시 LaneOffset을 더하면 위험 Actor가 트랙 밖에 스폰될 수 있습니다.
	const FVector SpawnLocation =
		TrackCenterLocation
		+ RunnerForward * FMath::Max(0.0f, CachedMovingHazardSettings.SpawnAheadDistance)
		+ RunnerRight * (LaneSign * FMath::Max(0.0f, CachedMovingHazardSettings.VirtualLaneSpacing));

	const FVector MovementVelocity = CachedMovingHazardSettings.MovementVelocity;
	const FRotator SpawnRotation = MovementVelocity.IsNearlyZero() ? FRotator::ZeroRotator : MovementVelocity.Rotation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = RunnerCharacter;
	SpawnParams.Instigator = RunnerCharacter;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	APGMovingRunEventHazardActor* HazardActor = World->SpawnActor<APGMovingRunEventHazardActor>(HazardClass, SpawnLocation, SpawnRotation, SpawnParams);
	if (!HazardActor)
	{
		return false;
	}

	// 좌/우 구역은 트랙 중앙 방향으로 밀어내고, 중앙 구역은 Actor가 오버랩 위치 기준으로 밀 방향을 결정합니다.
	const float PushDirectionSign = LaneSign < 0.0f ? 1.0f : (LaneSign > 0.0f ? -1.0f : 0.0f);
	const float Lifetime = CachedMovingHazardSettings.HazardLifetime > 0.0f
		? CachedMovingHazardSettings.HazardLifetime
		: FMath::Max(0.1f, PendingEventRule.Duration);
	HazardActor->InitializeMovingHazard(
		CachedMovingHazardSettings,
		MovementVelocity,
		Lifetime,
		PendingEventRule.Strength,
		PushDirectionSign);

	ActiveMovingHazards.RemoveAll([](const TObjectPtr<APGMovingRunEventHazardActor>& ExistingHazard)
	{
		return !IsValid(ExistingHazard);
	});
	ActiveMovingHazards.Add(HazardActor);
	return true;
}

void UPGRunEventSubsystem::DestroyActiveMovingHazards()
{
	for (APGMovingRunEventHazardActor* HazardActor : ActiveMovingHazards)
	{
		if (IsValid(HazardActor))
		{
			HazardActor->Destroy();
		}
	}

	ActiveMovingHazards.Reset();
}

