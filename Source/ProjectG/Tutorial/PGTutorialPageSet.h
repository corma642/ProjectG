// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PGTutorialPageSet.generated.h"

class UTexture2D;

/**
 * Home Help 튜토리얼에서 책처럼 넘겨 보여줄 설명 이미지 목록입니다.
 * BP/에디터에서 페이지 이미지를 순서대로 등록하고, C++은 이 순서를 그대로 사용합니다.
 */
UCLASS(BlueprintType)
class PROJECTG_API UPGTutorialPageSet : public UDataAsset
{
	GENERATED_BODY()

public:
	// 튜토리얼 페이지 이미지 목록. 배열 순서가 실제 표시 순서입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial")
	TArray<TSoftObjectPtr<UTexture2D>> Pages;
};
