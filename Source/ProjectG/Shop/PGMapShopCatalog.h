// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PGMapShopCatalog.generated.h"

class UPGMapDefinition;
class UWorld;

/**
 * 상점 맵 탭에 표시할 맵 상품 목록과 기본 장착 맵을 정의하는 카탈로그입니다.
 */
UCLASS(BlueprintType)
class PROJECTG_API UPGMapShopCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	// 현재 실제로 열린 레벨과 일치하는 맵 상품을 찾습니다. PIE 복제 레벨 prefix도 제거해 비교합니다.
	const UPGMapDefinition* FindMapForWorld(const UWorld* World, bool& bOutHasDuplicateLevel) const;

	// 카탈로그 검증과 런타임 월드 조회에서 공통으로 사용하는 정규화된 패키지 경로입니다.
	static FString GetNormalizedLevelPackageName(const UPGMapDefinition* MapDefinition);
	static FString GetNormalizedWorldPackageName(const UWorld* World);

	// 기본 보유/장착 맵으로 사용할 MapId입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Shop")
	FName DefaultMapId;

	// 상점에 표시할 맵 상품 목록입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Shop")
	TArray<TObjectPtr<UPGMapDefinition>> Maps;
};
