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
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

	UFUNCTION(BlueprintPure, Category = "ProjectG | Cargo")
	float GetDifficultyAlpha() const;

private:
	UFUNCTION()
	void HandleRunStarted();

	UFUNCTION()
	void HandleRunFailed(FName FailureReason);

	void LoadSettings();
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
	FRandomStream RandomStream;
	float RunElapsedTime = 0.0f;
	float NextDropTime = 0.0f;
	float TickAccumulator = 0.0f;
	bool bIsRunActive = false;
	bool bIsBoundToGameMode = false;
	bool bWarnedMissingCargoStack = false;
	bool bWarnedMissingCargoType = false;
	bool bWarnedInvalidCargoMesh = false;
};
