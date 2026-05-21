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
 * 한 런의 진행 거리, 점수, 획득 영수증을 추적하는 컴포넌트입니다.
 * 거리 기반 점수와 배달 보너스를 합산해 HUD와 결과 화면이 같은 스냅샷을 읽도록 관리합니다.
 */
UCLASS(ClassGroup = (ProjectG), meta = (BlueprintSpawnableComponent))
class PROJECTG_API UPGRunProgressComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPGRunProgressComponent();

	// 런 시작 위치와 전방 방향을 기준으로 거리/점수/영수증을 0부터 초기화합니다.
	void InitializeRunProgress(const FPGRunProgressSettings& InSettings, const FVector& InStartLocation, const FVector& InForwardVector, float InStartTimeSeconds);

	// 현재 위치 기준 진행 거리와 거리 기반 점수를 갱신합니다.
	void UpdateRunProgress(const FVector& CurrentLocation);

	// 실패 또는 런 종료 시점의 마지막 진행 값을 고정합니다.
	void StopRunProgress(const FVector& CurrentLocation);

	// 배달 완료 같은 이벤트 보너스 점수를 현재 런 점수에 더합니다.
	void AddScore(int32 ScoreDelta, FName Reason);

	// 영수증 픽업 또는 배달 보상으로 얻은 이번 런 영수증을 누적합니다.
	void AddReceipts(int32 Amount, FName Reason);

	// 결과 UI와 HUD에 표시할 5m 단위 진행 거리입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run")
	float GetDisplayedDistanceMeters() const { return DisplayedDistanceMeters; }

	// 실제 전방 진행 거리입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run")
	float GetRawDistanceMeters() const { return RawDistanceMeters; }

	// 거리 기반 점수와 배달 보너스를 합산한 현재 점수입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run")
	int32 GetScore() const { return Score; }

	// 현재 런에서 획득한 영수증 개수입니다. Home 지갑 잔액과는 별개입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run")
	int32 GetReceipts() const { return Receipts; }

	// 런 시작 이후 경과 시간입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run")
	float GetElapsedTimeSeconds() const;

	// 표시 거리 값이 바뀔 때 HUD가 갱신할 수 있도록 발생합니다.
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Run")
	FPGDisplayedDistanceChangedSignature OnDisplayedDistanceChanged;

	// 점수가 바뀔 때 HUD와 결과 데이터가 같은 값을 읽도록 발생합니다.
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Run")
	FPGRunScoreChangedSignature OnScoreChanged;

	// 이번 런 영수증 값이 바뀔 때 HUD와 결과 화면 갱신용으로 발생합니다.
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Run")
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
