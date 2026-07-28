// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Settings/PGGameSettings.h"
#include "PGReceiptShopWidget.generated.h"

class UBackgroundBlur;
class UButton;
class UPanelWidget;
class UPGReceiptShopPackItemWidget;
class UPGUserDataSubsystem;
class UTextBlock;
class UWidget;

DECLARE_MULTICAST_DELEGATE(FPGReceiptShopClosedSignature);

/**
 * Home에서 여는 영수증 구매 화면입니다.
 * 상품 클릭은 설정된 영수증을 즉시 지급하며, 누적 재화 저장은 UPGUserDataSubsystem이 담당합니다.
 */
UCLASS()
class PROJECTG_API UPGReceiptShopWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	FPGReceiptShopClosedSignature OnReceiptShopClosed;

	void InitializeReceiptShop(UPGUserDataSubsystem* InUserDataSubsystem, TSubclassOf<UPGReceiptShopPackItemWidget> InPackItemWidgetClass, const TArray<FPGReceiptShopPack>& InReceiptPacks);
	void PlayOpenAnimation();
	void PlayCloseAnimation();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 영수증 상점 전체 루트입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Receipt Shop")
	TObjectPtr<UWidget> RootPanel;

	// Home 배경을 살짝 흐리게 하는 블러 레이어입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Receipt Shop")
	TObjectPtr<UBackgroundBlur> BackgroundBlur;

	// Home 배경을 어둡게 덮는 딤 레이어입니다. 실제 색상/브러시는 BP가 담당합니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Receipt Shop")
	TObjectPtr<UWidget> BackdropDim;

	// 제목, 현재 보유량, 상품 리스트를 포함하는 메인 상점 패널입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Receipt Shop")
	TObjectPtr<UWidget> ShopPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Receipt Shop")
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Receipt Shop")
	TObjectPtr<UTextBlock> ReceiptBalanceText;

	// 상품 행 위젯들이 추가될 컨테이너입니다. VerticalBox, ScrollBox 등 UPanelWidget 계열을 사용할 수 있습니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Receipt Shop")
	TObjectPtr<UPanelWidget> PackListBox;

private:
	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleReceiptBalanceChanged(int32 NewBalance, FName Reason);

	void HandlePackSelected(int32 PackIndex);
	void RebuildPackList();
	void RefreshReceiptBalance() const;
	void SetReceiptShopButtonsEnabled(bool bEnabled) const;
	void ApplyEntryAnimation(float Alpha) const;

	TWeakObjectPtr<UPGUserDataSubsystem> UserDataSubsystem;
	TSubclassOf<UPGReceiptShopPackItemWidget> PackItemWidgetClass;
	TArray<FPGReceiptShopPack> ReceiptPacks;
	TArray<TObjectPtr<UPGReceiptShopPackItemWidget>> PackItemWidgets;
	TMap<int32, TObjectPtr<UPGReceiptShopPackItemWidget>> PackItemWidgetsByPackIndex;

	bool bIsOpening = false;
	bool bIsClosing = false;
	float AnimationElapsed = 0.0f;
	float AnimationDuration = 0.38f;
};
