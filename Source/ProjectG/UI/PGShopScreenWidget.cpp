// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PGShopScreenWidget.h"

#include "Audio/PGAudioStatics.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Settings/PGGameSettings.h"
#include "Shop/PGCharacterSkinDefinition.h"
#include "Shop/PGPlayerShopSubsystem.h"
#include "UI/PGCharacterShopItemWidget.h"
#include "Wallet/PGPlayerWalletSubsystem.h"

namespace
{
constexpr float ShopPanelStartYOffset = 540.0f;
constexpr float ShopContentStartYOffset = 120.0f;
constexpr float ShopTopBarStartYOffset = -140.0f;
constexpr float ShopReceiptStartXOffset = -260.0f;
constexpr float ShopCloseStartXOffset = 180.0f;
constexpr float ShopListStartYOffset = 260.0f;
constexpr float ShopActionStartYOffset = 210.0f;
constexpr float ShopPanelAnimationDuration = 0.5f;
constexpr float ShopElementAnimationDuration = 0.42f;

float ShopEaseOutCubic(float Alpha)
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	const float InverseAlpha = 1.0f - ClampedAlpha;
	return 1.0f - InverseAlpha * InverseAlpha * InverseAlpha;
}

float ShopEaseOutBack(float Alpha)
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	constexpr float Overshoot = 1.70158f;
	const float Shifted = ClampedAlpha - 1.0f;
	return 1.0f + Shifted * Shifted * ((Overshoot + 1.0f) * Shifted + Overshoot);
}
}

void UPGShopScreenWidget::InitializeShop(UPGPlayerShopSubsystem* InShopSubsystem, UPGPlayerWalletSubsystem* InWalletSubsystem, TSubclassOf<UPGCharacterShopItemWidget> InItemWidgetClass)
{
	ShopSubsystem = InShopSubsystem;
	WalletSubsystem = InWalletSubsystem;
	CharacterItemWidgetClass = InItemWidgetClass;

	if (UPGPlayerWalletSubsystem* Wallet = WalletSubsystem.Get())
	{
		Wallet->OnReceiptBalanceChanged.AddDynamic(this, &UPGShopScreenWidget::HandleReceiptBalanceChanged);
	}
	if (UPGPlayerShopSubsystem* Shop = ShopSubsystem.Get())
	{
		Shop->OnCharacterOwnershipChanged.AddDynamic(this, &UPGShopScreenWidget::HandleShopOwnershipChanged);
		Shop->OnEquippedCharacterChanged.AddDynamic(this, &UPGShopScreenWidget::HandleShopEquippedChanged);
	}

	RebuildCharacterList();
	RefreshReceiptBalance();
}

void UPGShopScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (CloseButton)
	{
		CloseButton->OnClicked.AddDynamic(this, &UPGShopScreenWidget::HandleCloseClicked);
	}
	if (PrevCharacterButton)
	{
		PrevCharacterButton->OnClicked.AddDynamic(this, &UPGShopScreenWidget::HandlePrevCharacterClicked);
	}
	if (NextCharacterButton)
	{
		NextCharacterButton->OnClicked.AddDynamic(this, &UPGShopScreenWidget::HandleNextCharacterClicked);
	}
	if (ActionButton)
	{
		ActionButton->OnClicked.AddDynamic(this, &UPGShopScreenWidget::HandleActionClicked);
	}
	if (MapsTabButton)
	{
		MapsTabButton->SetIsEnabled(false);
	}
	if (CharactersTabButton)
	{
		CharactersTabButton->SetIsEnabled(false);
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
	if (PrevCharacterButton)
	{
		PrevCharacterButton->OnClicked.RemoveDynamic(this, &UPGShopScreenWidget::HandlePrevCharacterClicked);
	}
	if (NextCharacterButton)
	{
		NextCharacterButton->OnClicked.RemoveDynamic(this, &UPGShopScreenWidget::HandleNextCharacterClicked);
	}
	if (ActionButton)
	{
		ActionButton->OnClicked.RemoveDynamic(this, &UPGShopScreenWidget::HandleActionClicked);
	}
	if (MapsTabButton)
	{
		MapsTabButton->OnClicked.RemoveDynamic(this, &UPGShopScreenWidget::HandleMapsTabClicked);
	}

	if (UPGPlayerWalletSubsystem* Wallet = WalletSubsystem.Get())
	{
		Wallet->OnReceiptBalanceChanged.RemoveDynamic(this, &UPGShopScreenWidget::HandleReceiptBalanceChanged);
	}
	if (UPGPlayerShopSubsystem* Shop = ShopSubsystem.Get())
	{
		Shop->OnCharacterOwnershipChanged.RemoveDynamic(this, &UPGShopScreenWidget::HandleShopOwnershipChanged);
		Shop->OnEquippedCharacterChanged.RemoveDynamic(this, &UPGShopScreenWidget::HandleShopEquippedChanged);
	}

	Super::NativeDestruct();
}

void UPGShopScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bIsOpening || bIsClosing)
	{
		AnimationElapsed += InDeltaTime;
		const float RawAlpha = AnimationDuration > KINDA_SMALL_NUMBER ? FMath::Clamp(AnimationElapsed / AnimationDuration, 0.0f, 1.0f) : 1.0f;
		const float Alpha = bIsClosing ? 1.0f - RawAlpha : RawAlpha;
		ApplyEntryAnimation(Alpha);

		if (RawAlpha >= 1.0f)
		{
			const bool bWasClosing = bIsClosing;
			bIsOpening = false;
			bIsClosing = false;
			SetShopButtonsEnabled(!bWasClosing);
			if (bWasClosing)
			{
				OnShopScreenClosed.Broadcast();
				RemoveFromParent();
			}
			else
			{
				RefreshSelectedCharacterState();
			}
		}
	}

	ApplyActionFeedback(InDeltaTime);
}

void UPGShopScreenWidget::PlayOpenAnimation()
{
	bIsOpening = true;
	bIsClosing = false;
	AnimationElapsed = 0.0f;
	SetShopButtonsEnabled(false);
	ApplyEntryAnimation(0.0f);
}

void UPGShopScreenWidget::PlayCloseAnimation()
{
	if (bIsClosing)
	{
		return;
	}

	bIsOpening = false;
	bIsClosing = true;
	AnimationElapsed = 0.0f;
	SetShopButtonsEnabled(false);
}

void UPGShopScreenWidget::HandleCloseClicked()
{
	UPGAudioStatics::PlayCommonButtonClick(this);
	PlayCloseAnimation();
}

void UPGShopScreenWidget::HandlePrevCharacterClicked()
{
	UPGAudioStatics::PlayCommonButtonClick(this);
	SelectCharacterByIndex(SelectedCharacterIndex - 1);
}

void UPGShopScreenWidget::HandleNextCharacterClicked()
{
	UPGAudioStatics::PlayCommonButtonClick(this);
	SelectCharacterByIndex(SelectedCharacterIndex + 1);
}

void UPGShopScreenWidget::HandleActionClicked()
{
	UPGAudioStatics::PlayCommonButtonClick(this);
	UPGPlayerShopSubsystem* Shop = ShopSubsystem.Get();
	if (!Shop)
	{
		return;
	}

	const auto& CharacterSkins = Shop->GetCharacterSkins();
	if (!CharacterSkins.IsValidIndex(SelectedCharacterIndex) || !CharacterSkins[SelectedCharacterIndex])
	{
		return;
	}

	const FName SkinId = CharacterSkins[SelectedCharacterIndex]->SkinId;
	bool bSuccess = false;
	bool bWasPurchaseAction = false;
	if (!Shop->IsCharacterOwned(SkinId))
	{
		bWasPurchaseAction = true;
		bSuccess = Shop->PurchaseCharacter(SkinId);
	}
	else if (!Shop->IsCharacterEquipped(SkinId))
	{
		bSuccess = Shop->EquipCharacter(SkinId);
	}
	else
	{
		bSuccess = true;
	}

	bActionFeedbackActive = true;
	ActionFeedbackElapsed = 0.0f;
	bActionFeedbackSuccess = bSuccess;
	if (bWasPurchaseAction)
	{
		if (const UPGGameSettings* Settings = GetDefault<UPGGameSettings>())
		{
			UPGAudioStatics::PlaySFX2D(this, bSuccess ? Settings->SFX.ShopPurchaseSuccessSound.Get() : Settings->SFX.ShopPurchaseFailSound.Get());
		}
	}
	RefreshSelectedCharacterState();
}

void UPGShopScreenWidget::HandleMapsTabClicked()
{
	UPGAudioStatics::PlayCommonButtonClick(this);
	if (MapsTabButton)
	{
		MapsTabButton->SetIsEnabled(false);
	}
}

void UPGShopScreenWidget::HandleReceiptBalanceChanged(int32 NewBalance, FName Reason)
{
	RefreshReceiptBalance();
	RefreshSelectedCharacterState();
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

void UPGShopScreenWidget::SelectCharacterByIndex(int32 NewIndex)
{
	UPGPlayerShopSubsystem* Shop = ShopSubsystem.Get();
	if (!Shop)
	{
		return;
	}

	const int32 CharacterCount = Shop->GetCharacterSkins().Num();
	if (CharacterCount <= 0)
	{
		return;
	}

	SelectedCharacterIndex = FMath::Clamp(NewIndex, 0, CharacterCount - 1);
	const UPGCharacterSkinDefinition* SelectedSkin = Shop->GetCharacterSkins()[SelectedCharacterIndex];
	if (SelectedSkin)
	{
		Shop->PreviewCharacter(SelectedSkin->SkinId);
	}
	RefreshSelectedCharacterState();
	RefreshCharacterItems();
	ScrollSelectedCharacterIntoView();
}

void UPGShopScreenWidget::SelectCharacterById(FName SkinId)
{
	UPGPlayerShopSubsystem* Shop = ShopSubsystem.Get();
	if (!Shop)
	{
		return;
	}

	const auto& CharacterSkins = Shop->GetCharacterSkins();
	for (int32 Index = 0; Index < CharacterSkins.Num(); ++Index)
	{
		if (CharacterSkins[Index] && CharacterSkins[Index]->SkinId == SkinId)
		{
			SelectCharacterByIndex(Index);
			return;
		}
	}
}

void UPGShopScreenWidget::RebuildCharacterList()
{
	UPGPlayerShopSubsystem* Shop = ShopSubsystem.Get();
	if (!Shop || !CharacterListBox || !CharacterItemWidgetClass)
	{
		return;
	}

	CharacterListBox->ClearChildren();
	CharacterItemWidgets.Reset();

	const auto& CharacterSkins = Shop->GetCharacterSkins();
	if (SelectedCharacterIndex == INDEX_NONE && CharacterSkins.Num() > 0)
	{
		SelectedCharacterIndex = 0;
	}
	else if (CharacterSkins.Num() > 0)
	{
		SelectedCharacterIndex = FMath::Clamp(SelectedCharacterIndex, 0, CharacterSkins.Num() - 1);
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

		const bool bOwned = Shop->IsCharacterOwned(SkinDefinition->SkinId);
		const bool bEquipped = Shop->IsCharacterEquipped(SkinDefinition->SkinId);
		const bool bSelected = Index == SelectedCharacterIndex;
		ItemWidget->SetCharacterSkin(SkinDefinition, bOwned, bEquipped, bSelected);
		ItemWidget->OnCharacterSelected.AddUObject(this, &UPGShopScreenWidget::SelectCharacterById);
		CharacterListBox->AddChild(ItemWidget);
		CharacterItemWidgets.Add(ItemWidget);
	}

	RefreshCharacterItems();
	RefreshSelectedCharacterState();
	ScrollSelectedCharacterIntoView();
}

void UPGShopScreenWidget::RefreshCharacterItems()
{
	UPGPlayerShopSubsystem* Shop = ShopSubsystem.Get();
	if (!Shop)
	{
		return;
	}

	const auto& CharacterSkins = Shop->GetCharacterSkins();
	for (int32 Index = 0; Index < CharacterItemWidgets.Num(); ++Index)
	{
		UPGCharacterShopItemWidget* ItemWidget = CharacterItemWidgets[Index];
		const UPGCharacterSkinDefinition* SkinDefinition = CharacterSkins.IsValidIndex(Index) ? CharacterSkins[Index] : nullptr;
		if (!ItemWidget || !SkinDefinition)
		{
			continue;
		}

		const bool bOwned = Shop->IsCharacterOwned(SkinDefinition->SkinId);
		const bool bEquipped = Shop->IsCharacterEquipped(SkinDefinition->SkinId);
		const bool bSelected = Index == SelectedCharacterIndex;
		ItemWidget->SetCharacterSkin(SkinDefinition, bOwned, bEquipped, bSelected);
	}
}

void UPGShopScreenWidget::RefreshSelectedCharacterState()
{
	UPGPlayerShopSubsystem* Shop = ShopSubsystem.Get();
	if (!Shop)
	{
		return;
	}

	const auto& CharacterSkins = Shop->GetCharacterSkins();
	const UPGCharacterSkinDefinition* SelectedSkin = CharacterSkins.IsValidIndex(SelectedCharacterIndex) ? CharacterSkins[SelectedCharacterIndex] : nullptr;
	if (!SelectedSkin)
	{
		if (SelectedCharacterNameText)
		{
			SelectedCharacterNameText->SetText(FText::GetEmpty());
		}
		if (ActionButton)
		{
			ActionButton->SetIsEnabled(false);
		}
		return;
	}

	if (SelectedCharacterNameText)
	{
		SelectedCharacterNameText->SetText(SelectedSkin->DisplayName);
	}

	const bool bOwned = Shop->IsCharacterOwned(SelectedSkin->SkinId);
	const bool bEquipped = Shop->IsCharacterEquipped(SelectedSkin->SkinId);
	if (ActionButtonText)
	{
		if (!bOwned)
		{
			ActionButtonText->SetText(FText::Format(NSLOCTEXT("ProjectGShop", "BuyCharacter", "BUY {0}"), FText::AsNumber(FMath::Max(0, SelectedSkin->PriceReceipts))));
		}
		else if (!bEquipped)
		{
			ActionButtonText->SetText(NSLOCTEXT("ProjectGShop", "EquipCharacter", "EQUIP"));
		}
		else
		{
			ActionButtonText->SetText(NSLOCTEXT("ProjectGShop", "EquippedCharacter", "EQUIPPED"));
		}
	}
	if (ActionButton)
	{
		ActionButton->SetIsEnabled(!bEquipped && !bIsOpening && !bIsClosing);
	}
	if (PrevCharacterButton)
	{
		PrevCharacterButton->SetIsEnabled(SelectedCharacterIndex > 0 && !bIsOpening && !bIsClosing);
	}
	if (NextCharacterButton)
	{
		NextCharacterButton->SetIsEnabled(SelectedCharacterIndex < CharacterSkins.Num() - 1 && !bIsOpening && !bIsClosing);
	}
}

void UPGShopScreenWidget::RefreshReceiptBalance()
{
	if (ReceiptBalanceText)
	{
		const int32 Balance = WalletSubsystem.IsValid() ? WalletSubsystem->GetReceiptBalance() : 0;
		ReceiptBalanceText->SetText(FText::AsNumber(FMath::Max(0, Balance)));
	}
}

void UPGShopScreenWidget::ScrollSelectedCharacterIntoView() const
{
	if (!CharacterListBox || !CharacterItemWidgets.IsValidIndex(SelectedCharacterIndex))
	{
		return;
	}

	if (UPGCharacterShopItemWidget* SelectedItemWidget = CharacterItemWidgets[SelectedCharacterIndex])
	{
		CharacterListBox->ScrollWidgetIntoView(SelectedItemWidget, true, EDescendantScrollDestination::Center);
	}
}

void UPGShopScreenWidget::SetShopButtonsEnabled(bool bEnabled) const
{
	if (CloseButton)
	{
		CloseButton->SetIsEnabled(bEnabled);
	}
	if (PrevCharacterButton)
	{
		PrevCharacterButton->SetIsEnabled(bEnabled);
	}
	if (NextCharacterButton)
	{
		NextCharacterButton->SetIsEnabled(bEnabled);
	}
	if (ActionButton)
	{
		ActionButton->SetIsEnabled(bEnabled);
	}
	if (CharactersTabButton)
	{
		CharactersTabButton->SetIsEnabled(false);
	}
	if (MapsTabButton)
	{
		MapsTabButton->SetIsEnabled(false);
	}
}

void UPGShopScreenWidget::ApplyEntryAnimation(float Alpha)
{
	ApplyWidgetAnimationState(ShopPanel, FVector2D(0.0f, ShopPanelStartYOffset), Alpha, 0.0f, ShopPanelAnimationDuration, 0.97f, 1.0f);
	ApplyWidgetAnimationState(ShopContentPanel, FVector2D(0.0f, ShopContentStartYOffset), Alpha, 0.06f, ShopElementAnimationDuration, 0.96f, 1.0f);
	ApplyWidgetAnimationState(ReceiptPanel, FVector2D(ShopReceiptStartXOffset, ShopTopBarStartYOffset), Alpha, 0.09f, ShopElementAnimationDuration, 1.0f, 1.0f);
	ApplyWidgetAnimationState(CloseButton, FVector2D(ShopCloseStartXOffset, ShopTopBarStartYOffset), Alpha, 0.1f, ShopElementAnimationDuration, 1.0f, 1.0f);
	ApplyWidgetAnimationState(TabPanel, FVector2D(0.0f, -120.0f), Alpha, 0.12f, ShopElementAnimationDuration, 0.98f, 1.0f);
	ApplyWidgetAnimationState(CharacterNavPanel, FVector2D::ZeroVector, Alpha, 0.16f, ShopElementAnimationDuration, 0.9f, 1.0f);
	ApplyWidgetAnimationState(CharacterListPanel, FVector2D(0.0f, ShopListStartYOffset), Alpha, 0.18f, ShopElementAnimationDuration, 0.98f, 1.0f);
	ApplyWidgetAnimationState(SelectedNamePanel, FVector2D(0.0f, 70.0f), Alpha, 0.23f, ShopElementAnimationDuration, 0.92f, 1.0f);
	ApplyWidgetAnimationState(ActionButtonPanel, FVector2D(0.0f, ShopActionStartYOffset), Alpha, 0.27f, ShopElementAnimationDuration, 0.94f, 1.0f);
}

void UPGShopScreenWidget::ApplyWidgetAnimationState(UWidget* Widget, const FVector2D& StartOffset, float Alpha, float Delay, float Duration, float StartScale, float EndScale) const
{
	if (!Widget)
	{
		return;
	}

	const float EffectiveDuration = FMath::Max(KINDA_SMALL_NUMBER, Duration);
	const float LocalAlpha = FMath::Clamp((Alpha - Delay) / EffectiveDuration, 0.0f, 1.0f);
	const float EasedAlpha = ShopEaseOutCubic(LocalAlpha);
	const float ScaleAlpha = ShopEaseOutBack(LocalAlpha);
	const float Scale = FMath::Lerp(StartScale, EndScale, ScaleAlpha);

	Widget->SetRenderOpacity(EasedAlpha);
	Widget->SetRenderTranslation(StartOffset * (1.0f - EasedAlpha));
	Widget->SetRenderScale(FVector2D(Scale, Scale));
}

void UPGShopScreenWidget::ApplyActionFeedback(float DeltaSeconds)
{
	if (!bActionFeedbackActive || !ActionButton)
	{
		return;
	}

	ActionFeedbackElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(ActionFeedbackElapsed / FMath::Max(KINDA_SMALL_NUMBER, ActionFeedbackDuration), 0.0f, 1.0f);
	const float Pulse = FMath::Sin(Alpha * UE_PI);
	if (bActionFeedbackSuccess)
	{
		const float Scale = 1.0f + Pulse * 0.08f;
		ActionButton->SetRenderScale(FVector2D(Scale, Scale));
	}
	else
	{
		ActionButton->SetRenderTranslation(FVector2D(FMath::Sin(Alpha * UE_PI * 6.0f) * 12.0f * (1.0f - Alpha), 0.0f));
	}

	if (Alpha >= 1.0f)
	{
		bActionFeedbackActive = false;
		ActionButton->SetRenderScale(FVector2D(1.0f, 1.0f));
		ActionButton->SetRenderTranslation(FVector2D::ZeroVector);
	}
}
