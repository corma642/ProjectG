// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PGCargoDropSet.generated.h"

class UPGCargoTypeDefinition;

/**
 * 러닝 중 자동으로 드롭할 적재물 후보 하나의 선택 규칙입니다.
 */
USTRUCT(BlueprintType)
struct FPGCargoDropEntry
{
	GENERATED_BODY()

	// 드롭할 적재물 데이터
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cargo Drop")
	TObjectPtr<UPGCargoTypeDefinition> CargoType;

	// 기본 선택 가중치
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cargo Drop", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "10.0"))
	float BaseWeight = 1.0f;

	// 이 러닝 시간 이후부터 후보로 사용할 수 있다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cargo Drop", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "300.0"))
	float MinRunTime = 0.0f;
};

/**
 * 배달용 적재물 드롭 후보 목록입니다.
 * 환경 RunEvent와 분리된 CargoDeliverySubsystem이 이 데이터로 적재물을 고릅니다.
 */
UCLASS(BlueprintType)
class PROJECTG_API UPGCargoDropSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// 러닝 중 랜덤 드롭 후보로 사용할 적재물 목록
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cargo Drop Set")
	TArray<FPGCargoDropEntry> Entries;
};
