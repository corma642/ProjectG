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

/** 상점 닫기 전환을 Home 흐름에 요청합니다. */
DECLARE_MULTICAST_DELEGATE(FPGShopCloseRequestedSignature);
/** 영수증 상점 모달 생성을 Home 흐름에 요청합니다. */
DECLARE_MULTICAST_DELEGATE(FPGShopReceiptShopRequestedSignature);
/** 장착한 맵 ID에 대응하는 레벨 이동을 Home 흐름에 요청합니다. */
DECLARE_MULTICAST_DELEGATE_OneParam(FPGShopMapTravelRequestedSignature, FName);

/** 상점의 공용 카드·상세 영역에 표시할 상품 도메인입니다. */
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
	/** 상점 닫기 애니메이션이 요청되면 Home 흐름에 화면 전환을 위임합니다. */
	FPGShopCloseRequestedSignature OnShopCloseRequested;

	/** 영수증 충전 화면을 열어야 할 때 Home 흐름에 모달 생성을 요청합니다. */
	FPGShopReceiptShopRequestedSignature OnReceiptShopRequested;

	/** 장착한 맵의 레벨로 이동해야 할 때 MapId를 Home 흐름에 전달합니다. */
	FPGShopMapTravelRequestedSignature OnMapTravelRequested;

	/** 유저 데이터와 카드 위젯 클래스를 연결하고 현재 장착 상품을 선택 상태로 복구합니다. */
	void InitializeShop(UPGUserDataSubsystem* InUserDataSubsystem, TSubclassOf<UPGCharacterShopItemWidget> InCharacterItemWidgetClass, TSubclassOf<UPGMapShopItemWidget> InMapItemWidgetClass);

	/** BP 레이아웃을 변경하지 않고 C++ 진입 애니메이션을 처음부터 재생합니다. */
	void PlayOpenAnimation();

	/** 뒤로가기처럼 외부 요청으로 닫기 버튼과 같은 동작(버튼 잠금·닫기 요청 알림)을 수행합니다. */
	void RequestClose();

	/** 상위 화면 전환이나 모달 표시 중 상점의 모든 사용자 입력을 일괄 잠급니다. */
	void SetShopInteractionEnabled(bool bEnabled);

protected:
	/** BP가 생성된 뒤 필수 버튼 delegate와 애니메이션 초기 상태를 연결합니다. */
	virtual void NativeConstruct() override;

	/** 위젯 파괴 전에 버튼과 유저 데이터 delegate를 해제해 재진입 시 중복 호출을 방지합니다. */
	virtual void NativeDestruct() override;

	/** 진입 및 짧은 구매 피드백이 재생되는 동안에만 해당 애니메이션 상태를 갱신합니다. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// WBP_ShopScreen 전체 루트 패널입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> RootPanel;

	// 상점 화면의 큰 배경 이미지 영역입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> ShopPanel;

	// 탭, 상품 리스트, 선택 정보, 액션 버튼을 담는 공용 콘텐츠 패널입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> ShopContentPanel;

	// 상점을 닫고 Home 화면으로 돌아가는 버튼입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UButton> CloseButton;

	// 현재 보유 영수증을 표시하는 상단 패널입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> ReceiptPanel;

	/** 저장된 누적 영수증 잔액을 표시하는 필수 텍스트입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UTextBlock> ReceiptBalanceText;

	/** 영수증 상점 모달을 여는 필수 추가 버튼입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UButton> ReceiptAddButton;

	// Characters / Maps 상품 종류를 전환하는 탭 버튼 묶음입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> TabPanel;

	/** 캐릭터 상품 목록과 월드 프리뷰를 선택하는 필수 탭 버튼입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UButton> CharactersTabButton;

	/** 맵 상품 목록과 중앙 썸네일을 선택하는 필수 탭 버튼입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UButton> MapsTabButton;

	// 현재 탭의 선택 상품을 이전/다음으로 넘기는 보조 버튼 영역입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> ItemNavPanel;

	/** 현재 선택보다 앞선 상품으로 이동하는 필수 버튼입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UButton> PrevItemButton;

	/** 현재 선택보다 뒤의 상품으로 이동하는 필수 버튼입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UButton> NextItemButton;

	// 현재 탭의 상품 카드 ScrollBox를 감싸는 공용 패널입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> ItemListPanel;

	/** 현재 탭의 카드 위젯을 자식으로 보유하고 선택 카드로 스크롤하는 필수 목록입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UScrollBox> ItemListBox;

	// 맵 탭에서 월드 프리뷰 대신 표시하는 중앙 썸네일입니다.
	/** 맵 탭에서 선택한 DataAsset 썸네일을 표시하는 필수 이미지입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UImage> SelectedMapThumbnailImage;

	// 현재 선택된 캐릭터 또는 맵 이름을 보여주는 상세 영역입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> SelectedNamePanel;

	/** 선택한 캐릭터 또는 맵의 표시 이름을 보여주는 필수 텍스트입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UTextBlock> SelectedItemNameText;

	// 미보유 상품 선택 시 가격과 영수증 아이콘을 보여주는 상세 가격 패널입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> SelectedPricePanel;

	/** 미보유 상품의 영수증 가격을 표시하는 필수 텍스트입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UTextBlock> SelectedPriceText;

	// BUY / EQUIP / EQUIPPED 액션 버튼 영역입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> ActionButtonPanel;

	/** 현재 상품 상태에 따라 구매 또는 장착 동작을 실행하는 필수 버튼입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UButton> ActionButton;

	/** 액션 버튼 문구를 표시하는 필수 텍스트입니다. 캐릭터는 BUY/EQUIP/EQUIPPED, 맵은 BUY/HOLDED/SELECTED입니다. */
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

	void LockActionButtonAfterPurchase(bool bPurchased);

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
	void ClearSelectedItemPresentation(bool bClearMapThumbnail) const;
	void ApplySelectedItemPresentation(const FText& DisplayName, int32 PriceReceipts, bool bOwned, bool bEquipped, int32 SelectedIndex, int32 ItemCount, const FText& BuyText, const FText& EquipText, const FText& EquippedText) const;
	void ApplyActionResultFeedback(bool bSuccess, bool bWasPurchaseAction);
	void ScrollSelectedCharacterIntoView() const;
	void ScrollSelectedMapIntoView() const;
	void SetShopButtonsEnabled(bool bEnabled) const;
	void ApplyEntryAnimation(float Alpha);
	void ApplyWidgetAnimationState(UWidget* Widget, const FVector2D& StartOffset, float Alpha, float Delay, float Duration, float StartScale = 1.0f, float EndScale = 1.0f) const;
	void StartActionFeedback(UWidget* TargetWidget, bool bSuccess);
	void ApplyActionFeedback(float DeltaSeconds);

	TWeakObjectPtr<UPGUserDataSubsystem> UserDataSubsystem;

	// 카드 생성 중 클래스가 GC되지 않도록 상점 수명 동안 강한 참조로 유지합니다.
	UPROPERTY(Transient)
	TSubclassOf<UPGCharacterShopItemWidget> CharacterItemWidgetClass;

	UPROPERTY(Transient)
	TSubclassOf<UPGMapShopItemWidget> MapItemWidgetClass;

	// ScrollBox가 수명을 소유하는 카드를 관찰하는 캐시입니다. 탭 재구성 시 두 배열을 함께 비웁니다.
	TArray<TWeakObjectPtr<UPGCharacterShopItemWidget>> CharacterItemWidgets;
	TArray<TWeakObjectPtr<UPGMapShopItemWidget>> MapItemWidgets;
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
	TWeakObjectPtr<UWidget> ActionFeedbackTarget;
	bool bExternalInteractionEnabled = true;
	bool bCharactersTabAvailable = false;
	bool bMapsTabAvailable = false;
};
