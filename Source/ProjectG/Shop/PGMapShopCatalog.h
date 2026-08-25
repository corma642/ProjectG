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
	/** 현재 월드와 일치하는 맵 상품을 반환합니다. 중복 레벨 매핑이면 bOutHasDuplicateLevel을 true로 설정합니다. */
	const UPGMapDefinition* FindMapForWorld(const UWorld* World, bool& bOutHasDuplicateLevel) const;

	/** 맵 DataAsset의 소프트 월드 경로를 비교 가능한 패키지 경로로 정규화하며 유효하지 않으면 빈 문자열입니다. */
	static FString GetNormalizedLevelPackageName(const UPGMapDefinition* MapDefinition);
	/** PIE prefix를 제거한 현재 월드 패키지 경로를 반환하며 World가 null이면 빈 문자열입니다. */
	static FString GetNormalizedWorldPackageName(const UWorld* World);

	// 기본 보유/장착 맵으로 사용할 MapId입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Shop")
	FName DefaultMapId;

	// 상점에 표시할 맵 상품 목록입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Shop")
	TArray<TObjectPtr<UPGMapDefinition>> Maps;
};
