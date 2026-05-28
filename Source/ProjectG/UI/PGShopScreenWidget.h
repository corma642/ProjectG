// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PGShopScreenWidget.generated.h"

class UButton;
class UImage;
class UPGCharacterShopItemWidget;
class UPGMapShopItemWidget;
class UPGUserDataSubsystem;
class UScrollBox;
class UTextBlock;
class UWidget;

DECLARE_MULTICAST_DELEGATE(FPGShopCloseRequestedSignature);
DECLARE_MULTICAST_DELEGATE(FPGShopReceiptShopRequestedSignature);
DECLARE_MULTICAST_DELEGATE_OneParam(FPGShopMapTravelRequestedSignature, FName);

UENUM()
enum class EPGShopTab : uint8
{
	Characters,
	Maps
};

/**
 * Home 상점 화면의 로직 전용 위젯입니다.
 * BP는 상점 배경, 탭, 리스트, 버튼 배치를 담당하고, C++은 캐릭터 선택/프리뷰/구매/장착과 진입 애니메이션만 처리합니다.
 */
UCLASS()
class PROJECTG_API UPGShopScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	FPGShopCloseRequestedSignature OnShopCloseRequested;
	FPGShopReceiptShopRequestedSignature OnReceiptShopRequested;
	FPGShopMapTravelRequestedSignature OnMapTravelRequested;

	void InitializeShop(UPGUserDataSubsystem* InUserDataSubsystem, TSubclassOf<UPGCharacterShopItemWidget> InCharacterItemWidgetClass, TSubclassOf<UPGMapShopItemWidget> InMapItemWidgetClass);
	void PlayOpenAnimation();
	void SetShopInteractionEnabled(bool bEnabled);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// WBP_ShopScreen 전체 루트 패널입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> RootPanel;

	// 상점 화면의 큰 배경 이미지 영역입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> ShopPanel;

	// 탭, 캐릭터 리스트, 선택 정보, 액션 버튼을 담는 메인 콘텐츠 패널입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> ShopContentPanel;

	// 상점을 닫고 Home 화면으로 돌아가는 버튼입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UButton> CloseButton;

	// 현재 보유 영수증을 표시하는 상단 패널입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> ReceiptPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UTextBlock> ReceiptBalanceText;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UButton> ReceiptAddButton;

	// Characters / Maps 탭 버튼 묶음입니다. Maps 탭은 아직 비활성 상태로 유지합니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> TabPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UButton> CharactersTabButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UButton> MapsTabButton;

	// 선택 캐릭터를 이전/다음으로 넘기는 보조 버튼 영역입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> ItemNavPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UButton> PrevItemButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UButton> NextItemButton;

	// 캐릭터 카드 ScrollBox를 감싸는 패널입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> ItemListPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UScrollBox> ItemListBox;

	// 맵 탭 전체 콘텐츠 패널입니다. 중앙 썸네일 프리뷰와 맵 카드 리스트를 포함합니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UImage> SelectedMapThumbnailImage;

	// 선택된 캐릭터 이름을 보여주는 상세 영역입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> SelectedNamePanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UTextBlock> SelectedItemNameText;

	// 미보유 캐릭터 선택 시 가격과 영수증 아이콘을 보여주는 상세 가격 패널입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> SelectedPricePanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UTextBlock> SelectedPriceText;

	// BUY / EQUIP / EQUIPPED 액션 버튼 영역입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> ActionButtonPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UButton> ActionButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UTextBlock> ActionButtonText;

private:
	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandlePrevItemClicked();

	UFUNCTION()
	void HandleNextItemClicked();

	UFUNCTION()
	void HandleActionClicked();

	UFUNCTION()
	void HandleCharactersTabClicked();

	UFUNCTION()
	void HandleMapsTabClicked();

	UFUNCTION()
	void HandleReceiptAddClicked();

	UFUNCTION()
	void HandleReceiptBalanceChanged(int32 NewBalance, FName Reason);

	UFUNCTION()
	void HandleShopOwnershipChanged(FName SkinId);

	UFUNCTION()
	void HandleShopEquippedChanged(FName SkinId);

	UFUNCTION()
	void HandleMapOwnershipChanged(FName MapId);

	UFUNCTION()
	void HandleMapEquippedChanged(FName MapId);

	void SelectCharacterByIndex(int32 NewIndex);
	void SelectCharacterById(FName SkinId);
	void SelectMapByIndex(int32 NewIndex);
	void SelectMapById(FName MapId);
	void SetActiveTab(EPGShopTab NewTab);
	void RebuildActiveItemList();
	void RebuildCharacterList();
	void RebuildMapList();
	void RefreshCharacterItems();
	void RefreshMapItems();
	void RefreshSelectedCharacterState();
	void RefreshSelectedMapState();
	void RefreshActiveTabState();
	void RefreshReceiptBalance();
	void ScrollSelectedCharacterIntoView() const;
	void ScrollSelectedMapIntoView() const;
	void SetShopButtonsEnabled(bool bEnabled) const;
	void ApplyEntryAnimation(float Alpha);
	void ApplyWidgetAnimationState(UWidget* Widget, const FVector2D& StartOffset, float Alpha, float Delay, float Duration, float StartScale = 1.0f, float EndScale = 1.0f) const;
	void ApplyActionFeedback(float DeltaSeconds);

	TWeakObjectPtr<UPGUserDataSubsystem> UserDataSubsystem;
	TSubclassOf<UPGCharacterShopItemWidget> CharacterItemWidgetClass;
	TSubclassOf<UPGMapShopItemWidget> MapItemWidgetClass;

	TArray<TObjectPtr<UPGCharacterShopItemWidget>> CharacterItemWidgets;
	TArray<TObjectPtr<UPGMapShopItemWidget>> MapItemWidgets;
	int32 SelectedCharacterIndex = INDEX_NONE;
	int32 SelectedMapIndex = INDEX_NONE;
	EPGShopTab ActiveTab = EPGShopTab::Characters;

	bool bIsOpening = false;
	float AnimationElapsed = 0.0f;
	float AnimationDuration = 0.56f;

	bool bActionFeedbackActive = false;
	float ActionFeedbackElapsed = 0.0f;
	float ActionFeedbackDuration = 0.22f;
	bool bActionFeedbackSuccess = true;
	bool bExternalInteractionEnabled = true;
};
