// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameModes/PGGameModeBase.h"

#include "Character/PGRunnerCharacter.h"
#include "Engine/EngineBaseTypes.h"
#include "EngineUtils.h"
#include "Player/PGPlayerController.h"
#include "Settings/PGGameSettings.h"
#include "TimerManager.h"
#include "Wallet/PGPlayerWalletSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGGameModeBase, Log, All);

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

	MoveRunnerToRunStartPoint();

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

void APGGameModeBase::MoveRunnerToRunStartPoint()
{
	UWorld* World = GetWorld();
	APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	APGRunnerCharacter* RunnerCharacter = PlayerController ? Cast<APGRunnerCharacter>(PlayerController->GetPawn()) : nullptr;
	if (!World || !RunnerCharacter)
	{
		return;
	}

	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	const FName RunStartPointTag = Settings ? Settings->HomeFlow.RunStartPointTag : FName(TEXT("RunStartPoint"));
	if (RunStartPointTag.IsNone())
	{
		return;
	}

	for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
	{
		AActor* CandidateActor = *ActorIt;
		if (CandidateActor && CandidateActor != RunnerCharacter && CandidateActor->ActorHasTag(RunStartPointTag))
		{
			RunnerCharacter->MoveToRunStartTransform(CandidateActor->GetActorTransform());
			return;
		}
	}

	if (!bRunStartPointWarningLogged)
	{
		bRunStartPointWarningLogged = true;
		UE_LOG(LogPGGameModeBase, Warning, TEXT("RunStartPoint actor with tag '%s' was not found. Runner will start from its current location."), *RunStartPointTag.ToString());
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

