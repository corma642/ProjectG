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

private:
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
	void ClearPendingEvent();
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
	float PendingEventFireTime = 0.0f;
	float NextScheduleRetryTime = 0.0f;
	float RunElapsedTime = 0.0f;
	float TickAccumulator = 0.0f;
	bool bIsRunActive = true;
	bool bIsBoundToRunFailed = false;
};
