// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PGReceiptShopPackItemWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/Texture2D.h"

namespace
{
float ReceiptPackEaseOutCubic(float Alpha)
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	const float InverseAlpha = 1.0f - ClampedAlpha;
	return 1.0f - InverseAlpha * InverseAlpha * InverseAlpha;
}
}

void UPGReceiptShopPackItemWidget::InitializePack(const FPGReceiptShopPack& InPack, int32 InPackIndex)
{
	PackData = InPack;
	PackIndex = InPackIndex;

	if (PackNameText)
	{
		PackNameText->SetText(FText::FromString(PackData.PackName));
	}
	if (ReceiptAmountText)
	{
		ReceiptAmountText->SetText(FText::Format(NSLOCTEXT("ProjectGReceiptShop", "ReceiptAmountFormat", "x{0}"), FText::AsNumber(FMath::Max(0, PackData.ReceiptAmount))));
	}
	if (PriceText)
	{
		PriceText->SetText(FText::FromString(PackData.PriceText));
	}
	if (ThumbnailImage)
	{
		if (UTexture2D* ThumbnailTexture = PackData.Thumbnail.Get())
		{
			ThumbnailImage->SetBrushFromTexture(ThumbnailTexture, true);
		}
	}
}

void UPGReceiptShopPackItemWidget::PlayPurchaseFeedback(bool bSuccess)
{
	bFeedbackActive = true;
	FeedbackElapsed = 0.0f;
	bFeedbackSuccess = bSuccess;
	ApplyPurchaseFeedback(0.0f);
}

void UPGReceiptShopPackItemWidget::SetPackButtonEnabled(bool bEnabled) const
{
	if (PurchaseButton)
	{
		PurchaseButton->SetIsEnabled(bEnabled);
	}
}

void UPGReceiptShopPackItemWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (PurchaseButton)
	{
		PurchaseButton->OnClicked.AddDynamic(this, &UPGReceiptShopPackItemWidget::HandlePurchaseClicked);
	}
}

void UPGReceiptShopPackItemWidget::NativeDestruct()
{
	if (PurchaseButton)
	{
		PurchaseButton->OnClicked.RemoveDynamic(this, &UPGReceiptShopPackItemWidget::HandlePurchaseClicked);
	}

	Super::NativeDestruct();
}

void UPGReceiptShopPackItemWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	ApplyPurchaseFeedback(InDeltaTime);
}

void UPGReceiptShopPackItemWidget::HandlePurchaseClicked()
{
	OnPackSelected.Broadcast(PackIndex);
}

void UPGReceiptShopPackItemWidget::ApplyPurchaseFeedback(float DeltaSeconds)
{
	if (!bFeedbackActive || !RootPanel)
	{
		return;
	}

	FeedbackElapsed += DeltaSeconds;
	const float Alpha = FeedbackDuration > KINDA_SMALL_NUMBER ? FMath::Clamp(FeedbackElapsed / FeedbackDuration, 0.0f, 1.0f) : 1.0f;
	const float EasedAlpha = ReceiptPackEaseOutCubic(Alpha);

	if (bFeedbackSuccess)
	{
		const float PulseAlpha = Alpha < 0.5f ? Alpha * 2.0f : (1.0f - Alpha) * 2.0f;
		const float Scale = FMath::Lerp(1.0f, 1.06f, ReceiptPackEaseOutCubic(PulseAlpha));
		RootPanel->SetRenderScale(FVector2D(Scale, Scale));
		RootPanel->SetRenderTranslation(FVector2D::ZeroVector);
	}
	else
	{
		const float Shake = FMath::Sin(EasedAlpha * PI * 6.0f) * (1.0f - EasedAlpha) * 16.0f;
		RootPanel->SetRenderTranslation(FVector2D(Shake, 0.0f));
		RootPanel->SetRenderScale(FVector2D(1.0f, 1.0f));
	}

	if (Alpha >= 1.0f)
	{
		bFeedbackActive = false;
		RootPanel->SetRenderTranslation(FVector2D::ZeroVector);
		RootPanel->SetRenderScale(FVector2D(1.0f, 1.0f));
	}
}
