// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PGReceiptShopWidget.h"

#include "Audio/PGAudioStatics.h"
#include "Components/BackgroundBlur.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Settings/PGGameSettings.h"
#include "UI/PGReceiptShopPackItemWidget.h"
#include "UI/PGUIAnimationUtils.h"
#include "User/PGUserDataSubsystem.h"

namespace
{
constexpr float ReceiptShopTargetBlurStrength = 5.0f;
constexpr float ReceiptShopTargetDimAlpha = 0.38f;
const FVector2D ReceiptShopPanelStartOffset(0.0f, 460.0f);

}

void UPGReceiptShopWidget::InitializeReceiptShop(UPGUserDataSubsystem* InUserDataSubsystem, TSubclassOf<UPGReceiptShopPackItemWidget> InPackItemWidgetClass, const TArray<FPGReceiptShopPack>& InReceiptPacks)
{
	UserDataSubsystem = InUserDataSubsystem;
	PackItemWidgetClass = InPackItemWidgetClass;
	ReceiptPacks = InReceiptPacks;

	if (UPGUserDataSubsystem* UserData = UserDataSubsystem.Get())
	{
		UserData->OnReceiptBalanceChanged.AddDynamic(this, &UPGReceiptShopWidget::HandleReceiptBalanceChanged);
	}

	RebuildPackList();
	RefreshReceiptBalance();
}

void UPGReceiptShopWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (CloseButton)
	{
		CloseButton->OnClicked.AddDynamic(this, &UPGReceiptShopWidget::HandleCloseClicked);
	}

	ApplyEntryAnimation(0.0f);
	SetReceiptShopButtonsEnabled(false);
}

void UPGReceiptShopWidget::NativeDestruct()
{
	if (CloseButton)
	{
		CloseButton->OnClicked.RemoveDynamic(this, &UPGReceiptShopWidget::HandleCloseClicked);
	}

	for (const TWeakObjectPtr<UPGReceiptShopPackItemWidget>& ItemWidgetPtr : PackItemWidgets)
	{
		UPGReceiptShopPackItemWidget* ItemWidget = ItemWidgetPtr.Get();
		if (ItemWidget)
		{
			ItemWidget->OnPackSelected.RemoveAll(this);
		}
	}

	if (UPGUserDataSubsystem* UserData = UserDataSubsystem.Get())
	{
		UserData->OnReceiptBalanceChanged.RemoveDynamic(this, &UPGReceiptShopWidget::HandleReceiptBalanceChanged);
	}

	Super::NativeDestruct();
}

void UPGReceiptShopWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bIsOpening && !bIsClosing)
	{
		return;
	}

	AnimationElapsed += InDeltaTime;
	const float RawAlpha = AnimationDuration > KINDA_SMALL_NUMBER ? FMath::Clamp(AnimationElapsed / AnimationDuration, 0.0f, 1.0f) : 1.0f;
	const float Alpha = bIsClosing ? 1.0f - RawAlpha : RawAlpha;

	// 배경 레이어와 상점 패널을 분리해 Home UI는 고정하고, 구매 패널만 모달처럼 떠오르게 한다.
	ApplyEntryAnimation(Alpha);

	if (RawAlpha >= 1.0f)
	{
		const bool bWasClosing = bIsClosing;
		bIsOpening = false;
		bIsClosing = false;
		SetReceiptShopButtonsEnabled(!bWasClosing);
		if (bWasClosing)
		{
			OnReceiptShopClosed.Broadcast();
			RemoveFromParent();
		}
	}
}

void UPGReceiptShopWidget::PlayOpenAnimation()
{
	bIsOpening = true;
	bIsClosing = false;
	AnimationElapsed = 0.0f;
	SetReceiptShopButtonsEnabled(false);
	ApplyEntryAnimation(0.0f);
}

void UPGReceiptShopWidget::PlayCloseAnimation()
{
	if (bIsClosing)
	{
		return;
	}

	bIsOpening = false;
	bIsClosing = true;
	AnimationElapsed = 0.0f;
	SetReceiptShopButtonsEnabled(false);
}

void UPGReceiptShopWidget::HandleCloseClicked()
{
	UPGAudioStatics::PlayCommonButtonClick(this);
	PlayCloseAnimation();
}

void UPGReceiptShopWidget::HandleReceiptBalanceChanged(int32 NewBalance, FName Reason)
{
	RefreshReceiptBalance();
}

void UPGReceiptShopWidget::HandlePackSelected(int32 PackIndex)
{
	UPGUserDataSubsystem* UserData = UserDataSubsystem.Get();
	const bool bValidPack = ReceiptPacks.IsValidIndex(PackIndex) && ReceiptPacks[PackIndex].ReceiptAmount > 0;
	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	const bool bTestGrantEnabled = Settings && Settings->Shop.IsReceiptShopTestGrantEnabled();
	// 실제 결제 대신 설정된 상품 수량을 즉시 지급해 포트폴리오용 구매 절차를 완결한다.
	const bool bSuccess = bTestGrantEnabled
		&& UserData
		&& bValidPack
		&& UserData->TryAddReceipts(ReceiptPacks[PackIndex].ReceiptAmount, TEXT("ReceiptShopTestGrant"));

	if (Settings)
	{
		UPGAudioStatics::PlayUIButtonSound(this, bSuccess ? Settings->SFX.ShopPurchaseSuccessSound.Get() : Settings->SFX.ShopPurchaseFailSound.Get());
	}
	else
	{
		UPGAudioStatics::PlayCommonButtonClick(this);
	}

	if (const TWeakObjectPtr<UPGReceiptShopPackItemWidget>* ItemWidget = PackItemWidgetsByPackIndex.Find(PackIndex))
	{
		if (UPGReceiptShopPackItemWidget* ItemWidgetObject = ItemWidget->Get())
		{
			ItemWidgetObject->PlayPurchaseFeedback(bSuccess);
		}
	}
}

void UPGReceiptShopWidget::RebuildPackList()
{
	if (!PackListBox || !PackItemWidgetClass)
	{
		return;
	}

	PackListBox->ClearChildren();
	PackItemWidgets.Reset();
	PackItemWidgetsByPackIndex.Reset();

	for (int32 Index = 0; Index < ReceiptPacks.Num(); ++Index)
	{
		if (ReceiptPacks[Index].ReceiptAmount <= 0)
		{
			continue;
		}

		UPGReceiptShopPackItemWidget* ItemWidget = CreateWidget<UPGReceiptShopPackItemWidget>(GetOwningPlayer(), PackItemWidgetClass);
		if (!ItemWidget)
		{
			continue;
		}

		ItemWidget->InitializePack(ReceiptPacks[Index], Index);
		ItemWidget->OnPackSelected.AddUObject(this, &UPGReceiptShopWidget::HandlePackSelected);
		PackListBox->AddChild(ItemWidget);
		PackItemWidgets.Add(ItemWidget);
		PackItemWidgetsByPackIndex.Add(Index, ItemWidget);
	}
}

void UPGReceiptShopWidget::RefreshReceiptBalance() const
{
	if (ReceiptBalanceText)
	{
		const int32 Balance = UserDataSubsystem.IsValid() ? UserDataSubsystem->GetReceiptBalance() : 0;
		ReceiptBalanceText->SetText(FText::AsNumber(FMath::Max(0, Balance)));
	}
}

void UPGReceiptShopWidget::SetReceiptShopButtonsEnabled(bool bEnabled) const
{
	if (CloseButton)
	{
		CloseButton->SetIsEnabled(bEnabled);
	}

	for (const TWeakObjectPtr<UPGReceiptShopPackItemWidget>& ItemWidgetPtr : PackItemWidgets)
	{
		UPGReceiptShopPackItemWidget* ItemWidget = ItemWidgetPtr.Get();
		if (ItemWidget)
		{
			ItemWidget->SetPackButtonEnabled(bEnabled);
		}
	}
}

void UPGReceiptShopWidget::ApplyEntryAnimation(float Alpha) const
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	const float EasedAlpha = PGUIAnimation::EaseOutCubic(ClampedAlpha);
	const float ScaleAlpha = PGUIAnimation::EaseOutBack(ClampedAlpha, 1.45f);

	if (BackgroundBlur)
	{
		BackgroundBlur->SetBlurStrength(FMath::Lerp(0.0f, ReceiptShopTargetBlurStrength, EasedAlpha));
		BackgroundBlur->SetRenderOpacity(EasedAlpha);
	}
	if (BackdropDim)
	{
		BackdropDim->SetRenderOpacity(FMath::Lerp(0.0f, ReceiptShopTargetDimAlpha, EasedAlpha));
	}
	if (ShopPanel)
	{
		ShopPanel->SetRenderOpacity(EasedAlpha);
		ShopPanel->SetRenderTranslation(ReceiptShopPanelStartOffset * (1.0f - EasedAlpha));
		const float Scale = FMath::Lerp(0.94f, 1.0f, ScaleAlpha);
		ShopPanel->SetRenderScale(FVector2D(Scale, Scale));
	}
}

void UPGReceiptShopWidget::RequestClose()
{
	HandleCloseClicked();
}
