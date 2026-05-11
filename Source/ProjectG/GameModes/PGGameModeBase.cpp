// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameModes/PGGameModeBase.h"

#include "Character/PGRunnerCharacter.h"
#include "Engine/EngineBaseTypes.h"
#include "Player/PGPlayerController.h"
#include "Settings/PGGameSettings.h"
#include "TimerManager.h"

APGGameModeBase::APGGameModeBase()
{
	DefaultPawnClass = APGRunnerCharacter::StaticClass();
	PlayerControllerClass = APGPlayerController::StaticClass();
}

void APGGameModeBase::BeginPlay()
{
	Super::BeginPlay();

	RunState = EPGRunState::Home;

	const UWorld* World = GetWorld();
	const FString StartFlowOption = World ? World->URL.GetOption(TEXT("StartFlow="), TEXT("")) : FString();
	if (StartFlowOption.Equals(TEXT("RetryCountdown"), ESearchCase::IgnoreCase))
	{
		GetWorldTimerManager().SetTimer(RetryCountdownFallbackTimerHandle, this, &APGGameModeBase::HandleRetryCountdownFallback, 1.0f, false);
	}
}

void APGGameModeBase::RequestStartDelivery()
{
	if (RunState != EPGRunState::Home)
	{
		return;
	}

	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	const float CountdownDuration = Settings ? FMath::Max(0.0f, Settings->HomeFlow.RunStartCountdownDuration) : 3.0f;

	RunState = EPGRunState::Countdown;
	GetWorldTimerManager().ClearTimer(RetryCountdownFallbackTimerHandle);
	OnRunCountdownStarted.Broadcast(CountdownDuration);

	if (CountdownDuration <= KINDA_SMALL_NUMBER)
	{
		FinishRunCountdown();
		return;
	}

	GetWorldTimerManager().SetTimer(RunCountdownTimerHandle, this, &APGGameModeBase::FinishRunCountdown, CountdownDuration, false);
}

void APGGameModeBase::RequestRunFailed(FName FailureReason)
{
	if (RunState == EPGRunState::Failed)
	{
		return;
	}

	RunState = EPGRunState::Failed;
	GetWorldTimerManager().ClearTimer(RunCountdownTimerHandle);
	GetWorldTimerManager().ClearTimer(RetryCountdownFallbackTimerHandle);
	OnRunFailed.Broadcast(FailureReason);
}

void APGGameModeBase::FinishRunCountdown()
{
	if (RunState != EPGRunState::Countdown)
	{
		return;
	}

	RunState = EPGRunState::Running;
	OnRunStarted.Broadcast();
}

void APGGameModeBase::HandleRetryCountdownFallback()
{
	if (RunState == EPGRunState::Home)
	{
		RequestStartDelivery();
	}
}

