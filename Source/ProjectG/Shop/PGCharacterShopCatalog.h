// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PGCharacterShopCatalog.generated.h"

class UPGCharacterSkinDefinition;

/**
 * 상점에 표시할 캐릭터 스킨 목록과 기본 장착 캐릭터를 정의하는 카탈로그입니다.
 * 사용자는 캐릭터 상품 DataAsset을 원하는 진열 순서대로 등록합니다.
 */
UCLASS(BlueprintType)
class PROJECTG_API UPGCharacterShopCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	// 기본 보유/장착 캐릭터로 사용할 SkinId입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Shop")
	FName DefaultSkinId;

	// 상점 캐릭터 진열 순서입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Shop")
	TArray<TObjectPtr<UPGCharacterSkinDefinition>> CharacterSkins;
};
