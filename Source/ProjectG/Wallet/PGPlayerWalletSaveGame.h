// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "PGPlayerWalletSaveGame.generated.h"

/**
 * 상점에서 사용할 플레이어 재화 저장 데이터입니다.
 * 현재는 누적 영수증 보유량만 저장하며, 상점/스킨 시스템이 붙으면 같은 저장 슬롯을 확장합니다.
 */
UCLASS()
class PROJECTG_API UPGPlayerWalletSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	// 플레이어가 상점 재화로 사용할 누적 영수증 보유량입니다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ProjectG | Wallet")
	int32 ReceiptBalance = 0;
};
