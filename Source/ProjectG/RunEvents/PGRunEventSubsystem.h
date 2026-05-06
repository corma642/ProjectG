// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RunEvents/PGRunEventSet.h"
#include "Settings/PGGameSettings.h"
#include "Subsystems/WorldSubsystem.h"
#include "PGRunEventSubsystem.generated.h"

class APGGameModeBase;
class APGRunnerCharacter;
class UPGCargoStackComponent;
class UPGRunnerEnvironmentComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPGRunEventPreviewStartedSignature, EPGRunEventType, EventType, float, TimeUntilTrigger, float, Direction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPGRunEventTriggeredSignature, EPGRunEventType, EventType, float, Duration, float, Direction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPGRunEventFinishedSignature, EPGRunEventType, EventType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPGRunEventCanceledSignature, EPGRunEventType, EventType);

/**
 * 러닝 시간 기반으로 전역 환경/적재 이벤트를 예약하고 발동하는 월드 서브시스템입니다.
 * 레벨 배치가 필요 없는 이벤트 총괄자이며, 트랙 세그먼트 생성 책임과 분리됩니다.
 */
UCLASS(BlueprintType)
class PROJECTG_API UPGRunEventSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

	// 현재 시간 기반 이벤트 난이도입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run Event")
	float GetDifficultyAlpha() const;

	// 현재 예약 또는 예고 중인 이벤트 종류입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run Event")
	EPGRunEventType GetPendingEventType() const { return PendingEventType; }

	// 현재 이벤트 예약/예고/발동 상태입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run Event")
	EPGRunEventState GetPendingEventState() const { return PendingEventState; }

	// 예약된 이벤트가 발동되기까지 남은 시간입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run Event")
	float GetTimeUntilPendingEvent() const { return PendingEventType == EPGRunEventType::None ? 0.0f : FMath::Max(0.0f, PendingEventFireTime - RunElapsedTime); }

	// 이벤트 전조가 시작될 때 로컬 연출 계층에 전달됩니다.
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Run Event")
	FPGRunEventPreviewStartedSignature OnRunEventPreviewStarted;

	// 이벤트 실제 효과가 적용될 때 로컬 연출 계층에 전달됩니다.
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Run Event")
	FPGRunEventTriggeredSignature OnRunEventTriggered;

	// 지속형 이벤트가 자연 종료될 때 로컬 연출 계층에 전달됩니다.
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Run Event")
	FPGRunEventFinishedSignature OnRunEventFinished;

	// 런 실패나 월드 종료처럼 이벤트가 강제로 정리될 때 로컬 연출 계층에 전달됩니다.
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Run Event")
	FPGRunEventCanceledSignature OnRunEventCanceled;

private:
	UFUNCTION()
	void HandleRunStarted();

	UFUNCTION()
	void HandleRunFailed(FName FailureReason);

	void LoadSettings();
	void InitializeDefaultRules();
	void InitializeRunnerReferences();
	void BindGameMode();
	void ProcessRunEventTick(float DeltaSeconds);
	void ScheduleNextEvent();
	bool SelectEventRule(FPGRunEventRule& OutRule);
	const TArray<FPGRunEventRule>& GetEffectiveEventRules() const;
	bool CanUseRule(const FPGRunEventRule& Rule) const;
	bool HasCargoCapacity() const;
	void TriggerPendingEvent();
	void ClearPendingEvent(bool bBroadcastCancel);
	void StartActiveEvent(EPGRunEventType EventType, float Duration);
	void FinishActiveEvent();
	void DrawRunEventDebug() const;

	UPROPERTY(Transient)
	TObjectPtr<APGRunnerCharacter> RunnerCharacter;

	UPROPERTY(Transient)
	TObjectPtr<UPGRunnerEnvironmentComponent> RunnerEnvironmentComponent;

	UPROPERTY(Transient)
	TObjectPtr<UPGCargoStackComponent> CargoStackComponent;

	UPROPERTY(Transient)
	TObjectPtr<UPGRunEventSet> LoadedEventSet;

	UPROPERTY(Transient)
	TObjectPtr<APGGameModeBase> BoundGameMode;

	FPGRunEventSettings CachedRunEventSettings;
	TArray<FPGRunEventRule> DefaultRules;
	FRandomStream RandomStream;
	TMap<EPGRunEventType, float> LastTriggeredTimeByEvent;
	FPGRunEventRule PendingEventRule;
	EPGRunEventType PendingEventType = EPGRunEventType::None;
	EPGRunEventState PendingEventState = EPGRunEventState::Inactive;
	EPGRunEventType ActiveEventType = EPGRunEventType::None;
	float ActiveEventEndTime = 0.0f;
	float PendingEventDirection = 0.0f;
	float PendingEventFireTime = 0.0f;
	float NextScheduleRetryTime = 0.0f;
	float RunElapsedTime = 0.0f;
	float TickAccumulator = 0.0f;
	bool bIsRunActive = false;
	bool bIsBoundToGameMode = false;
};
