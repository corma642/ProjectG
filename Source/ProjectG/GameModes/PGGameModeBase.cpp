// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameModes/PGGameModeBase.h"

#include "Character/PGRunnerCharacter.h"
#include "Engine/EngineBaseTypes.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Loading/PGRuntimePreloadSubsystem.h"
#include "Player/PGPlayerController.h"
#include "Settings/PGGameSettings.h"
#include "TimerManager.h"
#include "Track/PGRunDirector.h"
#include "Track/PGTrackSegment.h"
#include "User/PGUserDataSubsystem.h"

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
	bIsCurrentMapSetupValid = ValidateCurrentMapSetup();

	const UWorld* World = GetWorld();
	const FString StartFlowOption = World ? World->URL.GetOption(TEXT("StartFlow="), TEXT("")) : FString();
	bIsRetryEntry = StartFlowOption.Equals(TEXT("RetryCountdown"), ESearchCase::IgnoreCase);
	if (bIsRetryEntry)
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
	if (!bIsCurrentMapSetupValid)
	{
		UE_LOG(LogPGGameModeBase, Warning, TEXT("Start delivery aborted: the current map does not satisfy the run setup contract."));
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UPGRuntimePreloadSubsystem* PreloadSubsystem = GameInstance ? GameInstance->GetSubsystem<UPGRuntimePreloadSubsystem>() : nullptr;
	if (!PreloadSubsystem || !PreloadSubsystem->IsReadyForRun())
	{
		if (PreloadSubsystem)
		{
			PreloadSubsystem->RequestPreload(EPGRuntimePreloadGroup::Run);
			if (PreloadSubsystem->IsGroupReady(EPGRuntimePreloadGroup::Run))
			{
				PreloadSubsystem->LogRunReadinessIssues();
			}
		}
		return;
	}

	// Retry 진입은 플레이어가 이미 준비돼 있으므로 더 짧은 카운트다운으로 실패→재도전 대기를 줄인다.
	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	const float CountdownDuration = Settings ? FMath::Max(0.0f, bIsRetryEntry ? Settings->HomeFlow.RetryCountdownDuration : Settings->HomeFlow.RunStartCountdownDuration) : 3.0f;
	RunCountdownDuration = CountdownDuration;
	// 시작 구간은 첫 START 카운트다운 동안 달리는 거리(RunStartCountdownDuration × ForwardSpeed)에 맞춰 배치돼 있다.
	// Retry는 짧은 카운트다운만큼 덜 달리므로 그 차이만큼 앞에서 출발시켜 GO 지점과 거리 기준을 첫 START와 같게 유지한다.
	const float RunStartForwardOffset = bIsRetryEntry && Settings
		? FMath::Max(0.0f, Settings->HomeFlow.RunStartCountdownDuration - CountdownDuration) * Settings->Runner.ForwardSpeed
		: 0.0f;

	APGRunnerCharacter* RunnerCharacter = EnsureRunnerPossessedForRun();
	if (!RunnerCharacter)
	{
		UE_LOG(LogPGGameModeBase, Warning, TEXT("Start delivery aborted: no playable runner is possessed."));
		return;
	}
	RunnerCharacter->PrepareTransformForRunStart(RunStartForwardOffset);

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
		if (RunState == EPGRunState::Home)
		{
			// Retry는 Home UI가 없으므로 필수 예열이 아직 끝나지 않았으면 짧게 재시도한다.
			GetWorldTimerManager().SetTimer(RetryCountdownFallbackTimerHandle, this, &APGGameModeBase::HandleRetryCountdownFallback, 0.25f, false);
		}
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

	APGRunnerCharacter* PlayableRunner = GetPlayableRunnerCharacter();
	if (!PlayableRunner)
	{
		return nullptr;
	}

	if (PlayerController->GetPawn() != PlayableRunner)
	{
		PlayerController->Possess(PlayableRunner);
	}
	return Cast<APGRunnerCharacter>(PlayerController->GetPawn());
}

APGRunnerCharacter* APGGameModeBase::GetPlayableRunnerCharacter() const
{
	UWorld* World = GetWorld();
	const APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	if (APGRunnerCharacter* PossessedRunner = PlayerController ? Cast<APGRunnerCharacter>(PlayerController->GetPawn()) : nullptr)
	{
		return PossessedRunner;
	}

	return FindSingleRunnerInWorld();
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

bool APGGameModeBase::ValidateCurrentMapSetup()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	int32 RunnerCount = 0;
	for (TActorIterator<APGRunnerCharacter> RunnerIt(World); RunnerIt; ++RunnerIt)
	{
		RunnerCount += IsValid(*RunnerIt) ? 1 : 0;
	}

	int32 DirectorCount = 0;
	APGRunDirector* FoundDirector = nullptr;
	for (TActorIterator<APGRunDirector> DirectorIt(World); DirectorIt; ++DirectorIt)
	{
		if (IsValid(*DirectorIt))
		{
			FoundDirector = *DirectorIt;
			++DirectorCount;
		}
	}

	bool bValid = true;
	if (RunnerCount != 1)
	{
		UE_LOG(LogPGGameModeBase, Warning, TEXT("Map setup error: expected exactly one APGRunnerCharacter, found %d."), RunnerCount);
		bValid = false;
	}
	if (DirectorCount != 1 || !FoundDirector)
	{
		UE_LOG(LogPGGameModeBase, Warning, TEXT("Map setup error: expected exactly one APGRunDirector, found %d."), DirectorCount);
		return false;
	}
	if (!FoundDirector->GetInitialSegment())
	{
		UE_LOG(LogPGGameModeBase, Warning, TEXT("Map setup error: RunDirector '%s' has no InitialSegment."), *GetNameSafe(FoundDirector));
		bValid = false;
	}
	if (!FoundDirector->GetSegmentSet())
	{
		UE_LOG(LogPGGameModeBase, Warning, TEXT("Map setup error: RunDirector '%s' has no SegmentSet."), *GetNameSafe(FoundDirector));
		bValid = false;
	}
	if (const APGTrackSegment* InitialSegment = FoundDirector->GetInitialSegment())
	{
		const FVector SegmentForward = InitialSegment->GetSegmentForwardVector().GetSafeNormal();
		if (FVector::DotProduct(SegmentForward, FVector::ForwardVector) < 0.99f)
		{
			UE_LOG(LogPGGameModeBase, Warning, TEXT("Map setup error: InitialSegment '%s' must face world +X."), *GetNameSafe(InitialSegment));
			bValid = false;
		}
	}
	return bValid;
}

bool APGGameModeBase::SettleCurrentRunRewards(FName Reason)
{
	if (bRunRewardsSettled)
	{
		return true;
	}

	UWorld* World = GetWorld();
	const APGRunnerCharacter* RunnerCharacter = GetPlayableRunnerCharacter();
	const int32 RunReceipts = RunnerCharacter ? RunnerCharacter->GetRunReceipts() : 0;
	const int32 RunScore = RunnerCharacter ? RunnerCharacter->GetRunScore() : 0;
	if (RunReceipts <= 0 && RunScore <= 0)
	{
		bRunRewardsSettled = true;
		return true;
	}

	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UPGUserDataSubsystem* UserDataSubsystem = GameInstance ? GameInstance->GetSubsystem<UPGUserDataSubsystem>() : nullptr;
	if (!UserDataSubsystem)
	{
		UE_LOG(LogPGGameModeBase, Warning, TEXT("Run reward settlement failed: UPGUserDataSubsystem is unavailable."));
		return false;
	}

	bRunRewardsSettled = UserDataSubsystem->TrySettleRunRewards(RunReceipts, RunScore, Reason);
	return bRunRewardsSettled;
}

