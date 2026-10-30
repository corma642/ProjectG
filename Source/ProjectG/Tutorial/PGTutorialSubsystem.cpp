// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tutorial/PGTutorialSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Cargo/PGCargoDeliverySubsystem.h"
#include "Cargo/PGCargoStackComponent.h"
#include "Character/PGRunnerCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameModes/PGGameModeBase.h"
#include "Movement/PGRunnerMovementComponent.h"
#include "RunEvents/PGRunEventSubsystem.h"
#include "Track/PGRunDirector.h"
#include "Track/PGTrackSegment.h"
#include "UI/PGScreenTransitionFadeWidget.h"
#include "User/PGUserDataSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGTutorial, Log, All);

namespace
{
	// 균형 단계를 끝내는 중심 입력량(절댓값, 0~1). 작은 떨림이 아니라 실제로 끌었다고 볼 수 있는 값이다.
	constexpr float BalanceInputThreshold = 0.5f;
	// 좌우 이동 단계를 끝내는 중앙 기준 좌우 오프셋(cm). 옆 길(±300cm)에 거의 닿은 값이다.
	constexpr float LateralOffsetThreshold = 200.0f;
	// 구간 진입·완료 판정 주기(초). 구간은 20m 단위라 매 프레임 볼 필요가 없다.
	constexpr float CheckInterval = 0.1f;
	// 되감기 암전이 완전히 덮인 채 머무는 시간(초). 이 사이에 러너를 옮긴다.
	constexpr float RewindHoldSeconds = 0.1f;
}

void UPGTutorialSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	BoundGameMode = InWorld.GetAuthGameMode<APGGameModeBase>();
	if (!BoundGameMode || !BoundGameMode->IsTutorialRun())
	{
		BoundGameMode = nullptr;
		return;
	}

	if (const UPGGameSettings* Settings = GetDefault<UPGGameSettings>())
	{
		CachedSettings = Settings->Tutorial;
	}

	TArray<TSubclassOf<APGTrackSegment>> Sequence;
	BuildChapters(Sequence);
	for (TActorIterator<APGRunDirector> DirectorIt(&InWorld); DirectorIt; ++DirectorIt)
	{
		// 디렉터는 아직 BeginPlay 전이다. 순서를 먼저 넘겨 두면 시작 세그먼트 교체와 첫 생성부터 적용된다.
		DirectorIt->SetForcedSegmentSequence(Sequence, true);
	}

	BoundGameMode->OnRunCountdownStarted.AddDynamic(this, &UPGTutorialSubsystem::HandleRunCountdownStarted);
	BoundGameMode->OnRunRewindStarted.AddDynamic(this, &UPGTutorialSubsystem::HandleRunRewindStarted);
	if (UPGRunEventSubsystem* RunEvents = InWorld.GetSubsystem<UPGRunEventSubsystem>())
	{
		RunEvents->OnRunEventFinished.AddDynamic(this, &UPGTutorialSubsystem::HandleRunEventFinished);
	}
}

void UPGTutorialSubsystem::Deinitialize()
{
	if (BoundGameMode)
	{
		BoundGameMode->OnRunCountdownStarted.RemoveDynamic(this, &UPGTutorialSubsystem::HandleRunCountdownStarted);
		BoundGameMode->OnRunRewindStarted.RemoveDynamic(this, &UPGTutorialSubsystem::HandleRunRewindStarted);
		BoundGameMode = nullptr;
	}
	if (UPGRunEventSubsystem* RunEvents = GetWorld() ? GetWorld()->GetSubsystem<UPGRunEventSubsystem>() : nullptr)
	{
		RunEvents->OnRunEventFinished.RemoveDynamic(this, &UPGTutorialSubsystem::HandleRunEventFinished);
	}
	if (RunnerCharacter)
	{
		RunnerCharacter->OnJumpStarted.RemoveAll(this);
		if (UPGCargoStackComponent* CargoStack = RunnerCharacter->GetCargoStackComponent())
		{
			CargoStack->OnCargoListChanged.RemoveAll(this);
		}
		RunnerCharacter = nullptr;
	}
	RunnerMovement = nullptr;
	if (RewindFadeWidget)
	{
		RewindFadeWidget->OnFadeCovered.RemoveAll(this);
		RewindFadeWidget->RemoveFromParent();
		RewindFadeWidget = nullptr;
	}

	Super::Deinitialize();
}

bool UPGTutorialSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UPGTutorialSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UPGTutorialSubsystem, STATGROUP_Tickables);
}

bool UPGTutorialSubsystem::IsTickable() const
{
	return !IsTemplate() && bGuideActive;
}

EPGTutorialStep UPGTutorialSubsystem::GetCurrentStep() const
{
	return bStepActive && Chapters.IsValidIndex(CurrentChapterIndex) ? Chapters[CurrentChapterIndex].Step : EPGTutorialStep::None;
}

void UPGTutorialSubsystem::Tick(float DeltaTime)
{
	TickAccumulator += DeltaTime;
	if (TickAccumulator < CheckInterval || !BoundGameMode || BoundGameMode->GetRunState() != EPGRunState::Running)
	{
		return;
	}
	TickAccumulator = 0.0f;

	if (!bStepActive)
	{
		const int32 NextChapterIndex = CurrentChapterIndex + 1;
		const APGRunDirector* Director = BoundGameMode->GetRunDirector();
		if (Chapters.IsValidIndex(NextChapterIndex) && Director && Director->GetPlayerProgress() >= Chapters[NextChapterIndex].StartProgress)
		{
			ActivateChapter(NextChapterIndex);
		}
		return;
	}

	// 입력량으로 판정하는 단계만 여기서 본다. 점프·적재물·바람은 delegate로 끝난다.
	switch (GetCurrentStep())
	{
	case EPGTutorialStep::Balance:
		if (RunnerCharacter && FMath::Abs(RunnerCharacter->GetBalanceInput()) >= BalanceInputThreshold)
		{
			CompleteCurrentStep();
		}
		break;
	case EPGTutorialStep::Lateral:
		if (RunnerMovement && FMath::Abs(RunnerMovement->GetBalanceOffset()) >= LateralOffsetThreshold)
		{
			CompleteCurrentStep();
		}
		break;
	default:
		break;
	}
}

void UPGTutorialSubsystem::HandleRunCountdownStarted(float Duration)
{
	if (bGuideActive || bGuideFinished || !BoundGameMode)
	{
		// 안내가 끝난 뒤의 이어하기 카운트다운에서는 다시 켜지 않는다.
		return;
	}

	RunnerCharacter = BoundGameMode->GetPlayableRunnerCharacter();
	if (!RunnerCharacter)
	{
		UE_LOG(LogPGTutorial, Warning, TEXT("Tutorial stage could not find the runner. Guidance is disabled for this run."));
		return;
	}

	bGuideActive = true;
	BoundGameMode->SetTutorialGuideActive(true);
	RunnerMovement = Cast<UPGRunnerMovementComponent>(RunnerCharacter->GetCharacterMovement());
	RunnerCharacter->SetRewindRecordingEnabled(true);
	RunnerCharacter->OnJumpStarted.AddUObject(this, &UPGTutorialSubsystem::HandleRunnerJumpStarted);
	if (UPGCargoStackComponent* CargoStack = RunnerCharacter->GetCargoStackComponent())
	{
		CargoStack->OnCargoListChanged.AddUObject(this, &UPGTutorialSubsystem::HandleCargoListChanged);
	}
	SetAutomaticSystemsEnabled(false);
}

void UPGTutorialSubsystem::HandleRunRewindStarted(FName FailureReason)
{
	UE_LOG(LogPGTutorial, Display, TEXT("Tutorial rewind (%s) during step %s."), *FailureReason.ToString(), *UEnum::GetValueAsString(GetCurrentStep()));

	if (RewindFadeWidget)
	{
		RewindFadeWidget->OnFadeCovered.RemoveAll(this);
		RewindFadeWidget->RemoveFromParent();
		RewindFadeWidget = nullptr;
	}

	UWorld* World = GetWorld();
	APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	UClass* FadeClass = Settings ? Settings->HomeFlow.ScreenTransitionFadeWidgetClass.Get() : nullptr;
	if (PlayerController && FadeClass && FadeClass->IsChildOf(UPGScreenTransitionFadeWidget::StaticClass()))
	{
		RewindFadeWidget = CreateWidget<UPGScreenTransitionFadeWidget>(PlayerController, TSubclassOf<UPGScreenTransitionFadeWidget>(FadeClass));
	}
	if (!RewindFadeWidget)
	{
		// 페이드 위젯이 없으면(미설정·미예열) 암전 없이 바로 되돌린다.
		HandleRewindFadeCovered();
		return;
	}

	RewindFadeWidget->OnFadeCovered.AddUObject(this, &UPGTutorialSubsystem::HandleRewindFadeCovered);
	RewindFadeWidget->AddToViewport(500);
	RewindFadeWidget->PlayCoverThenReveal(CachedSettings.RewindFadeDuration, RewindHoldSeconds, CachedSettings.RewindFadeDuration, 1.0f);
}

void UPGTutorialSubsystem::HandleRewindFadeCovered()
{
	// 위젯은 걷히는 페이드까지 스스로 끝내고 사라진다. 여기서는 참조만 놓는다.
	RewindFadeWidget = nullptr;
	if (!BoundGameMode)
	{
		return;
	}

	BoundGameMode->FinishTutorialRewind();

	// 실패 때 지워진 적재물 드롭·바람은 안내 중인 단계면 다시 발동해 같은 상황을 다시 준다. 되감기로 적재물이 복원됐으면 드롭은 다시 하지 않는다.
	if (bStepActive)
	{
		const EPGTutorialStep Step = GetCurrentStep();
		const UPGCargoStackComponent* CargoStack = RunnerCharacter ? RunnerCharacter->GetCargoStackComponent() : nullptr;
		if (Step == EPGTutorialStep::Wind || (Step == EPGTutorialStep::Cargo && CargoStack && CargoStack->GetReservedCargoCount() == 0))
		{
			TriggerChapterAction(Step);
		}
	}
}

void UPGTutorialSubsystem::HandleRunEventFinished(EPGRunEventType EventType)
{
	if (EventType == EPGRunEventType::Wind && GetCurrentStep() == EPGTutorialStep::Wind)
	{
		CompleteCurrentStep();
	}
}

void UPGTutorialSubsystem::HandleRunnerJumpStarted()
{
	if (GetCurrentStep() == EPGTutorialStep::Jump)
	{
		CompleteCurrentStep();
	}
}

void UPGTutorialSubsystem::HandleCargoListChanged()
{
	const UPGCargoStackComponent* CargoStack = RunnerCharacter ? RunnerCharacter->GetCargoStackComponent() : nullptr;
	if (GetCurrentStep() == EPGTutorialStep::Cargo && CargoStack && CargoStack->GetCurrentCargoCount() > 0)
	{
		CompleteCurrentStep();
	}
}

void UPGTutorialSubsystem::BuildChapters(TArray<TSubclassOf<APGTrackSegment>>& OutSequence)
{
	Chapters.Reset();
	OutSequence.Reset();

	// 레벨 배치 시작 세그먼트를 대신하므로 첫 구간은 런 원점(진행 거리 0)에서 시작한다.
	float Progress = 0.0f;
	for (const FPGTutorialChapter& Chapter : CachedSettings.Chapters)
	{
		FPGTutorialChapterRuntime& Runtime = Chapters.AddDefaulted_GetRef();
		Runtime.Step = Chapter.Step;
		Runtime.StartProgress = Progress;
		for (const TSoftClassPtr<APGTrackSegment>& SegmentClassPtr : Chapter.Segments)
		{
			// 구간 세그먼트는 맵의 세그먼트 세트가 이미 들고 있어 동기 로드가 디스크 읽기를 만들지 않는다.
			UClass* SegmentClass = SegmentClassPtr.LoadSynchronous();
			const APGTrackSegment* SegmentDefault = SegmentClass ? SegmentClass->GetDefaultObject<APGTrackSegment>() : nullptr;
			if (!SegmentDefault)
			{
				UE_LOG(LogPGTutorial, Warning, TEXT("Tutorial chapter %s has an invalid segment class '%s'."), *UEnum::GetValueAsString(Chapter.Step), *SegmentClassPtr.ToString());
				continue;
			}
			OutSequence.Add(SegmentClass);
			Progress += SegmentDefault->GetSegmentLength();
		}
	}
	UE_LOG(LogPGTutorial, Display, TEXT("Tutorial chapters: %d, forced segments: %d (%.0f cm)."), Chapters.Num(), OutSequence.Num(), Progress);
}

void UPGTutorialSubsystem::ActivateChapter(int32 ChapterIndex)
{
	CurrentChapterIndex = ChapterIndex;
	bStepActive = true;
	const EPGTutorialStep Step = Chapters[ChapterIndex].Step;
	UE_LOG(LogPGTutorial, Display, TEXT("Tutorial step %d/%d started: %s"), ChapterIndex + 1, Chapters.Num(), *UEnum::GetValueAsString(Step));
	OnTutorialStepChanged.Broadcast(Step);
	TriggerChapterAction(Step);
}

void UPGTutorialSubsystem::CompleteCurrentStep()
{
	if (!bStepActive)
	{
		return;
	}

	bStepActive = false;
	UE_LOG(LogPGTutorial, Display, TEXT("Tutorial step %d/%d completed."), CurrentChapterIndex + 1, Chapters.Num());
	OnTutorialStepChanged.Broadcast(EPGTutorialStep::None);
	if (CurrentChapterIndex >= Chapters.Num() - 1)
	{
		FinishGuide();
	}
}

void UPGTutorialSubsystem::TriggerChapterAction(EPGTutorialStep Step)
{
	UWorld* World = GetWorld();
	switch (Step)
	{
	case EPGTutorialStep::Cargo:
		// 안내 구간에서는 자동 드롭이 꺼져 있으므로 받을 적재물을 여기서 하나 떨어뜨린다.
		if (UPGCargoDeliverySubsystem* CargoDelivery = World ? World->GetSubsystem<UPGCargoDeliverySubsystem>() : nullptr)
		{
			CargoDelivery->RequestImmediateDrop();
		}
		break;
	case EPGTutorialStep::Wind:
		// 바람은 트랙 중앙 쪽으로 분다. 옆 길(±300cm)에 선 러너를 바깥으로 밀면 버틸 여유가 75cm뿐이라 안내 중에 떨어진다. 방향은 전조 배지가 알려 주고 전조 시간 뒤 발동한다.
		if (UPGRunEventSubsystem* RunEvents = World ? World->GetSubsystem<UPGRunEventSubsystem>() : nullptr)
		{
			const float Direction = RunnerMovement && RunnerMovement->GetBalanceOffset() > 0.0f ? -1.0f : 1.0f;
			RunEvents->ScheduleEventNow(EPGRunEventType::Wind, Direction);
		}
		break;
	default:
		break;
	}
}

void UPGTutorialSubsystem::FinishGuide()
{
	bGuideActive = false;
	bGuideFinished = true;
	if (BoundGameMode)
	{
		BoundGameMode->SetTutorialGuideActive(false);
	}
	if (RunnerCharacter)
	{
		RunnerCharacter->SetRewindRecordingEnabled(false);
	}
	SetAutomaticSystemsEnabled(true);

	// 첫 완료에만 보상한다. 저장이 실패하면 보상 없는 완료로 보여 주고 다음 완료에서 다시 시도한다.
	int32 GrantedReceipts = 0;
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (UPGUserDataSubsystem* UserData = GameInstance ? GameInstance->GetSubsystem<UPGUserDataSubsystem>() : nullptr)
	{
		if (!UserData->IsTutorialCompleted() && UserData->TryGrantTutorialCompletion(CachedSettings.CompletionRewardReceipts, TEXT("TutorialReward")))
		{
			GrantedReceipts = CachedSettings.CompletionRewardReceipts;
		}
	}
	UE_LOG(LogPGTutorial, Display, TEXT("Tutorial guide finished (reward %d receipts)."), GrantedReceipts);
	OnTutorialCompleted.Broadcast(GrantedReceipts);
}

void UPGTutorialSubsystem::SetAutomaticSystemsEnabled(bool bEnabled)
{
	UWorld* World = GetWorld();
	if (UPGRunEventSubsystem* RunEvents = World ? World->GetSubsystem<UPGRunEventSubsystem>() : nullptr)
	{
		RunEvents->SetAutoSchedulingEnabled(bEnabled);
	}
	if (UPGCargoDeliverySubsystem* CargoDelivery = World ? World->GetSubsystem<UPGCargoDeliverySubsystem>() : nullptr)
	{
		CargoDelivery->SetAutoDropEnabled(bEnabled);
	}
}
