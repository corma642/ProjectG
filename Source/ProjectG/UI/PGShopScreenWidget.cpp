// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PGShopScreenWidget.h"

#include "Audio/PGAudioStatics.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "Settings/PGGameSettings.h"
#include "Shop/PGCharacterSkinDefinition.h"
#include "Shop/PGMapDefinition.h"
#include "UI/PGCharacterShopItemWidget.h"
#include "UI/PGMapShopItemWidget.h"
#include "UI/PGUIAnimationUtils.h"
#include "User/PGUserDataSubsystem.h"

namespace
{
constexpr float ShopPanelStartYOffset = 540.0f;
constexpr float ShopContentStartYOffset = 120.0f;
constexpr float ShopTopBarStartYOffset = -140.0f;
constexpr float ShopReceiptStartXOffset = -260.0f;
constexpr float ShopCloseStartXOffset = 180.0f;
constexpr float ShopListStartYOffset = 260.0f;
constexpr float ShopActionStartYOffset = 210.0f;
// 진입 애니메이션 전체 알파(0~1) 안에서 각 패널이 움직이는 구간 길이. 초가 아니라 정규화 비율이며 Delay와 더해 1을 넘지 않는다.
constexpr float ShopPanelAnimationDuration = 0.5f;
constexpr float ShopElementAnimationDuration = 0.42f;

}

void UPGShopScreenWidget::InitializeShop(UPGUserDataSubsystem* InUserDataSubsystem, TSubclassOf<UPGCharacterShopItemWidget> InCharacterItemWidgetClass, TSubclassOf<UPGMapShopItemWidget> InMapItemWidgetClass)
{
	UserDataSubsystem = InUserDataSubsystem;
	CharacterItemWidgetClass = InCharacterItemWidgetClass;
	MapItemWidgetClass = InMapItemWidgetClass;
	bCharactersTabAvailable = CharacterItemWidgetClass && InUserDataSubsystem && InUserDataSubsystem->GetCharacterCatalog();
	bMapsTabAvailable = MapItemWidgetClass && InUserDataSubsystem && InUserDataSubsystem->GetMapCatalog();
	if (!bCharactersTabAvailable && bMapsTabAvailable)
	{
		ActiveTab = EPGShopTab::Maps;
	}

	if (UPGUserDataSubsystem* UserData = UserDataSubsystem.Get())
	{
		UserData->OnReceiptBalanceChanged.AddDynamic(this, &UPGShopScreenWidget::HandleReceiptBalanceChanged);
		UserData->OnCharacterOwnershipChanged.AddDynamic(this, &UPGShopScreenWidget::HandleShopOwnershipChanged);
		UserData->OnEquippedCharacterChanged.AddDynamic(this, &UPGShopScreenWidget::HandleShopEquippedChanged);
		UserData->OnMapOwnershipChanged.AddDynamic(this, &UPGShopScreenWidget::HandleMapOwnershipChanged);
		UserData->OnEquippedMapChanged.AddDynamic(this, &UPGShopScreenWidget::HandleMapEquippedChanged);
	}

	RebuildActiveItemList();
	RefreshActiveTabState();
	RefreshReceiptBalance();
}

void UPGShopScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (CloseButton)
	{
		CloseButton->OnClicked.AddDynamic(this, &UPGShopScreenWidget::HandleCloseClicked);
	}
	if (PrevItemButton)
	{
		PrevItemButton->OnClicked.AddDynamic(this, &UPGShopScreenWidget::HandlePrevItemClicked);
	}
	if (NextItemButton)
	{
		NextItemButton->OnClicked.AddDynamic(this, &UPGShopScreenWidget::HandleNextItemClicked);
	}
	if (ActionButton)
	{
		ActionButton->OnClicked.AddDynamic(this, &UPGShopScreenWidget::HandleActionClicked);
	}
	if (ReceiptAddButton)
	{
		ReceiptAddButton->OnClicked.AddDynamic(this, &UPGShopScreenWidget::HandleReceiptAddClicked);

		// 테스트 지급 상점이 닫힌 빌드(Shipping 포함)에서는 눌러도 열 화면이 없으므로 버튼 자체를 숨긴다. 실결제 도입 시 IAP 진입으로 되살린다.
		const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
		if (!Settings || !Settings->Shop.IsReceiptShopTestGrantEnabled())
		{
			ReceiptAddButton->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	if (MapsTabButton)
	{
		MapsTabButton->OnClicked.AddDynamic(this, &UPGShopScreenWidget::HandleMapsTabClicked);
	}
	if (CharactersTabButton)
	{
		CharactersTabButton->OnClicked.AddDynamic(this, &UPGShopScreenWidget::HandleCharactersTabClicked);
	}

	ApplyEntryAnimation(0.0f);
	SetShopButtonsEnabled(false);
}

void UPGShopScreenWidget::NativeDestruct()
{
	if (CloseButton)
	{
		CloseButton->OnClicked.RemoveDynamic(this, &UPGShopScreenWidget::HandleCloseClicked);
	}
	if (PrevItemButton)
	{
		PrevItemButton->OnClicked.RemoveDynamic(this, &UPGShopScreenWidget::HandlePrevItemClicked);
	}
	if (NextItemButton)
	{
		NextItemButton->OnClicked.RemoveDynamic(this, &UPGShopScreenWidget::HandleNextItemClicked);
	}
	if (ActionButton)
	{
		ActionButton->OnClicked.RemoveDynamic(this, &UPGShopScreenWidget::HandleActionClicked);
	}
	if (ReceiptAddButton)
	{
		ReceiptAddButton->OnClicked.RemoveDynamic(this, &UPGShopScreenWidget::HandleReceiptAddClicked);
	}
	if (MapsTabButton)
	{
		MapsTabButton->OnClicked.RemoveDynamic(this, &UPGShopScreenWidget::HandleMapsTabClicked);
	}
	if (CharactersTabButton)
	{
		CharactersTabButton->OnClicked.RemoveDynamic(this, &UPGShopScreenWidget::HandleCharactersTabClicked);
	}

	if (UPGUserDataSubsystem* UserData = UserDataSubsystem.Get())
	{
		UserData->OnReceiptBalanceChanged.RemoveDynamic(this, &UPGShopScreenWidget::HandleReceiptBalanceChanged);
		UserData->OnCharacterOwnershipChanged.RemoveDynamic(this, &UPGShopScreenWidget::HandleShopOwnershipChanged);
		UserData->OnEquippedCharacterChanged.RemoveDynamic(this, &UPGShopScreenWidget::HandleShopEquippedChanged);
		UserData->OnMapOwnershipChanged.RemoveDynamic(this, &UPGShopScreenWidget::HandleMapOwnershipChanged);
		UserData->OnEquippedMapChanged.RemoveDynamic(this, &UPGShopScreenWidget::HandleMapEquippedChanged);
	}

	Super::NativeDestruct();
}

void UPGShopScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bIsOpening)
	{
		AnimationElapsed += InDeltaTime;
		const float RawAlpha = AnimationDuration > KINDA_SMALL_NUMBER ? FMath::Clamp(AnimationElapsed / AnimationDuration, 0.0f, 1.0f) : 1.0f;
		const float Alpha = RawAlpha;

		// Shop은 카메라 전환 화면이므로 닫힘은 HomeFlow의 전역 Fade가 담당한다.
		// 위젯 자체는 진입 애니메이션만 재생해 중복 전환 연출을 피한다.
		ApplyEntryAnimation(Alpha);

		if (RawAlpha >= 1.0f)
		{
			bIsOpening = false;
			SetShopButtonsEnabled(true);
			RefreshActiveTabState();
		}
	}

	ApplyActionFeedback(InDeltaTime);
}

void UPGShopScreenWidget::PlayOpenAnimation()
{
	bIsOpening = true;
	AnimationElapsed = 0.0f;
	SetShopButtonsEnabled(false);
	ApplyEntryAnimation(0.0f);
}

void UPGShopScreenWidget::SetShopInteractionEnabled(bool bEnabled)
{
	bExternalInteractionEnabled = bEnabled;
	SetShopButtonsEnabled(bEnabled && !bIsOpening);
	RefreshActiveTabState();
}

void UPGShopScreenWidget::HandleCloseClicked()
{
	UPGAudioStatics::PlayCommonButtonClick(this);
	SetShopButtonsEnabled(false);
	OnShopCloseRequested.Broadcast();
}

void UPGShopScreenWidget::HandlePrevItemClicked()
{
	UPGAudioStatics::PlayCommonButtonClick(this);
	if (ActiveTab == EPGShopTab::Maps)
	{
		SelectMapByIndex(SelectedMapIndex - 1);
		return;
	}
	SelectCharacterByIndex(SelectedCharacterIndex - 1);
}

void UPGShopScreenWidget::HandleNextItemClicked()
{
	UPGAudioStatics::PlayCommonButtonClick(this);
	if (ActiveTab == EPGShopTab::Maps)
	{
		SelectMapByIndex(SelectedMapIndex + 1);
		return;
	}
	SelectCharacterByIndex(SelectedCharacterIndex + 1);
}

void UPGShopScreenWidget::HandleActionClicked()
{
	UPGUserDataSubsystem* UserData = UserDataSubsystem.Get();
	if (!UserData)
	{
		UPGAudioStatics::PlayCommonButtonClick(this);
		return;
	}

	if (ActiveTab == EPGShopTab::Maps)
	{
		const auto& Maps = UserData->GetMaps();
		if (!Maps.IsValidIndex(SelectedMapIndex) || !Maps[SelectedMapIndex])
		{
			UPGAudioStatics::PlayCommonButtonClick(this);
			return;
		}

		const FName MapId = Maps[SelectedMapIndex]->MapId;
		bool bSuccess = false;
		bool bWasPurchaseAction = false;
		if (!UserData->IsMapOwned(MapId))
		{
			bWasPurchaseAction = true;
			bSuccess = UserData->PurchaseMap(MapId);
		}
		else if (!UserData->IsMapEquipped(MapId))
		{
			bSuccess = UserData->EquipMap(MapId);
			if (bSuccess)
			{
				OnMapTravelRequested.Broadcast(MapId);
			}
		}
		else
		{
			bSuccess = true;
		}

		ApplyActionResultFeedback(bSuccess, bWasPurchaseAction);
		RefreshSelectedMapState();
		LockActionButtonAfterPurchase(bSuccess && bWasPurchaseAction);
		return;
	}

	const auto& CharacterSkins = UserData->GetCharacterSkins();
	if (!CharacterSkins.IsValidIndex(SelectedCharacterIndex) || !CharacterSkins[SelectedCharacterIndex])
	{
		UPGAudioStatics::PlayCommonButtonClick(this);
		return;
	}

	const FName SkinId = CharacterSkins[SelectedCharacterIndex]->SkinId;
	bool bSuccess = false;
	bool bWasPurchaseAction = false;
	if (!UserData->IsCharacterOwned(SkinId))
	{
		// 구매 액션만 전용 성공/실패 사운드를 사용한다. 장착은 일반 UI 클릭음으로 유지한다.
		bWasPurchaseAction = true;
		bSuccess = UserData->PurchaseCharacter(SkinId);
	}
	else if (!UserData->IsCharacterEquipped(SkinId))
	{
		bSuccess = UserData->EquipCharacter(SkinId);
	}
	else
	{
		bSuccess = true;
	}

	ApplyActionResultFeedback(bSuccess, bWasPurchaseAction);
	RefreshSelectedCharacterState();
	LockActionButtonAfterPurchase(bSuccess && bWasPurchaseAction);
}

void UPGShopScreenWidget::LockActionButtonAfterPurchase(bool bPurchased)
{
	// 구매 직후 같은 버튼이 곧바로 장착(맵은 레벨 이동)으로 바뀌므로, 피드백 펄스가 끝날 때까지 연타를 막는다.
	if (bPurchased && ActionButton)
	{
		ActionButton->SetIsEnabled(false);
	}
}

void UPGShopScreenWidget::HandleCharactersTabClicked()
{
	UPGAudioStatics::PlayCommonButtonClick(this);
	SetActiveTab(EPGShopTab::Characters);
}

void UPGShopScreenWidget::HandleMapsTabClicked()
{
	UPGAudioStatics::PlayCommonButtonClick(this);
	SetActiveTab(EPGShopTab::Maps);
}

void UPGShopScreenWidget::HandleReceiptAddClicked()
{
	UPGAudioStatics::PlayCommonButtonClick(this);
	OnReceiptShopRequested.Broadcast();
}

void UPGShopScreenWidget::HandleReceiptBalanceChanged(int32 NewBalance, FName Reason)
{
	RefreshReceiptBalance();
	RefreshActiveTabState();
}

void UPGShopScreenWidget::HandleShopOwnershipChanged(FName SkinId)
{
	RefreshCharacterItems();
	SelectCharacterById(SkinId);
}

void UPGShopScreenWidget::HandleShopEquippedChanged(FName SkinId)
{
	RefreshCharacterItems();
	SelectCharacterById(SkinId);
}

void UPGShopScreenWidget::HandleMapOwnershipChanged(FName MapId)
{
	RefreshMapItems();
	SelectMapById(MapId);
}

void UPGShopScreenWidget::HandleMapEquippedChanged(FName MapId)
{
	RefreshMapItems();
	SelectMapById(MapId);
}

void UPGShopScreenWidget::SelectCharacterByIndex(int32 NewIndex)
{
	UPGUserDataSubsystem* UserData = UserDataSubsystem.Get();
	if (!UserData)
	{
		return;
	}

	const int32 CharacterCount = UserData->GetCharacterSkins().Num();
	if (CharacterCount <= 0)
	{
		return;
	}

	SelectedCharacterIndex = FMath::Clamp(NewIndex, 0, CharacterCount - 1);
	const UPGCharacterSkinDefinition* SelectedSkin = UserData->GetCharacterSkins()[SelectedCharacterIndex];
	if (SelectedSkin)
	{
		// 선택값은 위젯 인스턴스 수명보다 길게 유지한다. Shop을 닫았다 다시 열어도
		// 마지막으로 보던 카드를 기준으로 Prev/Next가 이어지도록 Subsystem에 세션 상태를 맡긴다.
		UserData->SetLastSelectedCharacterSkinId(SelectedSkin->SkinId);
		UserData->PreviewCharacter(SelectedSkin->SkinId);
	}
	RefreshSelectedCharacterState();
	RefreshCharacterItems();
	ScrollSelectedCharacterIntoView();
}

void UPGShopScreenWidget::SelectCharacterById(FName SkinId)
{
	UPGUserDataSubsystem* UserData = UserDataSubsystem.Get();
	if (!UserData)
	{
		return;
	}

	const auto& CharacterSkins = UserData->GetCharacterSkins();
	for (int32 Index = 0; Index < CharacterSkins.Num(); ++Index)
	{
		if (CharacterSkins[Index] && CharacterSkins[Index]->SkinId == SkinId)
		{
			SelectCharacterByIndex(Index);
			return;
		}
	}
}

void UPGShopScreenWidget::SelectMapByIndex(int32 NewIndex)
{
	UPGUserDataSubsystem* UserData = UserDataSubsystem.Get();
	if (!UserData)
	{
		return;
	}

	const int32 MapCount = UserData->GetMaps().Num();
	if (MapCount <= 0)
	{
		return;
	}

	SelectedMapIndex = FMath::Clamp(NewIndex, 0, MapCount - 1);
	const UPGMapDefinition* SelectedMap = UserData->GetMaps()[SelectedMapIndex];
	if (SelectedMap)
	{
		UserData->SetLastSelectedMapId(SelectedMap->MapId);
	}
	RefreshSelectedMapState();
	RefreshMapItems();
	ScrollSelectedMapIntoView();
}

void UPGShopScreenWidget::SelectMapById(FName MapId)
{
	UPGUserDataSubsystem* UserData = UserDataSubsystem.Get();
	if (!UserData)
	{
		return;
	}

	const auto& Maps = UserData->GetMaps();
	for (int32 Index = 0; Index < Maps.Num(); ++Index)
	{
		if (Maps[Index] && Maps[Index]->MapId == MapId)
		{
			SelectMapByIndex(Index);
			return;
		}
	}
}

void UPGShopScreenWidget::SetActiveTab(EPGShopTab NewTab)
{
	if ((NewTab == EPGShopTab::Characters && !bCharactersTabAvailable)
		|| (NewTab == EPGShopTab::Maps && !bMapsTabAvailable))
	{
		return;
	}

	if (ActiveTab == NewTab)
	{
		RefreshActiveTabState();
		return;
	}

	ActiveTab = NewTab;
	RebuildActiveItemList();
	RefreshActiveTabState();
}

void UPGShopScreenWidget::RebuildActiveItemList()
{
	if (ItemListBox)
	{
		ItemListBox->ClearChildren();
	}
	CharacterItemWidgets.Reset();
	MapItemWidgets.Reset();

	if (ActiveTab == EPGShopTab::Maps)
	{
		RebuildMapList();
	}
	else
	{
		RebuildCharacterList();
	}
}

void UPGShopScreenWidget::RebuildCharacterList()
{
	UPGUserDataSubsystem* UserData = UserDataSubsystem.Get();
	if (!UserData || !ItemListBox || !CharacterItemWidgetClass)
	{
		return;
	}

	CharacterItemWidgets.Reset();

	const auto& CharacterSkins = UserData->GetCharacterSkins();
	if (CharacterSkins.Num() > 0)
	{
		if (SelectedCharacterIndex == INDEX_NONE)
		{
			// Shop을 다시 열 때는 세션에서 마지막으로 보던 카드, 없으면 장착 캐릭터, 없으면 첫 카드 순서로 복구한다.
			const auto FindSkinIndex = [&CharacterSkins](FName SkinId) -> int32
			{
				if (SkinId.IsNone())
				{
					return INDEX_NONE;
				}

				for (int32 Index = 0; Index < CharacterSkins.Num(); ++Index)
				{
					if (CharacterSkins[Index] && CharacterSkins[Index]->SkinId == SkinId)
					{
						return Index;
					}
				}
				return INDEX_NONE;
			};

			SelectedCharacterIndex = FindSkinIndex(UserData->GetLastSelectedCharacterSkinId());
			if (SelectedCharacterIndex == INDEX_NONE)
			{
				const UPGCharacterSkinDefinition* EquippedSkin = UserData->GetEquippedCharacterSkin();
				SelectedCharacterIndex = EquippedSkin ? FindSkinIndex(EquippedSkin->SkinId) : INDEX_NONE;
			}
			if (SelectedCharacterIndex == INDEX_NONE)
			{
				SelectedCharacterIndex = 0;
			}
		}
		else
		{
			SelectedCharacterIndex = FMath::Clamp(SelectedCharacterIndex, 0, CharacterSkins.Num() - 1);
		}
	}
	else
	{
		SelectedCharacterIndex = INDEX_NONE;
	}

	for (int32 Index = 0; Index < CharacterSkins.Num(); ++Index)
	{
		UPGCharacterSkinDefinition* SkinDefinition = CharacterSkins[Index];
		if (!SkinDefinition)
		{
			continue;
		}

		UPGCharacterShopItemWidget* ItemWidget = CreateWidget<UPGCharacterShopItemWidget>(GetOwningPlayer(), CharacterItemWidgetClass);
		if (!ItemWidget)
		{
			continue;
		}

		const bool bOwned = UserData->IsCharacterOwned(SkinDefinition->SkinId);
		const bool bEquipped = UserData->IsCharacterEquipped(SkinDefinition->SkinId);
		const bool bSelected = Index == SelectedCharacterIndex;

		// 리스트 위젯은 탭 전환이나 최초 구성 때만 만든다. 이후 선택/구매/장착 상태 변화는
		// RefreshCharacterItems()에서 기존 카드의 상태만 갱신해 ScrollBox 관성 입력을 보존한다.
		ItemWidget->SetCharacterSkin(SkinDefinition, bOwned, bEquipped, bSelected);
		ItemWidget->OnCharacterSelected.AddUObject(this, &UPGShopScreenWidget::SelectCharacterById);
		ItemListBox->AddChild(ItemWidget);
		CharacterItemWidgets.Add(ItemWidget);
	}

	RefreshCharacterItems();
	if (const UPGCharacterSkinDefinition* SelectedSkin = CharacterSkins.IsValidIndex(SelectedCharacterIndex) ? CharacterSkins[SelectedCharacterIndex] : nullptr)
	{
		// 리스트 생성 직후에도 월드 러너 프리뷰와 UI 선택 상태가 같은 스킨을 가리키게 맞춘다.
		UserData->SetLastSelectedCharacterSkinId(SelectedSkin->SkinId);
		UserData->PreviewCharacter(SelectedSkin->SkinId);
	}
	RefreshSelectedCharacterState();
	ScrollSelectedCharacterIntoView();
}

void UPGShopScreenWidget::RebuildMapList()
{
	UPGUserDataSubsystem* UserData = UserDataSubsystem.Get();
	if (!UserData || !ItemListBox || !MapItemWidgetClass)
	{
		return;
	}

	MapItemWidgets.Reset();

	const auto& Maps = UserData->GetMaps();
	if (Maps.Num() > 0)
	{
		if (SelectedMapIndex == INDEX_NONE)
		{
			const auto FindMapIndex = [&Maps](FName MapId) -> int32
			{
				if (MapId.IsNone())
				{
					return INDEX_NONE;
				}

				for (int32 Index = 0; Index < Maps.Num(); ++Index)
				{
					if (Maps[Index] && Maps[Index]->MapId == MapId)
					{
						return Index;
					}
				}
				return INDEX_NONE;
			};

			SelectedMapIndex = FindMapIndex(UserData->GetLastSelectedMapId());
			if (SelectedMapIndex == INDEX_NONE)
			{
				const UPGMapDefinition* EquippedMap = UserData->GetEquippedMap();
				SelectedMapIndex = EquippedMap ? FindMapIndex(EquippedMap->MapId) : INDEX_NONE;
			}
			if (SelectedMapIndex == INDEX_NONE)
			{
				SelectedMapIndex = 0;
			}
		}
		else
		{
			SelectedMapIndex = FMath::Clamp(SelectedMapIndex, 0, Maps.Num() - 1);
		}
	}
	else
	{
		SelectedMapIndex = INDEX_NONE;
	}

	for (int32 Index = 0; Index < Maps.Num(); ++Index)
	{
		UPGMapDefinition* MapDefinition = Maps[Index];
		if (!MapDefinition)
		{
			continue;
		}

		UPGMapShopItemWidget* ItemWidget = CreateWidget<UPGMapShopItemWidget>(GetOwningPlayer(), MapItemWidgetClass);
		if (!ItemWidget)
		{
			continue;
		}

		const bool bOwned = UserData->IsMapOwned(MapDefinition->MapId);
		const bool bEquipped = UserData->IsMapEquipped(MapDefinition->MapId);
		const bool bSelected = Index == SelectedMapIndex;
		ItemWidget->SetMapDefinition(MapDefinition, bOwned, bEquipped, bSelected);
		ItemWidget->OnMapSelected.AddUObject(this, &UPGShopScreenWidget::SelectMapById);
		ItemListBox->AddChild(ItemWidget);
		MapItemWidgets.Add(ItemWidget);
	}

	if (const UPGMapDefinition* SelectedMap = Maps.IsValidIndex(SelectedMapIndex) ? Maps[SelectedMapIndex] : nullptr)
	{
		UserData->SetLastSelectedMapId(SelectedMap->MapId);
	}
	RefreshMapItems();
	RefreshSelectedMapState();
	ScrollSelectedMapIntoView();
}

void UPGShopScreenWidget::RefreshCharacterItems()
{
	UPGUserDataSubsystem* UserData = UserDataSubsystem.Get();
	if (!UserData)
	{
		return;
	}

	const auto& CharacterSkins = UserData->GetCharacterSkins();
	for (int32 Index = 0; Index < CharacterItemWidgets.Num(); ++Index)
	{
		UPGCharacterShopItemWidget* ItemWidget = CharacterItemWidgets[Index].Get();
		const UPGCharacterSkinDefinition* SkinDefinition = CharacterSkins.IsValidIndex(Index) ? CharacterSkins[Index] : nullptr;
		if (!ItemWidget || !SkinDefinition)
		{
			continue;
		}

		const bool bOwned = UserData->IsCharacterOwned(SkinDefinition->SkinId);
		const bool bEquipped = UserData->IsCharacterEquipped(SkinDefinition->SkinId);
		const bool bSelected = Index == SelectedCharacterIndex;
		ItemWidget->SetCharacterSkin(SkinDefinition, bOwned, bEquipped, bSelected);
	}
}

void UPGShopScreenWidget::RefreshMapItems()
{
	UPGUserDataSubsystem* UserData = UserDataSubsystem.Get();
	if (!UserData)
	{
		return;
	}

	const auto& Maps = UserData->GetMaps();
	for (int32 Index = 0; Index < MapItemWidgets.Num(); ++Index)
	{
		UPGMapShopItemWidget* ItemWidget = MapItemWidgets[Index].Get();
		const UPGMapDefinition* MapDefinition = Maps.IsValidIndex(Index) ? Maps[Index] : nullptr;
		if (!ItemWidget || !MapDefinition)
		{
			continue;
		}

		const bool bOwned = UserData->IsMapOwned(MapDefinition->MapId);
		const bool bEquipped = UserData->IsMapEquipped(MapDefinition->MapId);
		const bool bSelected = Index == SelectedMapIndex;
		ItemWidget->SetMapDefinition(MapDefinition, bOwned, bEquipped, bSelected);
	}
}

void UPGShopScreenWidget::RefreshSelectedCharacterState()
{
	UPGUserDataSubsystem* UserData = UserDataSubsystem.Get();
	if (!UserData)
	{
		return;
	}

	const auto& CharacterSkins = UserData->GetCharacterSkins();
	const UPGCharacterSkinDefinition* SelectedSkin = CharacterSkins.IsValidIndex(SelectedCharacterIndex) ? CharacterSkins[SelectedCharacterIndex] : nullptr;
	if (!SelectedSkin)
	{
		ClearSelectedItemPresentation(false);
		return;
	}

	const bool bOwned = UserData->IsCharacterOwned(SelectedSkin->SkinId);
	const bool bEquipped = UserData->IsCharacterEquipped(SelectedSkin->SkinId);
	ApplySelectedItemPresentation(
		SelectedSkin->DisplayName,
		SelectedSkin->PriceReceipts,
		bOwned,
		bEquipped,
		SelectedCharacterIndex,
		CharacterSkins.Num(),
		NSLOCTEXT("ProjectGShop", "BuyCharacter", "BUY"),
		NSLOCTEXT("ProjectGShop", "EquipCharacter", "EQUIP"),
		NSLOCTEXT("ProjectGShop", "EquippedCharacter", "EQUIPPED"));
}

void UPGShopScreenWidget::RefreshSelectedMapState()
{
	UPGUserDataSubsystem* UserData = UserDataSubsystem.Get();
	if (!UserData)
	{
		return;
	}

	const auto& Maps = UserData->GetMaps();
	const UPGMapDefinition* SelectedMap = Maps.IsValidIndex(SelectedMapIndex) ? Maps[SelectedMapIndex] : nullptr;
	if (!SelectedMap)
	{
		ClearSelectedItemPresentation(true);
		return;
	}

	if (SelectedMapThumbnailImage)
	{
		SelectedMapThumbnailImage->SetBrushFromTexture(SelectedMap->Thumbnail.Get());
	}

	const bool bOwned = UserData->IsMapOwned(SelectedMap->MapId);
	const bool bEquipped = UserData->IsMapEquipped(SelectedMap->MapId);
	ApplySelectedItemPresentation(
		SelectedMap->DisplayName,
		SelectedMap->PriceReceipts,
		bOwned,
		bEquipped,
		SelectedMapIndex,
		Maps.Num(),
		NSLOCTEXT("ProjectGShop", "BuyMap", "BUY"),
		NSLOCTEXT("ProjectGShop", "HeldMap", "HOLDED"),
		NSLOCTEXT("ProjectGShop", "SelectedMap", "SELECTED"));
}

void UPGShopScreenWidget::ClearSelectedItemPresentation(bool bClearMapThumbnail) const
{
	if (SelectedItemNameText)
	{
		SelectedItemNameText->SetText(FText::GetEmpty());
	}
	if (bClearMapThumbnail && SelectedMapThumbnailImage)
	{
		SelectedMapThumbnailImage->SetBrushFromTexture(nullptr);
	}
	if (SelectedPricePanel)
	{
		SelectedPricePanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (SelectedPriceText)
	{
		SelectedPriceText->SetText(FText::GetEmpty());
	}
	if (ActionButton)
	{
		ActionButton->SetIsEnabled(false);
	}
}

void UPGShopScreenWidget::ApplySelectedItemPresentation(
	const FText& DisplayName,
	int32 PriceReceipts,
	bool bOwned,
	bool bEquipped,
	int32 SelectedIndex,
	int32 ItemCount,
	const FText& BuyText,
	const FText& EquipText,
	const FText& EquippedText) const
{
	if (SelectedItemNameText)
	{
		SelectedItemNameText->SetText(DisplayName);
	}
	if (SelectedPricePanel)
	{
		// 가격은 미보유 상품의 상세 영역에서만 보여준다. 카드 내부 가격을 반복하지 않아 세로 화면 가독성을 지킨다.
		SelectedPricePanel->SetVisibility(!bOwned ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (SelectedPriceText)
	{
		SelectedPriceText->SetText(!bOwned ? FText::AsNumber(FMath::Max(0, PriceReceipts)) : FText::GetEmpty());
	}
	if (ActionButtonText)
	{
		ActionButtonText->SetText(!bOwned ? BuyText : (!bEquipped ? EquipText : EquippedText));
	}

	const bool bCanInteract = !bIsOpening && bExternalInteractionEnabled;
	if (ActionButton)
	{
		ActionButton->SetIsEnabled(!bEquipped && bCanInteract);
	}
	if (PrevItemButton)
	{
		PrevItemButton->SetIsEnabled(SelectedIndex > 0 && bCanInteract);
	}
	if (NextItemButton)
	{
		NextItemButton->SetIsEnabled(SelectedIndex < ItemCount - 1 && bCanInteract);
	}
	if (ReceiptAddButton)
	{
		ReceiptAddButton->SetIsEnabled(bCanInteract);
	}
}

void UPGShopScreenWidget::ApplyActionResultFeedback(bool bSuccess, bool bWasPurchaseAction)
{
	StartActionFeedback(ActionButton, bSuccess);
	if (!bWasPurchaseAction)
	{
		UPGAudioStatics::PlayCommonButtonClick(this);
		return;
	}

	// 구매 결과음이 지정되어 있으면 공통 버튼음을 겹쳐 재생하지 않는다.
	// 실패도 사용자가 바로 원인을 체감해야 하므로 동일한 경로에서 전용음을 우선한다.
	if (const UPGGameSettings* Settings = GetDefault<UPGGameSettings>())
	{
		UPGAudioStatics::PlayUIButtonSound(this, bSuccess ? Settings->SFX.ShopPurchaseSuccessSound.Get() : Settings->SFX.ShopPurchaseFailSound.Get());
	}
	else
	{
		UPGAudioStatics::PlayCommonButtonClick(this);
	}
}

void UPGShopScreenWidget::RefreshActiveTabState()
{
	const bool bCharactersActive = ActiveTab == EPGShopTab::Characters;
	if (ItemNavPanel)
	{
		ItemNavPanel->SetVisibility(ESlateVisibility::Visible);
	}
	if (ItemListPanel)
	{
		ItemListPanel->SetVisibility(ESlateVisibility::Visible);
	}
	if (SelectedItemNameText)
	{
		SelectedItemNameText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (SelectedMapThumbnailImage)
	{
		SelectedMapThumbnailImage->SetVisibility(bCharactersActive ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	if (CharactersTabButton)
	{
		CharactersTabButton->SetIsEnabled(bCharactersTabAvailable && !bCharactersActive && !bIsOpening && bExternalInteractionEnabled);
	}
	if (MapsTabButton)
	{
		MapsTabButton->SetIsEnabled(bMapsTabAvailable && bCharactersActive && !bIsOpening && bExternalInteractionEnabled);
	}

	if (bCharactersActive)
	{
		RefreshSelectedCharacterState();
		RefreshCharacterItems();
		ScrollSelectedCharacterIntoView();
	}
	else
	{
		RefreshSelectedMapState();
		RefreshMapItems();
		ScrollSelectedMapIntoView();
	}
}

void UPGShopScreenWidget::RefreshReceiptBalance()
{
	if (ReceiptBalanceText)
	{
		const int32 Balance = UserDataSubsystem.IsValid() ? UserDataSubsystem->GetReceiptBalance() : 0;
		ReceiptBalanceText->SetText(FText::AsNumber(FMath::Max(0, Balance)));
	}
}

void UPGShopScreenWidget::ScrollSelectedCharacterIntoView() const
{
	if (!ItemListBox || !CharacterItemWidgets.IsValidIndex(SelectedCharacterIndex))
	{
		return;
	}

	if (UPGCharacterShopItemWidget* SelectedItemWidget = CharacterItemWidgets[SelectedCharacterIndex].Get())
	{
		ItemListBox->ScrollWidgetIntoView(SelectedItemWidget, true, EDescendantScrollDestination::Center);
	}
}

void UPGShopScreenWidget::ScrollSelectedMapIntoView() const
{
	if (!ItemListBox || !MapItemWidgets.IsValidIndex(SelectedMapIndex))
	{
		return;
	}

	if (UPGMapShopItemWidget* SelectedItemWidget = MapItemWidgets[SelectedMapIndex].Get())
	{
		ItemListBox->ScrollWidgetIntoView(SelectedItemWidget, true, EDescendantScrollDestination::Center);
	}
}

void UPGShopScreenWidget::SetShopButtonsEnabled(bool bEnabled) const
{
	const bool bCanInteract = bEnabled && bExternalInteractionEnabled;
	if (CloseButton)
	{
		CloseButton->SetIsEnabled(bCanInteract);
	}
	if (PrevItemButton)
	{
		PrevItemButton->SetIsEnabled(bCanInteract);
	}
	if (NextItemButton)
	{
		NextItemButton->SetIsEnabled(bCanInteract);
	}
	if (ActionButton)
	{
		ActionButton->SetIsEnabled(bCanInteract);
	}
	if (ReceiptAddButton)
	{
		ReceiptAddButton->SetIsEnabled(bCanInteract);
	}
	if (CharactersTabButton)
	{
		CharactersTabButton->SetIsEnabled(bCanInteract && bCharactersTabAvailable && ActiveTab != EPGShopTab::Characters);
	}
	if (MapsTabButton)
	{
		MapsTabButton->SetIsEnabled(bCanInteract && bMapsTabAvailable && ActiveTab != EPGShopTab::Maps);
	}
	for (const TWeakObjectPtr<UPGCharacterShopItemWidget>& ItemWidgetPtr : CharacterItemWidgets)
	{
		UPGCharacterShopItemWidget* ItemWidget = ItemWidgetPtr.Get();
		if (ItemWidget)
		{
			ItemWidget->SetSelectionEnabled(bCanInteract && ActiveTab == EPGShopTab::Characters);
		}
	}
	for (const TWeakObjectPtr<UPGMapShopItemWidget>& ItemWidgetPtr : MapItemWidgets)
	{
		UPGMapShopItemWidget* ItemWidget = ItemWidgetPtr.Get();
		if (ItemWidget)
		{
			ItemWidget->SetSelectionEnabled(bCanInteract && ActiveTab == EPGShopTab::Maps);
		}
	}
}

void UPGShopScreenWidget::ApplyEntryAnimation(float Alpha)
{
	// BP 계층은 상점 배경, 상단 재화, 탭, 리스트, 액션 버튼을 별도 패널로 나눈다.
	// 각 패널에 작은 딜레이를 주면 전체 화면이 한 장으로 뜨지 않고 상품 진열대처럼 단계적으로 열린다.
	ApplyWidgetAnimationState(ShopPanel, FVector2D(0.0f, ShopPanelStartYOffset), Alpha, 0.0f, ShopPanelAnimationDuration, 0.97f, 1.0f);
	ApplyWidgetAnimationState(ShopContentPanel, FVector2D(0.0f, ShopContentStartYOffset), Alpha, 0.06f, ShopElementAnimationDuration, 0.96f, 1.0f);
	ApplyWidgetAnimationState(ReceiptPanel, FVector2D(ShopReceiptStartXOffset, ShopTopBarStartYOffset), Alpha, 0.09f, ShopElementAnimationDuration, 1.0f, 1.0f);
	ApplyWidgetAnimationState(CloseButton, FVector2D(ShopCloseStartXOffset, ShopTopBarStartYOffset), Alpha, 0.1f, ShopElementAnimationDuration, 1.0f, 1.0f);
	ApplyWidgetAnimationState(TabPanel, FVector2D(0.0f, -120.0f), Alpha, 0.12f, ShopElementAnimationDuration, 0.98f, 1.0f);
	ApplyWidgetAnimationState(ItemNavPanel, FVector2D::ZeroVector, Alpha, 0.16f, ShopElementAnimationDuration, 0.9f, 1.0f);
	ApplyWidgetAnimationState(ItemListPanel, FVector2D(0.0f, ShopListStartYOffset), Alpha, 0.18f, ShopElementAnimationDuration, 0.98f, 1.0f);
	ApplyWidgetAnimationState(SelectedNamePanel, FVector2D(0.0f, 70.0f), Alpha, 0.23f, ShopElementAnimationDuration, 0.92f, 1.0f);
	ApplyWidgetAnimationState(SelectedPricePanel, FVector2D(0.0f, 86.0f), Alpha, 0.25f, ShopElementAnimationDuration, 0.92f, 1.0f);
	ApplyWidgetAnimationState(ActionButtonPanel, FVector2D(0.0f, ShopActionStartYOffset), Alpha, 0.29f, ShopElementAnimationDuration, 0.94f, 1.0f);
}

void UPGShopScreenWidget::ApplyWidgetAnimationState(UWidget* Widget, const FVector2D& StartOffset, float Alpha, float Delay, float Duration, float StartScale, float EndScale) const
{
	if (!Widget)
	{
		return;
	}

	const float EffectiveDuration = FMath::Max(KINDA_SMALL_NUMBER, Duration);
	const float LocalAlpha = FMath::Clamp((Alpha - Delay) / EffectiveDuration, 0.0f, 1.0f);
	const float EasedAlpha = PGUIAnimation::EaseOutCubic(LocalAlpha);
	const float ScaleAlpha = PGUIAnimation::EaseOutBack(LocalAlpha);
	const float Scale = FMath::Lerp(StartScale, EndScale, ScaleAlpha);

	// BP가 배치한 패널은 유지하고 RenderTransform만 조작해 디자인 자산을 C++에서 덮어쓰지 않는다.
	Widget->SetRenderOpacity(EasedAlpha);
	Widget->SetRenderTranslation(StartOffset * (1.0f - EasedAlpha));
	Widget->SetRenderScale(FVector2D(Scale, Scale));
}

void UPGShopScreenWidget::StartActionFeedback(UWidget* TargetWidget, bool bSuccess)
{
	if (!TargetWidget)
	{
		return;
	}

	ActionFeedbackTarget = TargetWidget;
	bActionFeedbackActive = true;
	ActionFeedbackElapsed = 0.0f;
	bActionFeedbackSuccess = bSuccess;
	ApplyActionFeedback(0.0f);
}

void UPGShopScreenWidget::ApplyActionFeedback(float DeltaSeconds)
{
	UWidget* TargetWidget = ActionFeedbackTarget.Get();
	if (!bActionFeedbackActive || !TargetWidget)
	{
		return;
	}

	ActionFeedbackElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(ActionFeedbackElapsed / FMath::Max(KINDA_SMALL_NUMBER, ActionFeedbackDuration), 0.0f, 1.0f);
	const float Pulse = FMath::Sin(Alpha * UE_PI);
	if (bActionFeedbackSuccess)
	{
		// 성공은 짧은 펄스로 확정감을 주고, 실패는 흔들림으로 재화 부족/저장 실패를 구분한다.
		const float Scale = 1.0f + Pulse * 0.08f;
		TargetWidget->SetRenderScale(FVector2D(Scale, Scale));
	}
	else
	{
		TargetWidget->SetRenderTranslation(FVector2D(FMath::Sin(Alpha * UE_PI * 6.0f) * 12.0f * (1.0f - Alpha), 0.0f));
	}

	if (Alpha >= 1.0f)
	{
		bActionFeedbackActive = false;
		TargetWidget->SetRenderScale(FVector2D(1.0f, 1.0f));
		TargetWidget->SetRenderTranslation(FVector2D::ZeroVector);
		ActionFeedbackTarget.Reset();

		// 구매 직후 잠근 액션 버튼을 현재 보유/장착 상태 기준으로 다시 계산한다.
		if (ActiveTab == EPGShopTab::Maps)
		{
			RefreshSelectedMapState();
		}
		else
		{
			RefreshSelectedCharacterState();
		}
	}
}
