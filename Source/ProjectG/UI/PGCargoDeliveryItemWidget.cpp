// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PGCargoDeliveryItemWidget.h"

#include "Cargo/PGCargoStackComponent.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/Texture2D.h"
#include "Settings/PGGameSettings.h"
#include "UI/PGUIAnimationUtils.h"

void UPGCargoDeliveryItemWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetRenderOpacity(0.0f);

	if (const UPGGameSettings* Settings = GetDefault<UPGGameSettings>())
	{
		CargoDangerWarningRatio = Settings->Cargo.CargoDangerWarningRatio;
	}
	// 디자이너가 BP에서 보이게 둬도 경고 테두리는 런타임 상태로만 켠다.
	if (DangerFrame)
	{
		DangerFrame->SetVisibility(DangerAlpha > 0.0f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UPGCargoDeliveryItemWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (DangerFrame && DangerAlpha > 0.0f)
	{
		// 초당 2.5회 사인 펄스. 완전히 꺼지지 않도록 하한을 둬 경고가 끊겨 보이지 않게 한다.
		DangerPulseElapsed += InDeltaTime;
		const float Pulse = 0.5f + 0.5f * FMath::Sin(DangerPulseElapsed * 2.0f * PI * 2.5f);
		DangerFrame->SetRenderOpacity(DangerAlpha * FMath::Lerp(0.35f, 1.0f, Pulse));
	}

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
		SetDangerRatio(0.0f);
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
			NSLOCTEXT("PGCargoDeliveryItem", "RemainingTimeFormat", "{0}초"),
			FText::AsNumber(RemainingSeconds)));
	}

	if (DeliveryProgressBar)
	{
		DeliveryProgressBar->SetPercent(FMath::Clamp(CargoInfo.DeliveryProgress, 0.0f, 1.0f));
	}

	SetDangerRatio(CargoInfo.DangerRatio);
}

void UPGCargoDeliveryItemWidget::SetDangerRatio(float DangerRatio)
{
	// 문턱 아래면 숨기고, 문턱에서 약하게 켜져 테두리에 닿을수록 밝아진다. 바구니 링과 같은 기준이다.
	const float NewDangerAlpha = CargoDangerWarningRatio < 1.0f - KINDA_SMALL_NUMBER && DangerRatio >= CargoDangerWarningRatio
		? FMath::GetMappedRangeValueClamped(FVector2D(CargoDangerWarningRatio, 1.0f), FVector2D(0.4f, 1.0f), DangerRatio)
		: 0.0f;
	if (DangerFrame && (NewDangerAlpha > 0.0f) != (DangerAlpha > 0.0f))
	{
		DangerPulseElapsed = 0.0f;
		DangerFrame->SetVisibility(NewDangerAlpha > 0.0f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	DangerAlpha = NewDangerAlpha;
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
