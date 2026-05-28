// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PGMapDefinition.generated.h"

class UTexture2D;
class UWorld;

/**
 * 상점에서 판매하고 장착할 수 있는 러닝 맵 상품 데이터입니다.
 * 맵 장착은 월드 일부를 교체하지 않고 지정 레벨로 이동하는 방식으로 처리합니다.
 */
UCLASS(BlueprintType)
class PROJECTG_API UPGMapDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	// 구매, 보유, 장착 상태를 저장할 때 사용하는 고유 ID입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	FName MapId;

	// 상점 UI에 표시할 맵 이름입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	FText DisplayName;

	// 맵 탭 중앙 프리뷰와 카드에 표시할 썸네일입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	TObjectPtr<UTexture2D> Thumbnail;

	// 구매에 필요한 영수증 가격입니다. 기본 해금 맵은 0으로 둡니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map", meta = (ClampMin = "0", UIMin = "0"))
	int32 PriceReceipts = 0;

	// 선택 시 이동할 실제 게임 레벨입니다. 패키징 MapsToCook에도 반드시 포함해야 합니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	TSoftObjectPtr<UWorld> Level;

	// 첫 실행부터 보유 상태로 보정할 기본 맵인지 여부입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	bool bUnlockedByDefault = false;
};
