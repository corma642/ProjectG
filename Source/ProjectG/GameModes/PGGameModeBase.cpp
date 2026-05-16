// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameModes/PGGameModeBase.h"

#include "Character/PGRunnerCharacter.h"
#include "Engine/EngineBaseTypes.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Player/PGPlayerController.h"
#include "Settings/PGGameSettings.h"
#include "TimerManager.h"
#include "Wallet/PGPlayerWalletSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGGameModeBase, Log, All);

APGGameModeBase::APGGameModeBase()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = APGPlayerController::StaticClass();
}

APawn* APGGameModeBase::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
	// Home/Shop에서 보여줄 레벨 배치 러너를 그대로 사용하기 위해 GameMode의 자동 Pawn 스폰을 사용하지 않는다.
	return nullptr;
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

	if (!EnsureRunnerPossessedForRun())
	{
		UE_LOG(LogPGGameModeBase, Warning, TEXT("Start delivery aborted: no playable runner is possessed."));
		return;
	}

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

APGRunnerCharacter* APGGameModeBase::EnsureRunnerPossessedForRun()
{
	UWorld* World = GetWorld();
	APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	if (!World || !PlayerController)
	{
		return nullptr;
	}

	if (APGRunnerCharacter* PossessedRunner = Cast<APGRunnerCharacter>(PlayerController->GetPawn()))
	{
		return PossessedRunner;
	}

	APGRunnerCharacter* SingleRunner = FindSingleRunnerInWorld();
	if (!SingleRunner)
	{
		return nullptr;
	}

	PlayerController->Possess(SingleRunner);
	return Cast<APGRunnerCharacter>(PlayerController->GetPawn());
}

APGRunnerCharacter* APGGameModeBase::FindSingleRunnerInWorld() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	APGRunnerCharacter* FoundRunner = nullptr;
	int32 RunnerCount = 0;
	for (TActorIterator<APGRunnerCharacter> RunnerIt(World); RunnerIt; ++RunnerIt)
	{
		APGRunnerCharacter* Runner = *RunnerIt;
		if (!IsValid(Runner))
		{
			continue;
		}

		FoundRunner = Runner;
		++RunnerCount;
		if (RunnerCount > 1)
		{
			UE_LOG(LogPGGameModeBase, Warning, TEXT("Found multiple APGRunnerCharacter actors. Set exactly one playable runner or possess it before starting."));
			return nullptr;
		}
	}

	if (!FoundRunner)
	{
		UE_LOG(LogPGGameModeBase, Warning, TEXT("No APGRunnerCharacter actor was found for run start."));
	}
	return FoundRunner;
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

