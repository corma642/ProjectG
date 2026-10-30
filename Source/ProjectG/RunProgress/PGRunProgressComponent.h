// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Settings/PGGameSettings.h"
#include "PGRunProgressComponent.generated.h"

/** 표시용 진행 거리가 바뀔 때 미터 단위 값을 전달하는 delegate입니다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPGDisplayedDistanceChangedSignature, float, DisplayedDistanceMeters);

/** 현재 런 점수가 바뀔 때 새 값과 변경 사유를 전달하는 delegate입니다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPGRunScoreChangedSignature, int32, Score, FName, Reason);

/** 현재 런에서 획득한 영수증 수가 바뀔 때 새 값과 변경 사유를 전달하는 delegate입니다. */
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
	/** 외부 러너 갱신만 사용하므로 자체 ActorComponent Tick을 비활성화해 생성합니다. */
	UPGRunProgressComponent();

	/** 런 시작 월드 위치·전방 단위 벡터·초 단위 시작 시각을 기준으로 진행 상태를 0부터 초기화합니다. */
	void InitializeRunProgress(const FPGRunProgressSettings& InSettings, const FVector& InStartLocation, const FVector& InForwardVector, float InStartTimeSeconds);

	/** 현재 월드 위치의 전방 투영 거리와 거리 기반 점수를 갱신하고 변경 delegate를 발생시킵니다. */
	void UpdateRunProgress(const FVector& CurrentLocation);

	/** 실패 또는 런 종료 시점의 마지막 위치를 반영한 뒤 이후 진행 갱신을 정지합니다. */
	void StopRunProgress(const FVector& CurrentLocation);

	/** 이어하기로 런이 재개될 때 거리·점수·영수증을 유지한 채 진행 갱신만 다시 켭니다. */
	void ResumeRunProgress() { bIsProgressActive = true; }

	/** 양수 이벤트 보너스를 현재 런 점수에 더하고 OnScoreChanged를 발생시킵니다. */
	void AddScore(int32 ScoreDelta, FName Reason);

	/** 양수 영수증 보상을 이번 런에 누적하고 OnReceiptsChanged를 발생시킵니다. */
	void AddReceipts(int32 Amount, FName Reason);

	/** 결과 UI와 HUD에 표시할 설정 단위 양자화 거리를 미터로 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run")
	float GetDisplayedDistanceMeters() const { return DisplayedDistanceMeters; }

	/** 시작점에서 현재 위치까지의 실제 전방 투영 거리를 미터로 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run")
	float GetRawDistanceMeters() const { return RawDistanceMeters; }

	/** 거리 기반 점수와 이벤트 보너스를 합산한 현재 점수를 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run")
	int32 GetScore() const { return Score; }

	/** 현재 런에서 획득한 영수증 수를 반환합니다. Home의 영구 지갑 잔액과는 별개입니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run")
	int32 GetReceipts() const { return Receipts; }

	/** 런 시작 이후 경과 시간을 초 단위로 반환합니다. 월드 시간 기준이라 정지 뒤에도 계속 증가합니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run")
	float GetElapsedTimeSeconds() const;

	/** 표시 거리 값이 바뀔 때 HUD 갱신용 미터 값을 전달합니다. */
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Run")
	FPGDisplayedDistanceChangedSignature OnDisplayedDistanceChanged;

	/** 점수가 바뀔 때 새 값과 변경 이유를 HUD와 결과 데이터에 전달합니다. */
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Run")
	FPGRunScoreChangedSignature OnScoreChanged;

	/** 이번 런 영수증 값이 바뀔 때 새 값과 변경 이유를 전달합니다. */
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
