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
		// 맵 상점은 아직 구현 범위가 아니므로 탭은 보이되 상호작용은 막는다.
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

		// 열림과 닫힘은 같은 패널 타이밍을 반대로 재생한다. 이렇게 두면 BP 레이아웃을 바꿔도
		// C++은 각 패널의 최종 위치를 기준으로 RenderTransform만 보간하면 된다.
		ApplyEntryAnimation(Alpha);

		if (RawAlpha >= 1.0f)
		{
			// 닫힘 완료 시점에만 상위 HomeFlow에 알린다. 중간에 제거하면 카메라 복구와 프리뷰 복구 순서가 흔들릴 수 있다.
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
	UPGPlayerShopSubsystem* Shop = ShopSubsystem.Get();
	if (!Shop)
	{
		UPGAudioStatics::PlayCommonButtonClick(this);
		return;
	}

	const auto& CharacterSkins = Shop->GetCharacterSkins();
	if (!CharacterSkins.IsValidIndex(SelectedCharacterIndex) || !CharacterSkins[SelectedCharacterIndex])
	{
		UPGAudioStatics::PlayCommonButtonClick(this);
		return;
	}

	const FName SkinId = CharacterSkins[SelectedCharacterIndex]->SkinId;
	bool bSuccess = false;
	bool bWasPurchaseAction = false;
	if (!Shop->IsCharacterOwned(SkinId))
	{
		// 구매 액션만 전용 성공/실패 사운드를 사용한다. 장착은 일반 UI 클릭음으로 유지한다.
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
	else
	{
		UPGAudioStatics::PlayCommonButtonClick(this);
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
		// 선택값은 위젯 인스턴스 수명보다 길게 유지한다. Shop을 닫았다 다시 열어도
		// 마지막으로 보던 카드를 기준으로 Prev/Next가 이어지도록 Subsystem에 세션 상태를 맡긴다.
		Shop->SetLastSelectedCharacterSkinId(SelectedSkin->SkinId);
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

			SelectedCharacterIndex = FindSkinIndex(Shop->GetLastSelectedCharacterSkinId());
			if (SelectedCharacterIndex == INDEX_NONE)
			{
				const UPGCharacterSkinDefinition* EquippedSkin = Shop->GetEquippedCharacterSkin();
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

		const bool bOwned = Shop->IsCharacterOwned(SkinDefinition->SkinId);
		const bool bEquipped = Shop->IsCharacterEquipped(SkinDefinition->SkinId);
		const bool bSelected = Index == SelectedCharacterIndex;

		// 리스트 위젯은 최초 구성 때만 만든다. 이후 선택/구매/장착 상태 변화는
		// RefreshCharacterItems()에서 기존 카드의 상태만 갱신해 ScrollBox 관성 입력을 보존한다.
		ItemWidget->SetCharacterSkin(SkinDefinition, bOwned, bEquipped, bSelected);
		ItemWidget->OnCharacterSelected.AddUObject(this, &UPGShopScreenWidget::SelectCharacterById);
		CharacterListBox->AddChild(ItemWidget);
		CharacterItemWidgets.Add(ItemWidget);
	}

	RefreshCharacterItems();
	if (const UPGCharacterSkinDefinition* SelectedSkin = CharacterSkins.IsValidIndex(SelectedCharacterIndex) ? CharacterSkins[SelectedCharacterIndex] : nullptr)
	{
		// 리스트 생성 직후에도 월드 러너 프리뷰와 UI 선택 상태가 같은 스킨을 가리키게 맞춘다.
		Shop->SetLastSelectedCharacterSkinId(SelectedSkin->SkinId);
		Shop->PreviewCharacter(SelectedSkin->SkinId);
	}
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
		return;
	}

	if (SelectedCharacterNameText)
	{
		SelectedCharacterNameText->SetText(SelectedSkin->DisplayName);
	}

	const bool bOwned = Shop->IsCharacterOwned(SelectedSkin->SkinId);
	const bool bEquipped = Shop->IsCharacterEquipped(SelectedSkin->SkinId);
	if (SelectedPricePanel)
	{
		// 가격은 미보유 캐릭터의 상세 영역에서만 보여준다. 카드 내부 가격 텍스트는 모바일 가독성 때문에 제거했다.
		SelectedPricePanel->SetVisibility(!bOwned ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (SelectedPriceText)
	{
		SelectedPriceText->SetText(!bOwned ? FText::AsNumber(FMath::Max(0, SelectedSkin->PriceReceipts)) : FText::GetEmpty());
	}
	if (ActionButtonText)
	{
		if (!bOwned)
		{
			ActionButtonText->SetText(NSLOCTEXT("ProjectGShop", "BuyCharacter", "BUY"));
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
	// BP 계층은 상점 배경, 상단 재화, 탭, 리스트, 액션 버튼을 별도 패널로 나눈다.
	// 각 패널에 작은 딜레이를 주면 전체 화면이 한 장으로 뜨지 않고 상품 진열대처럼 단계적으로 열린다.
	ApplyWidgetAnimationState(ShopPanel, FVector2D(0.0f, ShopPanelStartYOffset), Alpha, 0.0f, ShopPanelAnimationDuration, 0.97f, 1.0f);
	ApplyWidgetAnimationState(ShopContentPanel, FVector2D(0.0f, ShopContentStartYOffset), Alpha, 0.06f, ShopElementAnimationDuration, 0.96f, 1.0f);
	ApplyWidgetAnimationState(ReceiptPanel, FVector2D(ShopReceiptStartXOffset, ShopTopBarStartYOffset), Alpha, 0.09f, ShopElementAnimationDuration, 1.0f, 1.0f);
	ApplyWidgetAnimationState(CloseButton, FVector2D(ShopCloseStartXOffset, ShopTopBarStartYOffset), Alpha, 0.1f, ShopElementAnimationDuration, 1.0f, 1.0f);
	ApplyWidgetAnimationState(TabPanel, FVector2D(0.0f, -120.0f), Alpha, 0.12f, ShopElementAnimationDuration, 0.98f, 1.0f);
	ApplyWidgetAnimationState(CharacterNavPanel, FVector2D::ZeroVector, Alpha, 0.16f, ShopElementAnimationDuration, 0.9f, 1.0f);
	ApplyWidgetAnimationState(CharacterListPanel, FVector2D(0.0f, ShopListStartYOffset), Alpha, 0.18f, ShopElementAnimationDuration, 0.98f, 1.0f);
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
	const float EasedAlpha = ShopEaseOutCubic(LocalAlpha);
	const float ScaleAlpha = ShopEaseOutBack(LocalAlpha);
	const float Scale = FMath::Lerp(StartScale, EndScale, ScaleAlpha);

	// BP가 배치한 패널은 유지하고 RenderTransform만 조작해 디자인 자산을 C++에서 덮어쓰지 않는다.
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
		// 성공은 짧은 펄스로 확정감을 주고, 실패는 흔들림으로 재화 부족/저장 실패를 구분한다.
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
