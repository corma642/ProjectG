// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Settings/PGGameSettings.h"
#include "Subsystems/WorldSubsystem.h"
#include "PGCargoDeliverySubsystem.generated.h"

class APGGameModeBase;
class APGRunnerCharacter;
class UPGCargoDropSet;
class UPGCargoStackComponent;
class UPGCargoTypeDefinition;

/**
 * 런 시간에 따라 배달 적재물 드롭을 예약하는 월드 서브시스템입니다.
 * 바람/눈바람 같은 돌발 RunEvent와 분리해, 배달 적재물 누적과 완료 보상 흐름만 담당합니다.
 */
UCLASS()
class PROJECTG_API UPGCargoDeliverySubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 설정을 캐시하고 월드 시작 이후 연결할 수 있도록 런타임 상태를 초기화합니다. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** GameMode delegate와 현재 맵/러너 참조를 월드 종료 전에 해제합니다. */
	virtual void Deinitialize() override;

	/** 플레이 가능한 월드가 준비된 뒤 GameMode와 러너의 CargoStack을 연결합니다. */
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Game과 PIE 월드에서만 배달 스케줄러를 생성하도록 제한합니다. */
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

	/** 활성 런에서만 누적 시간을 갱신하고 예정된 적재물 드롭을 처리합니다. */
	virtual void Tick(float DeltaTime) override;

	/** Unreal Tick 통계에 CargoDeliverySubsystem 식별자를 제공합니다. */
	virtual TStatId GetStatId() const override;

	/** Running 중이거나 GameMode 바인딩을 아직 못 한 동안 Tick을 허용합니다. 미바인딩 Tick은 바인딩 재시도용입니다. */
	virtual bool IsTickable() const override;

	/** 현재 런 시간이 설정된 난이도 상승 시간에서 차지하는 0~1 비율을 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Cargo")
	float GetDifficultyAlpha() const;

private:
	UFUNCTION()
	void HandleRunStarted();

	UFUNCTION()
	void HandleRunFailed(FName FailureReason);

	void LoadSettings();
	bool ResolveCurrentMapDropSet();
	void BindGameMode();
	void InitializeRunnerReferences();
	void ProcessCargoDeliveryTick(float DeltaSeconds);
	bool TryStartCargoDrop();
	UPGCargoTypeDefinition* SelectCargoType();
	void ScheduleNextDrop(float DelaySeconds);
	float CalculateCurrentDropInterval() const;

	UPROPERTY(Transient)
	TObjectPtr<APGGameModeBase> BoundGameMode;

	UPROPERTY(Transient)
	TObjectPtr<APGRunnerCharacter> RunnerCharacter;

	UPROPERTY(Transient)
	TObjectPtr<UPGCargoStackComponent> CargoStackComponent;

	UPROPERTY(Transient)
	TObjectPtr<UPGCargoDropSet> LoadedDropSet;

	FPGCargoDeliverySettings CachedDeliverySettings;
	FPGRunDifficultySettings CachedDifficultySettings;
	FRandomStream RandomStream;
	float RunElapsedTime = 0.0f;
	float NextDropTime = 0.0f;
	float TickAccumulator = 0.0f;
	float CurrentMapCargoLossWorldZ = 0.0f;
	bool bIsRunActive = false;
	bool bIsBoundToGameMode = false;
	bool bHasCurrentMapCargoLossWorldZ = false;
	bool bWarnedMissingCargoStack = false;
	bool bWarnedMissingDropSet = false;
	bool bWarnedMissingCargoType = false;
	bool bWarnedInvalidCargoMesh = false;
};
