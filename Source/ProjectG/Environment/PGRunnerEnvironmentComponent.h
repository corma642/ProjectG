// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Settings/PGGameSettings.h"
#include "PGRunnerEnvironmentComponent.generated.h"

/**
 * 바람과 눈바람처럼 러너 조작에 영향을 주는 환경 효과를 관리하는 컴포넌트입니다.
 * 적재물 드롭은 CargoDeliverySubsystem 책임이므로 이 컴포넌트는 환경 위협만 담당합니다.
 */
UCLASS(ClassGroup = (ProjectG), meta = (BlueprintSpawnableComponent))
class PROJECTG_API UPGRunnerEnvironmentComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UPGRunnerEnvironmentComponent();

	// Project Settings 환경 설정을 받아 초기화합니다.
	void InitializeEnvironment(const FPGEnvironmentSettings& InEnvironmentSettings);

	// 런 진행 상태를 갱신합니다. 실패 뒤에는 환경 효과를 모두 정리합니다.
	void SetRunActive(bool bInRunActive);

	// 활성 환경 효과를 갱신하고 최종 중심 입력값을 반환합니다.
	float UpdateEnvironment(float DeltaSeconds, float RawBalanceInput);

	// 지정 방향의 바람을 적용합니다. Direction은 -1 또는 1 방향으로 정규화됩니다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Environment")
	void ApplyWind(float Direction, float Strength, float Duration);

	// 눈바람으로 플레이어 기울임 입력을 과민하게 증폭합니다. 입력이 없으면 추가 효과는 없습니다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Environment")
	void ApplySnowstorm(float Duration, float Strength);

	// 활성 환경 효과를 모두 해제합니다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Environment")
	void ClearEnvironmentEffects();

	// 현재 환경 효과가 반영된 최종 중심 입력값입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Environment")
	float GetProcessedBalanceInput() const { return ProcessedBalanceInput; }

	// 적재물 물리 반응에 사용할 플레이어 입력 기반 중심값입니다. 기본 흔들림은 포함하지 않습니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Environment")
	float GetProcessedCargoBalanceInput() const { return ProcessedCargoBalanceInput; }

	// 눈바람이 현재 플레이어 입력 과민 효과를 적용 중인지 반환합니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Environment")
	bool IsSnowstormActive() const { return SnowstormRemainingTime > 0.0f; }

	// 바람 또는 눈바람 효과가 아직 적용 중인지 여부입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Environment")
	bool HasActiveEnvironmentEffect() const;

private:
	void DrawEnvironmentDebug() const;

	FPGEnvironmentSettings CachedEnvironmentSettings;
	bool bIsRunActive = true;
	float WindRemainingTime = 0.0f;
	float WindDirection = 0.0f;
	float WindStrength = 0.0f;
	float SnowstormRemainingTime = 0.0f;
	float SnowstormStrength = 1.0f;
	float ProcessedBalanceInput = 0.0f;
	float ProcessedCargoBalanceInput = 0.0f;
};
