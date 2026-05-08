// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PGCargoDeliveryItemWidget.h"

#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGCargoDeliveryItemWidget, Log, All);

namespace
{
float CargoItemEaseOutCubic(float Alpha)
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	return 1.0f - FMath::Pow(1.0f - ClampedAlpha, 3.0f);
}
}

void UPGCargoDeliveryItemWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ValidateRequiredWidgets();
	SetRenderOpacity(0.0f);
}

void UPGCargoDeliveryItemWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bIsEntering && !bIsExiting)
	{
		return;
	}

	AnimationElapsed += InDeltaTime;
	const float Alpha = AnimationDuration > KINDA_SMALL_NUMBER
		? FMath::Clamp(AnimationElapsed / AnimationDuration, 0.0f, 1.0f)
		: 1.0f;
	ApplyAnimationState(CargoItemEaseOutCubic(Alpha), bIsExiting);

	if (Alpha >= 1.0f)
	{
		const bool bFinishedExit = bIsExiting;
		bIsEntering = false;
		bIsExiting = false;

		if (bFinishedExit)
		{
			OnExitAnimationFinished.Broadcast(this);
		}
	}
}

void UPGCargoDeliveryItemWidget::ConfigureCargoItem(const FPGActiveCargoInfo& CargoInfo)
{
	CargoInstanceId = CargoInfo.InstanceId;

	if (ThumbnailImage)
	{
		ThumbnailImage->SetBrushFromTexture(CargoInfo.CargoThumbnail);
	}

	if (RemainingTimeText)
	{
		RemainingTimeText->SetText(FText::Format(
			NSLOCTEXT("PGCargoDeliveryItem", "RemainingTimeFormat", "{0}s"),
			FText::AsNumber(FMath::CeilToInt(CargoInfo.RemainingDeliveryTime))));
	}

	if (DeliveryProgressBar)
	{
		DeliveryProgressBar->SetPercent(FMath::Clamp(CargoInfo.DeliveryProgress, 0.0f, 1.0f));
	}
}

void UPGCargoDeliveryItemWidget::PlayEnterAnimation()
{
	bIsEntering = true;
	bIsExiting = false;
	AnimationElapsed = 0.0f;
	SetVisibility(ESlateVisibility::HitTestInvisible);
	ApplyAnimationState(0.0f, false);
}

void UPGCargoDeliveryItemWidget::PlayExitAnimation()
{
	bIsEntering = false;
	bIsExiting = true;
	AnimationElapsed = 0.0f;
	SetVisibility(ESlateVisibility::HitTestInvisible);
	ApplyAnimationState(0.0f, true);
}

void UPGCargoDeliveryItemWidget::ValidateRequiredWidgets() const
{
	if (!RootPanel)
	{
		UE_LOG(LogPGCargoDeliveryItemWidget, Warning, TEXT("%s requires BindWidget 'RootPanel'."), *GetNameSafe(this));
	}
	if (!ThumbnailImage)
	{
		UE_LOG(LogPGCargoDeliveryItemWidget, Warning, TEXT("%s requires BindWidget 'ThumbnailImage'."), *GetNameSafe(this));
	}
	if (!RemainingTimeText)
	{
		UE_LOG(LogPGCargoDeliveryItemWidget, Warning, TEXT("%s requires BindWidget 'RemainingTimeText'."), *GetNameSafe(this));
	}
	if (!DeliveryProgressBar)
	{
		UE_LOG(LogPGCargoDeliveryItemWidget, Warning, TEXT("%s requires BindWidget 'DeliveryProgressBar'."), *GetNameSafe(this));
	}
}

void UPGCargoDeliveryItemWidget::ApplyAnimationState(float Alpha, bool bExiting)
{
	const float Opacity = bExiting ? 1.0f - Alpha : Alpha;
	const float XOffset = bExiting ? -80.0f * Alpha : 80.0f * (1.0f - Alpha);
	const float Scale = bExiting ? FMath::Lerp(1.0f, 0.88f, Alpha) : FMath::Lerp(0.88f, 1.0f, Alpha);

	SetRenderOpacity(Opacity);
	SetRenderTranslation(FVector2D(XOffset, 0.0f));
	SetRenderScale(FVector2D(Scale, Scale));
}
