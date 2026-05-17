// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PGShopScreenWidget.generated.h"

class UButton;
class UPGCharacterShopItemWidget;
class UPGPlayerShopSubsystem;
class UPGPlayerWalletSubsystem;
class UScrollBox;
class UTextBlock;
class UWidget;

DECLARE_MULTICAST_DELEGATE(FPGShopScreenClosedSignature);

/**
 * Home 상점 화면의 로직 위젯입니다.
 * 캐릭터 상품 표시, 구매/장착, 프리뷰와 패널 애니메이션을 C++에서 처리합니다.
 */
UCLASS()
class PROJECTG_API UPGShopScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	FPGShopScreenClosedSignature OnShopScreenClosed;

	void InitializeShop(UPGPlayerShopSubsystem* InShopSubsystem, UPGPlayerWalletSubsystem* InWalletSubsystem, TSubclassOf<UPGCharacterShopItemWidget> InItemWidgetClass);
	void PlayOpenAnimation();
	void PlayCloseAnimation();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UWidget> RootPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UWidget> ShopPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UWidget> ShopContentPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UWidget> ReceiptPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UTextBlock> ReceiptBalanceText;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UWidget> TabPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UButton> CharactersTabButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UButton> MapsTabButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UWidget> CharacterNavPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UButton> PrevCharacterButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UButton> NextCharacterButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UWidget> CharacterListPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UScrollBox> CharacterListBox;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UWidget> SelectedNamePanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UTextBlock> SelectedCharacterNameText;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UWidget> SelectedPricePanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UTextBlock> SelectedPriceText;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UWidget> ActionButtonPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UButton> ActionButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UTextBlock> ActionButtonText;

private:
	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandlePrevCharacterClicked();

	UFUNCTION()
	void HandleNextCharacterClicked();

	UFUNCTION()
	void HandleActionClicked();

	UFUNCTION()
	void HandleMapsTabClicked();

	UFUNCTION()
	void HandleReceiptBalanceChanged(int32 NewBalance, FName Reason);

	UFUNCTION()
	void HandleShopOwnershipChanged(FName SkinId);

	UFUNCTION()
	void HandleShopEquippedChanged(FName SkinId);

	void SelectCharacterByIndex(int32 NewIndex);
	void SelectCharacterById(FName SkinId);
	void RebuildCharacterList();
	void RefreshCharacterItems();
	void RefreshSelectedCharacterState();
	void RefreshReceiptBalance();
	void ScrollSelectedCharacterIntoView() const;
	void SetShopButtonsEnabled(bool bEnabled) const;
	void ApplyEntryAnimation(float Alpha);
	void ApplyWidgetAnimationState(UWidget* Widget, const FVector2D& StartOffset, float Alpha, float Delay, float Duration, float StartScale = 1.0f, float EndScale = 1.0f) const;
	void ApplyActionFeedback(float DeltaSeconds);

	TWeakObjectPtr<UPGPlayerShopSubsystem> ShopSubsystem;
	TWeakObjectPtr<UPGPlayerWalletSubsystem> WalletSubsystem;
	TSubclassOf<UPGCharacterShopItemWidget> CharacterItemWidgetClass;

	TArray<TObjectPtr<UPGCharacterShopItemWidget>> CharacterItemWidgets;
	int32 SelectedCharacterIndex = INDEX_NONE;

	bool bIsOpening = false;
	bool bIsClosing = false;
	float AnimationElapsed = 0.0f;
	float AnimationDuration = 0.56f;

	bool bActionFeedbackActive = false;
	float ActionFeedbackElapsed = 0.0f;
	float ActionFeedbackDuration = 0.22f;
	bool bActionFeedbackSuccess = true;
};
