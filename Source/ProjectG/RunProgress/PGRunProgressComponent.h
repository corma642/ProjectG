// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Settings/PGGameSettings.h"
#include "PGRunProgressComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPGDisplayedDistanceChangedSignature, float, DisplayedDistanceMeters);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPGRunScoreChangedSignature, int32, Score, FName, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPGReceiptsChangedSignature, int32, Receipts, FName, Reason);

/**
 * 러닝 중 진행 거리와 점수를 추적하는 컴포넌트입니다.
 * 거리 기반 기본 점수와 배달 완료 보너스를 합산해 결과 화면과 HUD가 같은 값을 읽도록 관리합니다.
 */
UCLASS(ClassGroup = (ProjectG), meta = (BlueprintSpawnableComponent))
class PROJECTG_API UPGRunProgressComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPGRunProgressComponent();

	// 런 시작 지점과 방향, 점수 설정을 받아 진행 상태를 0부터 초기화합니다.
	void InitializeRunProgress(const FPGRunProgressSettings& InSettings, const FVector& InStartLocation, const FVector& InForwardVector, float InStartTimeSeconds);

	// 현재 위치 기준 진행 거리와 거리 기반 점수를 갱신합니다.
	void UpdateRunProgress(const FVector& CurrentLocation);

	// 실패 시점의 진행 거리와 점수를 마지막 값으로 고정합니다.
	void StopRunProgress(const FVector& CurrentLocation);

	// 거리 외 이벤트나 배달 보상 점수를 추가합니다.
	void AddScore(int32 ScoreDelta, FName Reason);

	// 영수증 수집 또는 배달 보상으로 얻은 영수증을 누적합니다.
	void AddReceipts(int32 Amount, FName Reason);

	// 결과 UI와 HUD에 표시할 5m 단위 진행 거리입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run Progress")
	float GetDisplayedDistanceMeters() const { return DisplayedDistanceMeters; }

	// 실제 전방 진행 거리입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run Progress")
	float GetRawDistanceMeters() const { return RawDistanceMeters; }

	// 거리 기반 점수와 배달 보너스를 합산한 현재 점수입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run Progress")
	int32 GetScore() const { return Score; }

	// 현재 런에서 누적 획득한 영수증 개수입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run Progress")
	int32 GetReceipts() const { return Receipts; }

	// 런 시작 이후 경과 시간입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run Progress")
	float GetElapsedTimeSeconds() const;

	// 표시 거리 값이 바뀔 때 HUD가 연결할 수 있는 이벤트입니다.
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Run Progress")
	FPGDisplayedDistanceChangedSignature OnDisplayedDistanceChanged;

	// 점수가 바뀔 때 HUD가 연결할 수 있는 이벤트입니다.
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Run Progress")
	FPGRunScoreChangedSignature OnScoreChanged;

	// 영수증 개수가 바뀌면 HUD와 결과 화면이 같은 값을 읽을 수 있도록 발생합니다.
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Run Progress")
	FPGReceiptsChangedSignature OnReceiptsChanged;

private:
	float CalculateRawDistanceMeters(const FVector& CurrentLocation) const;
	float QuantizeDistanceMeters(float InRawDistanceMeters) const;
	void RecalculateScore(FName Reason);

	FPGRunProgressSettings CachedSettings;
	FVector StartLocation = FVector::ZeroVector;
	FVector StartForwardVector = FVector::ForwardVector;
	float StartTimeSeconds = 0.0f;
	float RawDistanceMeters = 0.0f;
	float DisplayedDistanceMeters = 0.0f;
	int32 BonusScore = 0;
	int32 Score = 0;
	int32 Receipts = 0;
	bool bIsProgressActive = false;
};
