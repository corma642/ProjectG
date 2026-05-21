// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RunEvents/PGRunEventSet.h"
#include "Settings/PGGameSettings.h"
#include "Subsystems/WorldSubsystem.h"
#include "PGRunEventSubsystem.generated.h"

class APGGameModeBase;
class APGMovingRunEventHazardActor;
class APGRunnerCharacter;
class UPGRunnerEnvironmentComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPGRunEventPreviewStartedSignature, EPGRunEventType, EventType, float, TimeUntilTrigger, float, Direction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPGRunEventTriggeredSignature, EPGRunEventType, EventType, float, Duration, float, Direction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPGRunEventFinishedSignature, EPGRunEventType, EventType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPGRunEventCanceledSignature, EPGRunEventType, EventType);

/**
 * 러닝 시간 기반으로 바람/눈바람 같은 전역 위협 이벤트를 예약하고 발동하는 월드 서브시스템입니다.
 * 트랙 생성이나 배달 적재물 드롭과 분리해, 환경 이벤트의 예고/발동/종료 상태만 관리합니다.
 */
UCLASS(BlueprintType)
class PROJECTG_API UPGRunEventSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

	// 현재 러닝 시간 기준 이벤트 난이도 알파입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run Event")
	float GetDifficultyAlpha() const;

	// 현재 예약, 예고, 발동 중인 이벤트 종류입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run Event")
	EPGRunEventType GetPendingEventType() const { return PendingEventType; }

	// 현재 이벤트 예약/예고/발동 상태입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run Event")
	EPGRunEventState GetPendingEventState() const { return PendingEventState; }

	// 예약된 이벤트가 발동하기까지 남은 시간입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run Event")
	float GetTimeUntilPendingEvent() const { return PendingEventType == EPGRunEventType::None ? 0.0f : FMath::Max(0.0f, PendingEventFireTime - RunElapsedTime); }

	// 이벤트 예고가 시작되면 로컬 연출 계층에 전달합니다.
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Run Event")
	FPGRunEventPreviewStartedSignature OnRunEventPreviewStarted;

	// 이벤트 효과가 실제 적용되는 순간 로컬 연출 계층과 환경 컴포넌트에 전달합니다.
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Run Event")
	FPGRunEventTriggeredSignature OnRunEventTriggered;

	// 지속형 이벤트가 자연 종료되면 로컬 연출 계층에 전달합니다.
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Run Event")
	FPGRunEventFinishedSignature OnRunEventFinished;

	// 런 실패나 상태 정리로 이벤트가 강제 종료될 때 전달합니다.
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Run Event")
	FPGRunEventCanceledSignature OnRunEventCanceled;

private:
	UFUNCTION()
	void HandleRunStarted();

	UFUNCTION()
	void HandleRunFailed(FName FailureReason);

	void LoadSettings();
	void InitializeRunnerReferences();
	void BindGameMode();
	void ProcessRunEventTick(float DeltaSeconds);
	void ScheduleNextEvent();
	bool SelectEventRule(FPGRunEventRule& OutRule);
	const TArray<FPGRunEventRule>& GetEffectiveEventRules() const;
	bool CanUseRule(const FPGRunEventRule& Rule) const;
	void TriggerPendingEvent();
	void ClearPendingEvent(bool bBroadcastCancel);
	void StartActiveEvent(EPGRunEventType EventType, float Duration);
	void FinishActiveEvent();
	float ResolveMovingHazardLaneSign(const FPGRunEventRule& Rule);
	bool SpawnMovingHazard();
	void DestroyActiveMovingHazards();

	UPROPERTY(Transient)
	TObjectPtr<APGRunnerCharacter> RunnerCharacter;

	UPROPERTY(Transient)
	TObjectPtr<UPGRunnerEnvironmentComponent> RunnerEnvironmentComponent;

	UPROPERTY(Transient)
	mutable TObjectPtr<UPGRunEventSet> LoadedEventSet;

	UPROPERTY(Transient)
	TObjectPtr<APGGameModeBase> BoundGameMode;

	FPGRunEventSettings CachedRunEventSettings;
	FPGMovingHazardSettings CachedMovingHazardSettings;
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
	mutable bool bWarnedMissingEventRules = false;
	bool bWarnedMissingMovingHazardClass = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<APGMovingRunEventHazardActor>> ActiveMovingHazards;
};
