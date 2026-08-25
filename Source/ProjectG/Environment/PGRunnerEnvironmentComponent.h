// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Settings/PGGameSettings.h"
#include "PGRunnerEnvironmentComponent.generated.h"

/**
 * 바람과 눈바람 RunEvent가 러너 조작에 주는 효과를 관리하는 컴포넌트입니다.
 * 적재물 드롭은 CargoDeliverySubsystem 책임이므로 이 컴포넌트는 입력/적재물 반응 보정만 담당합니다.
 */
UCLASS(ClassGroup = (ProjectG), meta = (BlueprintSpawnableComponent))
class PROJECTG_API UPGRunnerEnvironmentComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	/** 프레임 Tick 없이 러너가 호출하는 명시적 갱신 컴포넌트로 생성합니다. */
	UPGRunnerEnvironmentComponent();

	/** Project Settings의 Wind/Snowstorm 효과 설정을 복사하고 모든 효과 시간을 reset합니다. */
	void InitializeEnvironment(const FPGEnvironmentSettings& InEnvironmentSettings);

	/** 런 진행 상태를 갱신합니다. false로 바뀌면 활성 환경 효과를 즉시 정리합니다. */
	void SetRunActive(bool bInRunActive);

	/** 활성 효과를 DeltaSeconds만큼 갱신하고 -1~1 최종 중심 입력값을 반환합니다. */
	float UpdateEnvironment(float DeltaSeconds, float RawBalanceInput);

	/** 초 단위 Duration 동안 바람을 적용합니다. Direction은 부호에 따라 -1 또는 1로 정규화됩니다. */
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Run Event Effects")
	void ApplyWind(float Direction, float Strength, float Duration);

	/** 초 단위 Duration 동안 Strength 입력 배율을 적용합니다. 원본 입력이 0이면 추가 이동은 만들지 않습니다. */
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Run Event Effects")
	void ApplySnowstorm(float Duration, float Strength);

	/** 활성 바람과 눈바람을 모두 해제하고 보정 출력값을 기본 상태로 복구합니다. */
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Run Event Effects")
	void ClearEnvironmentEffects();

	/** 현재 환경 효과가 반영된 최종 중심 입력값을 -1~1 범위로 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run Event Effects")
	float GetProcessedBalanceInput() const { return ProcessedBalanceInput; }

	/** 적재물 물리 반응용 중심값을 반환합니다. 기본 흔들림은 제외하고 입력·이벤트 보정만 포함합니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run Event Effects")
	float GetProcessedCargoBalanceInput() const { return ProcessedCargoBalanceInput; }

	/** 눈바람이 현재 플레이어 입력 과민 효과를 적용 중인지 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run Event Effects")
	bool IsSnowstormActive() const { return SnowstormRemainingTime > 0.0f; }

	/** 바람 또는 눈바람 효과가 아직 적용 중인지 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run Event Effects")
	bool HasActiveEnvironmentEffect() const;

private:
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
