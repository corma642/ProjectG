// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "PGUserSaveGame.generated.h"

/**
 * 유저 진행 데이터를 한곳에 저장하는 통합 SaveGame입니다.
 * 그래픽, 사운드, FPS 같은 환경설정은 UPGGameUserSettings가 담당하고,
 * 영수증 재화, 해금, 장착 상태만 여기에 저장합니다.
 */
UCLASS()
class PROJECTG_API UPGUserSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	// 상점 재화로 사용하는 누적 영수증 보유량입니다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ProjectG | User")
	int32 ReceiptBalance = 0;

	// Home 화면에 표시하는 지금까지의 최고 런 점수입니다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ProjectG | User")
	int32 BestRunScore = 0;

	// 구매했거나 기본 해금된 캐릭터 스킨 ID 목록입니다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ProjectG | User")
	TArray<FName> OwnedCharacterSkinIds;

	// 현재 러너에 적용할 장착 캐릭터 스킨 ID입니다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ProjectG | User")
	FName EquippedCharacterSkinId;

	// 구매했거나 기본 보유 중인 맵 상품 ID 목록입니다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ProjectG | User")
	TArray<FName> OwnedMapIds;

	// 현재 Home/Run 레벨로 사용할 장착 맵 ID입니다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ProjectG | User")
	FName EquippedMapId;

	// 튜토리얼 스테이지를 끝내 완료 보상을 받았는지. 이전 첫 런 가이드 완료 플래그라 저장 호환을 위해 이름을 유지합니다. true면 보상을 다시 주지 않고 첫 START도 일반 런으로 갑니다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ProjectG | User")
	bool bRunGuideCompleted = false;
};
