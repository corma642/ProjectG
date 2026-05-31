// Copyright Epic Games, Inc. All Rights Reserved.

#include "Shop/PGMapShopCatalog.h"

#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "Shop/PGMapDefinition.h"

FString UPGMapShopCatalog::GetNormalizedLevelPackageName(const UPGMapDefinition* MapDefinition)
{
	return MapDefinition && !MapDefinition->Level.IsNull()
		? FPackageName::ObjectPathToPackageName(MapDefinition->Level.ToSoftObjectPath().ToString())
		: FString();
}

FString UPGMapShopCatalog::GetNormalizedWorldPackageName(const UWorld* World)
{
	return World ? UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) : FString();
}

const UPGMapDefinition* UPGMapShopCatalog::FindMapForWorld(const UWorld* World, bool& bOutHasDuplicateLevel) const
{
	bOutHasDuplicateLevel = false;
	if (!World)
	{
		return nullptr;
	}

	// PIE는 원본 맵을 UEDPIE_* 패키지로 복제한다. 상품 데이터의 원본 Level 경로와
	// 비교할 때만 prefix를 제거해 에디터 직접 실행과 패키징 실행을 같은 규칙으로 처리한다.
	const FString CurrentLevelPackageName = GetNormalizedWorldPackageName(World);
	const UPGMapDefinition* MatchedMap = nullptr;

	for (const UPGMapDefinition* MapDefinition : Maps)
	{
		if (!MapDefinition || MapDefinition->Level.IsNull())
		{
			continue;
		}

		const FString MapLevelPackageName = GetNormalizedLevelPackageName(MapDefinition);
		if (!CurrentLevelPackageName.Equals(MapLevelPackageName, ESearchCase::CaseSensitive))
		{
			continue;
		}

		if (MatchedMap)
		{
			bOutHasDuplicateLevel = true;
			return nullptr;
		}

		MatchedMap = MapDefinition;
	}

	return MatchedMap;
}
