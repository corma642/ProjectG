// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameModes/PGGameModeBase.h"

#include "Character/PGRunnerCharacter.h"
#include "Engine/EngineBaseTypes.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
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

void APGGameModeBase::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	const FString StartFlowOption = UGameplayStatics::ParseOption(Options, TEXT("StartFlow"));
	StartFlow = StartFlowOption.Equals(TEXT("RetryCountdown"), ESearchCase::IgnoreCase) ? EPGRunStartFlow::RetryCountdown
		: StartFlowOption.Equals(TEXT("Tutorial"), ESearchCase::IgnoreCase) ? EPGRunStartFlow::Tutorial
		: EPGRunStartFlow::Home;
}

void APGGameModeBase::BeginPlay()
{
	Super::BeginPlay();

	RunState = EPGRunState::Home;
	bIsCurrentMapSetupValid = ValidateCurrentMapSetup();

	if (StartFlow != EPGRunStartFlow::Home)
	{
		// Retry·튜토리얼 진입은 Home UI가 없으므로 PlayerController 쪽 요청이 늦어도 카운트다운이 시작되게 한다.
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

	// Retry 진입은 플레이어가 이미 준비돼 있으므로 더 짧은 카운트다운으로 실패→재도전 대기를 줄인다. 튜토리얼은 첫 START와 같은 길이로 시작한다.
	const bool bIsRetryEntry = StartFlow == EPGRunStartFlow::RetryCountdown;
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
	bRunReceiptsDoubled = false;
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
	if (RunState == EPGRunState::Failed || bIsTutorialRewind)
	{
		return;
	}

	if (bTutorialGuideActive && RunState == EPGRunState::Running)
	{
		// 튜토리얼 안내 중 실패는 런을 끝내지 않는다. 러너와 런 시스템은 실패처럼 멈추고, 암전이 화면을 덮으면 FinishTutorialRewind가 몇 초 전으로 되돌린다.
		RunState = EPGRunState::Countdown;
		bIsTutorialRewind = true;
		GetWorldTimerManager().ClearTimer(RunCountdownTimerHandle);
		OnRunRewindStarted.Broadcast(FailureReason);
		return;
	}

	RunState = EPGRunState::Failed;
	SettleCurrentRunRewards(FailureReason);
	GetWorldTimerManager().ClearTimer(RunCountdownTimerHandle);
	GetWorldTimerManager().ClearTimer(RetryCountdownFallbackTimerHandle);
	OnRunFailed.Broadcast(FailureReason);
}

void APGGameModeBase::FinishTutorialRewind()
{
	if (!bIsTutorialRewind)
	{
		return;
	}

	bIsTutorialRewind = false;
	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	if (APGRunnerCharacter* RunnerCharacter = GetPlayableRunnerCharacter())
	{
		RunnerCharacter->RewindForTutorial(Settings ? Settings->Tutorial.RewindSeconds : 3.0f);
	}

	// 카운트다운 없이 바로 달린다. 되돌린 자리가 실패 지점보다 몇 초 앞이라 플레이어가 상황을 다시 볼 시간은 그 거리로 충분하다.
	RunState = EPGRunState::Running;
	OnRunResumed.Broadcast();
}

void APGGameModeBase::FinishRunCountdown()
{
	if (RunState != EPGRunState::Countdown)
	{
		return;
	}

	RunState = EPGRunState::Running;
	if (bIsContinueCountdown)
	{
		bIsContinueCountdown = false;
		OnRunResumed.Broadcast();
		return;
	}
	OnRunStarted.Broadcast();
}

bool APGGameModeBase::IsContinueOffered() const
{
	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	return RunState == EPGRunState::Failed && Settings && Settings->RunResult.bEnableContinue && ContinueCount < Settings->RunResult.MaxContinuesPerRun;
}

bool APGGameModeBase::CanContinueRun() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UPGUserDataSubsystem* UserDataSubsystem = GameInstance ? GameInstance->GetSubsystem<UPGUserDataSubsystem>() : nullptr;
	return IsContinueOffered() && UserDataSubsystem && UserDataSubsystem->GetReceiptBalance() >= GetContinueReceiptCost();
}

int32 APGGameModeBase::GetContinueReceiptCost() const
{
	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	return Settings ? FMath::Max(0, Settings->RunResult.ContinueReceiptCost) : 0;
}

bool APGGameModeBase::RequestContinueRun(bool bFreeByAd)
{
	APGRunnerCharacter* RunnerCharacter = GetPlayableRunnerCharacter();
	UGameInstance* GameInstance = GetGameInstance();
	UPGUserDataSubsystem* UserDataSubsystem = GameInstance ? GameInstance->GetSubsystem<UPGUserDataSubsystem>() : nullptr;
	if (!IsContinueOffered() || !RunnerCharacter || !UserDataSubsystem || (!bFreeByAd && !CanContinueRun()))
	{
		return false;
	}

	const int32 Cost = bFreeByAd ? 0 : GetContinueReceiptCost();
	if (Cost > 0 && !UserDataSubsystem->TrySpendReceipts(Cost, TEXT("RunContinue")))
	{
		return false;
	}
	LastContinuePaidReceipts = Cost;

	++ContinueCount;
	if (bRunRewardsSettled)
	{
		// 실패 시 정산한 런 영수증은 이미 지갑에 들어갔다. 다음 정산은 재개 뒤 새로 번 영수증만 더하고, 최고 점수는 최댓값 갱신이라 다시 넣어도 같다.
		SettledRunReceipts = RunnerCharacter->GetRunReceipts();
	}
	bRunRewardsSettled = false;

	// 플레이어는 이미 상황을 알고 있으므로 Retry와 같은 짧은 카운트다운을 쓴다. 러너 위치는 러너가 OnRunContinueCountdownStarted에서 스스로 바로잡는다.
	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	RunCountdownDuration = Settings ? FMath::Max(0.0f, Settings->HomeFlow.RetryCountdownDuration) : 2.0f;
	RunState = EPGRunState::Countdown;
	bIsContinueCountdown = true;
	OnRunContinueCountdownStarted.Broadcast(RunCountdownDuration);

	if (RunCountdownDuration <= KINDA_SMALL_NUMBER)
	{
		FinishRunCountdown();
		return true;
	}

	GetWorldTimerManager().SetTimer(RunCountdownTimerHandle, this, &APGGameModeBase::FinishRunCountdown, RunCountdownDuration, false);
	return true;
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
	RunDirector = FoundDirector;

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

bool APGGameModeBase::CanDoubleRunReceipts() const
{
	const APGRunnerCharacter* RunnerCharacter = GetPlayableRunnerCharacter();
	return RunState == EPGRunState::Failed && !bRunReceiptsDoubled && RunnerCharacter && RunnerCharacter->GetRunReceipts() > 0;
}

bool APGGameModeBase::RequestDoubleRunReceipts()
{
	UGameInstance* GameInstance = GetGameInstance();
	UPGUserDataSubsystem* UserDataSubsystem = GameInstance ? GameInstance->GetSubsystem<UPGUserDataSubsystem>() : nullptr;
	if (!CanDoubleRunReceipts() || !UserDataSubsystem)
	{
		return false;
	}

	// 실패 시 정산으로 런 영수증은 이미 지갑에 들어갔으므로 같은 양을 한 번 더 넣는 것이 2배다.
	// 이어하기 뒤 정산 기준(SettledRunReceipts)에는 넣지 않아 재개 뒤 정산이 보너스를 다시 세지 않는다.
	bRunReceiptsDoubled = UserDataSubsystem->TryAddReceipts(GetPlayableRunnerCharacter()->GetRunReceipts(), TEXT("RunReceiptsDoubled"));
	return bRunReceiptsDoubled;
}

bool APGGameModeBase::SettleCurrentRunRewards(FName Reason)
{
	if (bRunRewardsSettled)
	{
		return true;
	}

	UWorld* World = GetWorld();
	const APGRunnerCharacter* RunnerCharacter = GetPlayableRunnerCharacter();
	// 이어하기 전에 정산한 영수증은 빼고 그 뒤에 번 만큼만 더한다.
	const int32 RunReceipts = RunnerCharacter ? RunnerCharacter->GetRunReceipts() - SettledRunReceipts : 0;
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

	// 튜토리얼 런은 안내 구간이 쉬워 거리 점수가 부풀려지므로 영수증만 정산하고 최고 점수는 갱신하지 않는다.
	bRunRewardsSettled = UserDataSubsystem->TrySettleRunRewards(RunReceipts, RunScore, Reason, !IsTutorialRun());
	return bRunRewardsSettled;
}

