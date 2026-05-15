// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "PGPlayerShopSaveGame.generated.h"

/**
 * 플레이어가 구매한 캐릭터 스킨과 현재 장착 스킨을 저장하는 SaveGame입니다.
 * 실제 영수증 보유량은 지갑 SaveGame에서 별도로 관리합니다.
 */
UCLASS()
class PROJECTG_API UPGPlayerShopSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ProjectG | Shop")
	TArray<FName> OwnedCharacterSkinIds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ProjectG | Shop")
	FName EquippedCharacterSkinId;
};
