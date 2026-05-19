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
 * BP가 상점 배경과 카드 배치를 담당하고, C++은 캐릭터 선택/프리뷰/구매/장착과 진입 애니메이션만 처리합니다.
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

	// WBP_ShopScreen 전체 루트입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UWidget> RootPanel;

	// 상점의 큰 배경 이미지 영역입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UWidget> ShopPanel;

	// 탭, 캐릭터 리스트, 선택 정보, 액션 버튼을 담는 메인 콘텐츠 패널입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UWidget> ShopContentPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UButton> CloseButton;

	// 현재 보유 영수증 표시 패널입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UWidget> ReceiptPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UTextBlock> ReceiptBalanceText;

	// Characters / Maps 탭 버튼 묶음입니다. Maps는 현재 비활성 상태로 유지합니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UWidget> TabPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UButton> CharactersTabButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UButton> MapsTabButton;

	// 선택 캐릭터를 이전/다음으로 넘기는 보조 버튼 영역입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UWidget> CharacterNavPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UButton> PrevCharacterButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UButton> NextCharacterButton;

	// 관성 스크롤을 맡는 캐릭터 카드 리스트 영역입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UWidget> CharacterListPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UScrollBox> CharacterListBox;

	// 선택된 캐릭터 이름을 표시하는 상세 영역입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UWidget> SelectedNamePanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UTextBlock> SelectedCharacterNameText;

	// 미보유 캐릭터 선택 시 가격과 영수증 아이콘을 보여주는 상세 가격 패널입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UWidget> SelectedPricePanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop")
	TObjectPtr<UTextBlock> SelectedPriceText;

	// BUY / EQUIP / EQUIPPED 액션 버튼 영역입니다.
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
