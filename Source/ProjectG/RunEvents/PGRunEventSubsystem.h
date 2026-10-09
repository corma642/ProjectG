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

/** 이벤트 발동 전 준비 시간을 UI/VFX/SFX 계층에 전달하는 delegate입니다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPGRunEventPreviewStartedSignature, EPGRunEventType, EventType, float, TimeUntilTrigger, float, Direction);

/** 이벤트 효과가 실제 적용되는 순간 종류, 지속 시간과 방향을 전달하는 delegate입니다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPGRunEventTriggeredSignature, EPGRunEventType, EventType, float, Duration, float, Direction);

/** 지속 시간이 끝나 자연 종료된 이벤트 종류를 전달하는 delegate입니다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPGRunEventFinishedSignature, EPGRunEventType, EventType);

/** 실패 또는 월드 정리로 강제 취소된 이벤트 종류를 전달하는 delegate입니다. */
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
	/** 이벤트 설정과 난수 상태를 준비하고 월드 시작 delegate 연결을 기다립니다. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** 활성 이벤트를 취소하고 GameMode 및 환경 컴포넌트 연결을 해제합니다. */
	virtual void Deinitialize() override;

	/** GameMode와 러너 환경 컴포넌트를 월드 Actor 생성 이후 연결합니다. */
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Game과 PIE 월드에서만 이벤트 스케줄러를 생성하도록 제한합니다. */
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

	/** 활성 런의 예약·예고·발동·종료 상태를 시간 순서대로 처리합니다. */
	virtual void Tick(float DeltaTime) override;

	/** Unreal Tick 통계에 RunEventSubsystem 식별자를 제공합니다. */
	virtual TStatId GetStatId() const override;

	/** 런이 활성화되어 실제 상태 전환이 필요할 때만 Tick을 허용합니다. */
	virtual bool IsTickable() const override;

	/** 현재 러닝 시간이 난이도 램프에서 차지하는 0~1 알파를 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run Event")
	float GetDifficultyAlpha() const;

	/** 현재 예약·예고·발동 중인 이벤트 종류를 반환하며 없으면 None입니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run Event")
	EPGRunEventType GetPendingEventType() const { return PendingEventType; }

	/** 현재 이벤트 예약·예고·발동 상태를 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run Event")
	EPGRunEventState GetPendingEventState() const { return PendingEventState; }

	/** 예약된 이벤트 발동까지 남은 시간을 초 단위로 반환하며, 예약이 없으면 0입니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run Event")
	float GetTimeUntilPendingEvent() const { return PendingEventType == EPGRunEventType::None ? 0.0f : FMath::Max(0.0f, PendingEventFireTime - RunElapsedTime); }

#if !UE_BUILD_SHIPPING
	/**
	 * 개발 빌드 전용: 쇼케이스 녹화에서 지정한 종류의 이벤트를 지금 전조부터 시작하도록 예약합니다.
	 * 세기와 지속 시간은 이벤트 세트에 등록된 그 종류의 첫 규칙을 따릅니다. 런 중이 아니거나 규칙이 없으면 false를 반환합니다.
	 */
	bool DebugScheduleEvent(EPGRunEventType EventType, float Direction);
#endif

	/** 이벤트 예고가 시작되면 종류, 발동까지 남은 초와 -1~1 방향을 연출 계층에 전달합니다. */
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Run Event")
	FPGRunEventPreviewStartedSignature OnRunEventPreviewStarted;

	/** 이벤트 효과가 실제 적용되는 순간 종류, 초 단위 지속 시간과 방향을 전달합니다. */
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Run Event")
	FPGRunEventTriggeredSignature OnRunEventTriggered;

	/** 지속형 이벤트가 자연 종료되면 로컬 연출 계층에 전달합니다. */
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Run Event")
	FPGRunEventFinishedSignature OnRunEventFinished;

	/** 런 실패 또는 월드 정리로 이벤트가 강제 종료될 때 전달합니다. */
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
	FPGRunDifficultySettings CachedDifficultySettings;
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
