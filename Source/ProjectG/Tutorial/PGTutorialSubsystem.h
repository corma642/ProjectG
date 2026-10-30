// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RunEvents/PGRunEventSet.h"
#include "Settings/PGGameSettings.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tutorial/PGTutorialTypes.h"
#include "PGTutorialSubsystem.generated.h"

class APGGameModeBase;
class APGRunnerCharacter;
class APGTrackSegment;
class UPGRunnerMovementComponent;
class UPGScreenTransitionFadeWidget;

/** 안내 단계가 바뀔 때 새 단계를 전달합니다. None은 단계 사이(안내를 숨길 때)입니다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPGTutorialStepChangedSignature, EPGTutorialStep, Step);
/** 마지막 단계가 끝났을 때 이번에 지급한 보상 영수증(이미 완료했던 플레이면 0)을 전달합니다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPGTutorialCompletedSignature, int32, RewardReceipts);

/**
 * 튜토리얼 스테이지(StartFlow=Tutorial로 열린 레벨)의 진행을 맡는 월드 서브시스템입니다.
 * 트랙 디렉터에 Tutorial.Chapters의 세그먼트 순서를 넘기고, 러너가 각 구간에 들어서면 단계 안내를 띄우고 조작 성공을 판정합니다.
 * 안내 구간 동안 자동 런 이벤트·적재물 드롭을 끄고(적재물·바람 단계는 직접 1회 발동), 실패는 GameMode의 되감기 흐름에 맞춰 암전 뒤 몇 초 전으로 되돌립니다.
 * 안내 UI는 UPGRunHUDComponent가 delegate를 받아 그립니다. 튜토리얼 레벨이 아니면 아무 일도 하지 않습니다.
 */
UCLASS()
class PROJECTG_API UPGTutorialSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 튜토리얼 레벨이면 GameMode·이벤트 delegate를 연결하고 디렉터에 구간 세그먼트 순서를 넘깁니다. */
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** 연결한 delegate와 되감기 페이드 위젯을 월드 종료 전에 정리합니다. */
	virtual void Deinitialize() override;

	/** Game과 PIE 월드에서만 만듭니다. */
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

	/** 러너의 트랙 진행으로 다음 구간 안내를 띄우고, 입력량으로 판정하는 단계(균형·좌우 이동)의 완료를 봅니다. */
	virtual void Tick(float DeltaTime) override;

	/** Unreal Tick 통계에 TutorialSubsystem 식별자를 제공합니다. */
	virtual TStatId GetStatId() const override;

	/** 안내 구간이 진행 중일 때만 Tick합니다. */
	virtual bool IsTickable() const override;

	/** 지금 안내 중인 단계입니다. 단계 사이나 안내 구간 밖이면 None입니다. */
	EPGTutorialStep GetCurrentStep() const;

	/** 단계 안내가 바뀔 때(None이면 숨김) HUD에 전달합니다. */
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Tutorial")
	FPGTutorialStepChangedSignature OnTutorialStepChanged;

	/** 마지막 단계가 끝나 보상을 처리한 뒤 지급한 영수증 수와 함께 발생합니다. */
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Tutorial")
	FPGTutorialCompletedSignature OnTutorialCompleted;

private:
	struct FPGTutorialChapterRuntime
	{
		EPGTutorialStep Step = EPGTutorialStep::None;
		// 구간 첫 세그먼트의 트랙 진행 거리(cm). 러너가 이 거리를 넘고 앞 단계가 끝나면 안내가 뜬다.
		float StartProgress = 0.0f;
	};

	UFUNCTION()
	void HandleRunCountdownStarted(float Duration);

	UFUNCTION()
	void HandleRunRewindStarted(FName FailureReason);

	UFUNCTION()
	void HandleRunEventFinished(EPGRunEventType EventType);

	void HandleRunnerJumpStarted();
	void HandleCargoListChanged();
	void HandleRewindFadeCovered();
	void BuildChapters(TArray<TSubclassOf<APGTrackSegment>>& OutSequence);
	void ActivateChapter(int32 ChapterIndex);
	void CompleteCurrentStep();
	void TriggerChapterAction(EPGTutorialStep Step);
	void FinishGuide();
	void SetAutomaticSystemsEnabled(bool bEnabled);

	UPROPERTY(Transient)
	TObjectPtr<APGGameModeBase> BoundGameMode;

	UPROPERTY(Transient)
	TObjectPtr<APGRunnerCharacter> RunnerCharacter;

	UPROPERTY(Transient)
	TObjectPtr<UPGRunnerMovementComponent> RunnerMovement;

	// 되감기 암전. 덮인 순간 러너를 되돌리고, 걷히면 위젯이 스스로 사라진다.
	UPROPERTY(Transient)
	TObjectPtr<UPGScreenTransitionFadeWidget> RewindFadeWidget;

	TArray<FPGTutorialChapterRuntime> Chapters;
	FPGTutorialSettings CachedSettings;
	int32 CurrentChapterIndex = INDEX_NONE;
	// 현재 구간의 안내가 떠 있고 완료를 기다리는 중인지. 완료 뒤 다음 구간에 들어서기 전까지는 false다.
	bool bStepActive = false;
	// 안내 구간 진행 중(실패가 되감기로 처리되는 동안)인지.
	bool bGuideActive = false;
	// 마지막 단계까지 끝났는지. 이후 카운트다운(이어하기)에서 다시 켜지지 않게 한다.
	bool bGuideFinished = false;
	float TickAccumulator = 0.0f;
};
