// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PGCargoTypeDefinition.generated.h"

class UStaticMesh;

/**
 * Go Deliver!에서 적재물 한 종류의 표시 메시, 핵심 여부, 흔들림/손상 계수를 정의하는 데이터 에셋
 */
UCLASS(BlueprintType)
class PROJECTG_API UPGCargoTypeDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// 에디터와 디버그 표시에서 읽을 적재물 이름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cargo")
	FText DisplayName;

	// 적재물 슬롯에 표시할 Static Mesh. 비워두면 디버그 구체로 표시한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cargo")
	TObjectPtr<UStaticMesh> CargoMesh;

	// 잃으면 즉시 실패 흐름으로 이어지는 핵심 적재물인지 여부
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cargo")
	bool bIsCoreCargo = true;

	// 중심 입력이 이 적재물의 시각 흔들림에 반영되는 배율
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cargo", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "3.0"))
	float SwayMultiplier = 1.0f;

	// 중심 입력과 라인 점프가 안정도 손상으로 누적되는 배율
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cargo", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "3.0"))
	float StabilityDamageMultiplier = 1.0f;
};
