// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PGCharacterSkinDefinition.generated.h"

class UAnimInstance;
class USkeletalMesh;
class UTexture2D;

/**
 * 상점에서 판매하고 러너에 적용할 캐릭터 스킨 데이터입니다.
 * 실제 Actor 교체 없이 러너의 SkeletalMesh/AnimClass/상대 Transform만 교체합니다.
 */
UCLASS(BlueprintType)
class PROJECTG_API UPGCharacterSkinDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	// 저장/구매/장착 상태를 식별하는 고유 ID입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Skin")
	FName SkinId;

	// 상점 UI에 표시할 캐릭터 이름입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Skin")
	FText DisplayName;

	// 상점 카드에 표시할 썸네일 이미지입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Skin")
	TObjectPtr<UTexture2D> Thumbnail;

	// 구매에 필요한 영수증 가격입니다. 기본 지급 캐릭터는 0으로 둡니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Skin", meta = (ClampMin = "0", UIMin = "0"))
	int32 PriceReceipts = 0;

	// 러너 MeshComponent에 적용할 Skeletal Mesh입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Skin | Mesh")
	TObjectPtr<USkeletalMesh> SkeletalMesh;

	// 러너 MeshComponent에 적용할 AnimInstance 클래스입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Skin | Mesh")
	TSubclassOf<UAnimInstance> AnimClass;

	// 캐릭터별 시각 위치 보정값입니다. 캡슐/CargoStack 기준은 변경하지 않습니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Skin | Mesh")
	FVector MeshRelativeLocation = FVector::ZeroVector;

	// 캐릭터별 시각 회전 보정값입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Skin | Mesh")
	FRotator MeshRelativeRotation = FRotator::ZeroRotator;

	// 캐릭터별 시각 스케일 보정값입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Skin | Mesh")
	FVector MeshRelativeScale = FVector::OneVector;

	// 첫 실행부터 보유한 기본 캐릭터인지 여부입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Skin")
	bool bUnlockedByDefault = false;
};
