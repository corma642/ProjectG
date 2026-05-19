// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PGCharacterShopItemWidget.generated.h"

class UImage;
class UPGCharacterSkinDefinition;
class UButton;
class UWidget;

DECLARE_MULTICAST_DELEGATE_OneParam(FPGCharacterShopItemSelectedSignature, FName);

/**
 * 상점 캐릭터 목록에 들어가는 개별 카드 로직 위젯입니다.
 * 카드 내부는 썸네일, 잠금 표시, CURRENT 표시만 담당하고 가격/구매 상태는 상점 상세 영역에서 표시합니다.
 */
UCLASS()
class PROJECTG_API UPGCharacterShopItemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	FPGCharacterShopItemSelectedSignature OnCharacterSelected;

	void SetCharacterSkin(const UPGCharacterSkinDefinition* InSkinDefinition, bool bOwned, bool bEquipped, bool bSelected);
	FName GetSkinId() const { return SkinId; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 카드 전체 루트입니다. 선택 상태일 때 스케일/투명도 애니메이션을 적용합니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop Item")
	TObjectPtr<UWidget> RootPanel;

	// 카드 전체를 덮는 투명 선택 버튼입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop Item")
	TObjectPtr<UButton> SelectButton;

	// 캐릭터 스킨 썸네일 이미지입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop Item")
	TObjectPtr<UImage> ThumbnailImage;

	// 미보유 캐릭터에 표시하는 잠금 오버레이입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop Item")
	TObjectPtr<UWidget> LockImage;

	// 현재 장착 중인 캐릭터에 표시하는 CURRENT 마커입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop Item")
	TObjectPtr<UWidget> CurrentMarker;

private:
	UFUNCTION()
	void HandleSelectButtonClicked();

	void StartStateAnimation(bool bOwned, bool bEquipped, bool bSelected);
	void ApplyStateAnimation(float Alpha);
	void ApplyFinalState();
	void BroadcastSelected();

	FName SkinId;
	bool bLastOwned = false;
	bool bLastEquipped = false;
	bool bLastSelected = false;
	bool bHasState = false;
	bool bStateAnimationActive = false;
	float StateAnimationElapsed = 0.0f;
	float StateAnimationDuration = 0.18f;
	float StartRootScale = 1.0f;
	float StartRootOpacity = 0.86f;
	float StartThumbnailScale = 1.0f;
	float StartLockOpacity = 0.0f;
	float StartMarkerOpacity = 0.0f;
	FVector2D StartMarkerTranslation = FVector2D::ZeroVector;
};
