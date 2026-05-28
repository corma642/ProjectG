// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PGMapShopCatalog.generated.h"

class UPGMapDefinition;

/**
 * 상점 맵 탭에 표시할 맵 상품 목록과 기본 장착 맵을 정의하는 카탈로그입니다.
 */
UCLASS(BlueprintType)
class PROJECTG_API UPGMapShopCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	// 기본 보유/장착 맵으로 사용할 MapId입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Shop")
	FName DefaultMapId;

	// 상점에 표시할 맵 상품 목록입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Shop")
	TArray<TObjectPtr<UPGMapDefinition>> Maps;
};
