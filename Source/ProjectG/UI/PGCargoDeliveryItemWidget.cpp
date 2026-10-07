// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PGCargoDeliveryItemWidget.h"

#include "Cargo/PGCargoStackComponent.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/Texture2D.h"
#include "UI/PGUIAnimationUtils.h"

void UPGCargoDeliveryItemWidget::NativeConstruct()
{
	Super::NativeConstruct();
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
	ApplyAnimationState(PGUIAnimation::EaseOutCubic(Alpha), bIsExiting);

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
	if (CargoInstanceId != CargoInfo.InstanceId)
	{
		// 풀에서 재사용된 카드는 이전 적재물의 표시 상태를 버리고 새로 그린다.
		CargoInstanceId = CargoInfo.InstanceId;
		LastDisplayedRemainingSeconds = INDEX_NONE;
		LastDisplayedThumbnail.Reset();
	}

	if (ThumbnailImage && LastDisplayedThumbnail.Get() != CargoInfo.CargoThumbnail)
	{
		LastDisplayedThumbnail = CargoInfo.CargoThumbnail;
		ThumbnailImage->SetBrushFromTexture(CargoInfo.CargoThumbnail);
	}

	// 남은 시간은 정수 초만 표시하므로 초가 바뀔 때만 텍스트를 다시 만든다.
	const int32 RemainingSeconds = FMath::CeilToInt(CargoInfo.RemainingDeliveryTime);
	if (RemainingTimeText && RemainingSeconds != LastDisplayedRemainingSeconds)
	{
		LastDisplayedRemainingSeconds = RemainingSeconds;
		RemainingTimeText->SetText(FText::Format(
			NSLOCTEXT("PGCargoDeliveryItem", "RemainingTimeFormat", "{0}s"),
			FText::AsNumber(RemainingSeconds)));
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

void UPGCargoDeliveryItemWidget::ApplyAnimationState(float Alpha, bool bExiting)
{
	const float Opacity = bExiting ? 1.0f - Alpha : Alpha;
	const float XOffset = bExiting ? -80.0f * Alpha : 80.0f * (1.0f - Alpha);
	const float Scale = bExiting ? FMath::Lerp(1.0f, 0.88f, Alpha) : FMath::Lerp(0.88f, 1.0f, Alpha);

	SetRenderOpacity(Opacity);
	SetRenderTranslation(FVector2D(XOffset, 0.0f));
	SetRenderScale(FVector2D(Scale, Scale));
}
