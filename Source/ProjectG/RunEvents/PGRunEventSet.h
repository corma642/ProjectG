// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PGRunEventSet.generated.h"

/**
 * 러닝 중 전역 이벤트 종류입니다.
 * 트랙 세그먼트와 분리되어 RunEventSubsystem이 시간 기반으로 예약하고 발동합니다.
 */
UENUM(BlueprintType)
enum class EPGRunEventType : uint8
{
	None UMETA(DisplayName = "None"),
	Wind UMETA(DisplayName = "Wind"),
	Snowstorm UMETA(DisplayName = "Snowstorm"),
	CargoDrop UMETA(DisplayName = "Cargo Drop")
};

/**
 * 전역 런 이벤트의 예약/예고/발동 상태입니다.
 * BP/UMG는 이 값을 읽어 이벤트 예고 UI와 연출을 구성할 수 있습니다.
 */
UENUM(BlueprintType)
enum class EPGRunEventState : uint8
{
	Inactive UMETA(DisplayName = "Inactive"),
	Scheduled UMETA(DisplayName = "Scheduled"),
	Preview UMETA(DisplayName = "Preview"),
	Triggered UMETA(DisplayName = "Triggered")
};

/**
 * 러닝 시간 기반 전역 이벤트 하나의 선택 규칙입니다.
 * 이벤트별 난이도, 가중치, 쿨다운, 효과 강도를 데이터로 조정합니다.
 */
USTRUCT(BlueprintType)
struct FPGRunEventRule
{
	GENERATED_BODY()

	// 발동할 이벤트 종류입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Event")
	EPGRunEventType EventType = EPGRunEventType::None;

	// 이 러닝 시간 이후부터 후보가 됩니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Event", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "180.0"))
	float MinRunTime = 0.0f;

	// 이 난이도 이상부터 후보가 됩니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Event", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float MinDifficultyAlpha = 0.0f;

	// 기본 선택 가중치입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Event", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "10.0"))
	float BaseWeight = 1.0f;

	// 난이도가 1.0에 가까워질수록 추가되는 가중치입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Event", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "10.0"))
	float DifficultyWeight = 0.0f;

	// 같은 이벤트가 다시 발동되기 전 필요한 시간입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Event", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "60.0"))
	float CooldownSeconds = 6.0f;

	// 현재 적재 수가 최대치면 후보에서 제외할지 여부입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Event")
	bool bRequiresCargoCapacity = false;

	// 바람/눈바람 효과 지속 시간입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Event | Effect", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "10.0"))
	float Duration = 3.0f;

	// 바람 이벤트 강도입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Event | Effect", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "3.0"))
	float Strength = 1.0f;

	// 눈바람 입력 지연 비율입니다. 낮을수록 입력 반응이 더 둔해집니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Event | Effect", meta = (ClampMin = "0.05", ClampMax = "1.0", UIMin = "0.05", UIMax = "1.0"))
	float InputLagScale = 0.35f;

	// 바람 방향을 발동마다 좌/우 랜덤으로 정할지 여부입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Event | Effect")
	bool bRandomizeDirection = true;
};

/**
 * 전역 런 이벤트 후보 목록 DataAsset입니다.
 * RunEventSubsystem이 이 데이터를 기반으로 러닝 시간에 따라 이벤트를 예약합니다.
 */
UCLASS(BlueprintType)
class PROJECTG_API UPGRunEventSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// 러닝 중 자동 발동할 이벤트 규칙 목록입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Event Set")
	TArray<FPGRunEventRule> EventRules;
};
