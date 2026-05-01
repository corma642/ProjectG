// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Settings/PGGameSettings.h"
#include "PGRunnerEnvironmentComponent.generated.h"

class UPGCargoStackComponent;

/**
 * 바람, 눈바람, 적재 추가 요청 같은 러너 주변 환경 효과를 관리하는 컴포넌트입니다.
 * 환경 효과는 캐릭터 중심 입력을 보정하거나 CargoStackComponent에 적재물 추가 요청을 전달합니다.
 */
UCLASS(ClassGroup = (ProjectG), meta = (BlueprintSpawnableComponent))
class PROJECTG_API UPGRunnerEnvironmentComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UPGRunnerEnvironmentComponent();

	// Project Settings 환경 설정과 적재 컴포넌트 참조를 받아 초기화합니다.
	void InitializeEnvironment(const FPGEnvironmentSettings& InEnvironmentSettings, UPGCargoStackComponent* InCargoStackComponent);

	// 런 진행 상태를 갱신합니다. 실패 후에는 환경 효과와 적재 추가 요청을 무시합니다.
	void SetRunActive(bool bInRunActive);

	// 활성화된 환경 효과를 갱신하고 최종 중심 입력값을 반환합니다.
	float UpdateEnvironment(float DeltaSeconds, float RawBalanceInput);

	// 지정 방향의 바람을 적용합니다. Direction은 -1 또는 1 방향으로 정규화됩니다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Environment")
	void ApplyWind(float Direction, float Strength, float Duration);

	// 눈바람으로 인한 입력 지연을 적용합니다. 중심 입력 반응을 늦춰 시야/환경 압박을 만듭니다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Environment")
	void ApplySnowstorm(float Duration, float InputLagScale);

	// 새 적재물 추가 대기/투입 흐름을 요청합니다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Environment")
	bool RequestCargoDrop();

	// 활성화된 환경 효과를 모두 해제합니다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Environment")
	void ClearEnvironmentEffects();

	// 현재 환경 효과가 반영된 최종 중심 입력값입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Environment")
	float GetProcessedBalanceInput() const { return ProcessedBalanceInput; }

	// 바람 또는 눈바람 효과가 아직 적용 중인지 여부입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Environment")
	bool HasActiveEnvironmentEffect() const;

private:
	// 개발 빌드에서 환경 효과 상태를 지면 텍스트로 표시합니다.
	void DrawEnvironmentDebug() const;

	// BeginPlay에서 캐시된 환경 이벤트 설정입니다.
	FPGEnvironmentSettings CachedEnvironmentSettings;

	// 적재물 추가 요청을 전달할 적재 컴포넌트입니다.
	UPROPERTY(Transient)
	TObjectPtr<UPGCargoStackComponent> CargoStackComponent;

	// 현재 런이 진행 중인지 여부입니다.
	bool bIsRunActive = true;

	// 바람 효과가 남은 시간입니다.
	float WindRemainingTime = 0.0f;

	// 바람이 중심을 미는 방향입니다.
	float WindDirection = 0.0f;

	// 바람 중심 영향 강도입니다.
	float WindStrength = 0.0f;

	// 눈바람 효과가 남은 시간입니다.
	float SnowstormRemainingTime = 0.0f;

	// 눈바람 입력 지연 비율입니다.
	float SnowstormInputLagScale = 1.0f;

	// 눈바람 보간에 사용하는 지연 중심 입력값입니다.
	float DelayedBalanceInput = 0.0f;

	// 환경 효과가 적용된 최종 중심 입력값입니다.
	float ProcessedBalanceInput = 0.0f;
};
