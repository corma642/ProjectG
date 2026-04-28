// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Settings/PGGameSettings.h"
#include "PGRunnerEnvironmentComponent.generated.h"

class UPGCargoStackComponent;

/**
 * 러너에게 적용 중인 바람, 얼음 길, 보너스 적재 요청 같은 환경 이벤트 상태를 관리하는 컴포넌트
 */
UCLASS(ClassGroup = (ProjectG), meta = (BlueprintSpawnableComponent))
class PROJECTG_API UPGRunnerEnvironmentComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UPGRunnerEnvironmentComponent();

	// Project Settings 환경 설정과 적재물 스택 참조를 받아 환경 이벤트 처리 준비를 끝낸다.
	void InitializeEnvironment(const FPGEnvironmentSettings& InEnvironmentSettings, UPGCargoStackComponent* InCargoStackComponent);

	// 현재 런이 진행 중인지 갱신한다. 실패 후에는 환경 효과와 적재 요청을 무시한다.
	void SetRunActive(bool bInRunActive);

	// 활성화된 환경 효과를 갱신하고 최종 중심 입력값을 반환한다.
	float UpdateEnvironment(float DeltaSeconds, float RawBalanceInput);

	// 지정 방향의 바람을 적용한다. Direction은 -1 또는 1 방향으로 정규화된다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Environment")
	void ApplyWind(float Direction, float Strength, float Duration);

	// 얼음 길 입력 지연을 적용한다. InputLagScale이 낮을수록 입력 반응이 느려진다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Environment")
	void ApplyIce(float Duration, float InputLagScale);

	// 보너스 적재물을 요청한다. 실패 상태이거나 최대 적재 개수면 false를 반환한다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Environment")
	bool RequestBonusCargo();

	// 활성화된 환경 효과를 모두 해제한다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Environment")
	void ClearEnvironmentEffects();

	// 현재 환경 효과가 반영된 최종 중심 입력값
	UFUNCTION(BlueprintPure, Category = "ProjectG | Environment")
	float GetProcessedBalanceInput() const { return ProcessedBalanceInput; }

	// 바람 또는 얼음 효과가 아직 적용 중인지 여부
	UFUNCTION(BlueprintPure, Category = "ProjectG | Environment")
	bool HasActiveEnvironmentEffect() const;

private:
	// 개발 빌드에서 환경 효과 상태를 지면 위 텍스트로 표시한다.
	void DrawEnvironmentDebug() const;

	// BeginPlay에서 캐시한 환경 이벤트 설정
	FPGEnvironmentSettings CachedEnvironmentSettings;

	// 보너스 적재 요청을 전달할 적재물 스택 컴포넌트
	UPROPERTY(Transient)
	TObjectPtr<UPGCargoStackComponent> CargoStackComponent;

	// 현재 런이 진행 중인지 여부
	bool bIsRunActive = true;

	// 바람 효과가 남아 있는 시간
	float WindRemainingTime = 0.0f;

	// 바람이 중심을 미는 방향
	float WindDirection = 0.0f;

	// 바람 중심 영향 강도
	float WindStrength = 0.0f;

	// 얼음 길 효과가 남아 있는 시간
	float IceRemainingTime = 0.0f;

	// 얼음 길 입력 지연 비율
	float IceInputLagScale = 1.0f;

	// 얼음 길 보간에 사용하는 지연 중심 입력값
	float DelayedBalanceInput = 0.0f;

	// 환경 효과가 적용된 최종 중심 입력값
	float ProcessedBalanceInput = 0.0f;
};
