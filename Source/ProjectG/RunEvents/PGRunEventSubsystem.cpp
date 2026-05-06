// Copyright Epic Games, Inc. All Rights Reserved.

#include "RunEvents/PGRunEventSubsystem.h"

#include "Character/PGRunnerCharacter.h"
#include "DrawDebugHelpers.h"
#include "Environment/PGRunnerEnvironmentComponent.h"
#include "GameModes/PGGameModeBase.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGRunEventSubsystem, Log, All);

void UPGRunEventSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	LoadSettings();
	InitializeDefaultRules();

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
	return CachedRunEventSettings.DifficultyRampDuration > KINDA_SMALL_NUMBER
		? FMath::Clamp(RunElapsedTime / CachedRunEventSettings.DifficultyRampDuration, 0.0f, 1.0f)
		: 1.0f;
}

void UPGRunEventSubsystem::HandleRunStarted()
{
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
}

void UPGRunEventSubsystem::LoadSettings()
{
	if (const UPGGameSettings* Settings = GetDefault<UPGGameSettings>())
	{
		CachedRunEventSettings = Settings->RunEvent;
		LoadedEventSet = CachedRunEventSettings.DefaultRunEventSet.LoadSynchronous();
		if (!LoadedEventSet && !CachedRunEventSettings.DefaultRunEventSet.IsNull())
		{
			UE_LOG(LogPGRunEventSubsystem, Warning, TEXT("DefaultRunEventSet could not be loaded. C++ default run event rules will be used."));
		}
	}
}

void UPGRunEventSubsystem::InitializeDefaultRules()
{
	DefaultRules.Reset();

	FPGRunEventRule WindRule;
	WindRule.EventType = EPGRunEventType::Wind;
	WindRule.MinRunTime = 10.0f;
	WindRule.MinDifficultyAlpha = 0.1f;
	WindRule.BaseWeight = 1.0f;
	WindRule.DifficultyWeight = 1.0f;
	WindRule.CooldownSeconds = 5.0f;
	WindRule.Duration = 3.0f;
	WindRule.Strength = 1.0f;
	DefaultRules.Add(WindRule);

	FPGRunEventRule SnowstormRule;
	SnowstormRule.EventType = EPGRunEventType::Snowstorm;
	SnowstormRule.MinRunTime = 20.0f;
	SnowstormRule.MinDifficultyAlpha = 0.25f;
	SnowstormRule.BaseWeight = 0.75f;
	SnowstormRule.DifficultyWeight = 1.0f;
	SnowstormRule.CooldownSeconds = 7.0f;
	SnowstormRule.Duration = 3.0f;
	SnowstormRule.Strength = 1.0f;
	DefaultRules.Add(SnowstormRule);

}

void UPGRunEventSubsystem::InitializeRunnerReferences()
{
	RunnerCharacter = Cast<APGRunnerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	RunnerEnvironmentComponent = RunnerCharacter ? RunnerCharacter->FindComponentByClass<UPGRunnerEnvironmentComponent>() : nullptr;
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
		BoundGameMode->OnRunStarted.AddDynamic(this, &UPGRunEventSubsystem::HandleRunStarted);
		BoundGameMode->OnRunFailed.AddDynamic(this, &UPGRunEventSubsystem::HandleRunFailed);
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
		DrawRunEventDebug();
		return;
	}

	if (PendingEventState == EPGRunEventState::Scheduled && RunElapsedTime >= PendingEventFireTime - FMath::Max(0.0f, CachedRunEventSettings.PreviewLeadTime))
	{
		PendingEventState = EPGRunEventState::Preview;
		OnRunEventPreviewStarted.Broadcast(PendingEventType, GetTimeUntilPendingEvent(), PendingEventDirection);
	}

	if (RunElapsedTime >= PendingEventFireTime)
	{
		TriggerPendingEvent();
		ScheduleNextEvent();
	}

	DrawRunEventDebug();
}

void UPGRunEventSubsystem::ScheduleNextEvent()
{
	ClearPendingEvent(false);

	if (ActiveEventType != EPGRunEventType::None)
	{
		NextScheduleRetryTime = ActiveEventEndTime > 0.0f ? ActiveEventEndTime : RunElapsedTime + 1.0f;
		return;
	}

	if (RunElapsedTime < CachedRunEventSettings.SafeInitialRunTime)
	{
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
	PendingEventRule = SelectedRule;
	PendingEventType = SelectedRule.EventType;
	PendingEventState = EPGRunEventState::Scheduled;
	PendingEventDirection = SelectedRule.EventType == EPGRunEventType::Wind
		? (SelectedRule.bRandomizeDirection && RandomStream.RandRange(0, 1) == 0 ? -1.0f : 1.0f)
		: 0.0f;
	PendingEventFireTime = RunElapsedTime + FMath::Lerp(StartInterval, EndInterval, DifficultyAlpha);
}

bool UPGRunEventSubsystem::SelectEventRule(FPGRunEventRule& OutRule)
{
	const TArray<FPGRunEventRule>& Rules = GetEffectiveEventRules();
	const float DifficultyAlpha = GetDifficultyAlpha();
	float TotalWeight = 0.0f;

	for (const FPGRunEventRule& Rule : Rules)
	{
		if (!CanUseRule(Rule))
		{
			continue;
		}

		TotalWeight += FMath::Max(0.0f, Rule.BaseWeight + Rule.DifficultyWeight * DifficultyAlpha);
	}

	if (TotalWeight <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float Roll = RandomStream.FRandRange(0.0f, TotalWeight);
	float AccumulatedWeight = 0.0f;
	for (const FPGRunEventRule& Rule : Rules)
	{
		if (!CanUseRule(Rule))
		{
			continue;
		}

		const float Weight = FMath::Max(0.0f, Rule.BaseWeight + Rule.DifficultyWeight * DifficultyAlpha);
		AccumulatedWeight += Weight;
		if (Roll <= AccumulatedWeight)
		{
			OutRule = Rule;
			return true;
		}
	}

	return false;
}

const TArray<FPGRunEventRule>& UPGRunEventSubsystem::GetEffectiveEventRules() const
{
	return LoadedEventSet && !LoadedEventSet->EventRules.IsEmpty() ? LoadedEventSet->EventRules : DefaultRules;
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

	return FMath::Max(0.0f, Rule.BaseWeight + Rule.DifficultyWeight * GetDifficultyAlpha()) > KINDA_SMALL_NUMBER;
}

void UPGRunEventSubsystem::TriggerPendingEvent()
{
	if (PendingEventType == EPGRunEventType::None || !RunnerEnvironmentComponent)
	{
		ClearPendingEvent(true);
		return;
	}

	PendingEventState = EPGRunEventState::Triggered;
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
	case EPGRunEventType::None:
	default:
		break;
	}

	if (bTriggered)
	{
		LastTriggeredTimeByEvent.FindOrAdd(PendingEventType) = RunElapsedTime;
		OnRunEventTriggered.Broadcast(PendingEventType, PendingEventRule.Duration, PendingEventDirection);
		StartActiveEvent(PendingEventType, PendingEventRule.Duration);
	}
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

void UPGRunEventSubsystem::DrawRunEventDebug() const
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	if (!CachedRunEventSettings.bDrawRunEventDebug || !GetWorld())
	{
		return;
	}

	const FVector DebugLocation = RunnerCharacter ? RunnerCharacter->GetActorLocation() + FVector::UpVector * 330.0f : FVector::UpVector * 330.0f;
	const FString DebugText = FString::Printf(
		TEXT("RunEvent Time:%.1f Difficulty:%.2f Pending:%d State:%d ETA:%.1f"),
		RunElapsedTime,
		GetDifficultyAlpha(),
		static_cast<int32>(PendingEventType),
		static_cast<int32>(PendingEventState),
		GetTimeUntilPendingEvent());
	DrawDebugString(GetWorld(), DebugLocation, DebugText, nullptr, FColor::Orange, 0.0f, true);
#endif
}
