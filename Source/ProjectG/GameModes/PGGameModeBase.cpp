// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameModes/PGGameModeBase.h"

#include "Character/PGRunnerCharacter.h"
#include "Engine/EngineBaseTypes.h"
#include "Player/PGPlayerController.h"
#include "Settings/PGGameSettings.h"
#include "TimerManager.h"
#include "Wallet/PGPlayerWalletSubsystem.h"

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
	bRunRewardsSettled = false;
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
	SettleCurrentRunRewards(FailureReason);
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

void APGGameModeBase::SettleCurrentRunRewards(FName Reason)
{
	if (bRunRewardsSettled)
	{
		return;
	}

	bRunRewardsSettled = true;

	UWorld* World = GetWorld();
	const APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	const APGRunnerCharacter* RunnerCharacter = PlayerController ? Cast<APGRunnerCharacter>(PlayerController->GetPawn()) : nullptr;
	const int32 RunReceipts = RunnerCharacter ? RunnerCharacter->GetRunReceipts() : 0;
	if (RunReceipts <= 0)
	{
		return;
	}

	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UPGPlayerWalletSubsystem* WalletSubsystem = GameInstance ? GameInstance->GetSubsystem<UPGPlayerWalletSubsystem>() : nullptr;
	if (WalletSubsystem)
	{
		WalletSubsystem->AddReceipts(RunReceipts, Reason);
	}
}

