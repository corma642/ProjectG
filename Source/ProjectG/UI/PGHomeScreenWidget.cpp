// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PGHomeScreenWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"

namespace
{
constexpr float LogoExitDelay = 0.0f;
constexpr float StartButtonExitDelay = 0.05f;
constexpr float SubButtonExitDelay = 0.10f;
const FVector2D LogoExitOffset(0.0f, -600.0f);
const FVector2D StartButtonExitOffset(-900.0f, 0.0f);
const FVector2D SubButtonExitOffset(0.0f, 700.0f);

float EaseInCubicHome(float Alpha)
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	return ClampedAlpha * ClampedAlpha * ClampedAlpha;
}
}

void UPGHomeScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (StartDeliveryButton)
	{
		StartDeliveryButton->OnClicked.AddDynamic(this, &UPGHomeScreenWidget::HandleStartDeliveryClicked);
	}
	if (SkinsButton)
	{
		SkinsButton->OnClicked.AddDynamic(this, &UPGHomeScreenWidget::HandleSkinsClicked);
	}
	if (SettingsButton)
	{
		SettingsButton->OnClicked.AddDynamic(this, &UPGHomeScreenWidget::HandleSettingsClicked);
	}
	if (HelpButton)
	{
		HelpButton->OnClicked.AddDynamic(this, &UPGHomeScreenWidget::HandleHelpClicked);
	}

	ApplyExitAnimation(0.0f);
}

void UPGHomeScreenWidget::NativeDestruct()
{
	if (StartDeliveryButton)
	{
		StartDeliveryButton->OnClicked.RemoveDynamic(this, &UPGHomeScreenWidget::HandleStartDeliveryClicked);
	}
	if (SkinsButton)
	{
		SkinsButton->OnClicked.RemoveDynamic(this, &UPGHomeScreenWidget::HandleSkinsClicked);
	}
	if (SettingsButton)
	{
		SettingsButton->OnClicked.RemoveDynamic(this, &UPGHomeScreenWidget::HandleSettingsClicked);
	}
	if (HelpButton)
	{
		HelpButton->OnClicked.RemoveDynamic(this, &UPGHomeScreenWidget::HandleHelpClicked);
	}

	Super::NativeDestruct();
}

void UPGHomeScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bIsExitAnimating)
	{
		return;
	}

	ExitAnimationElapsed += InDeltaTime;
	const float Alpha = ExitAnimationDuration > KINDA_SMALL_NUMBER
		? FMath::Clamp(ExitAnimationElapsed / ExitAnimationDuration, 0.0f, 1.0f)
		: 1.0f;
	ApplyExitAnimation(Alpha);

	if (Alpha >= 1.0f)
	{
		bIsExitAnimating = false;
		RemoveFromParent();
	}
}

void UPGHomeScreenWidget::PlayExitAnimation(float Duration)
{
	ExitAnimationDuration = FMath::Max(0.0f, Duration);
	ExitAnimationElapsed = 0.0f;
	bIsExitAnimating = true;

	SetHomeButtonsEnabled(false);

	ApplyExitAnimation(0.0f);
	if (ExitAnimationDuration <= KINDA_SMALL_NUMBER)
	{
		ApplyExitAnimation(1.0f);
		bIsExitAnimating = false;
		RemoveFromParent();
	}
}

void UPGHomeScreenWidget::SetHomeButtonsEnabled(bool bEnabled) const
{
	if (StartDeliveryButton)
	{
		StartDeliveryButton->SetIsEnabled(bEnabled);
	}
	if (SkinsButton)
	{
		SkinsButton->SetIsEnabled(bEnabled);
	}
	if (SettingsButton)
	{
		SettingsButton->SetIsEnabled(bEnabled);
	}
	if (HelpButton)
	{
		HelpButton->SetIsEnabled(bEnabled);
	}
}

void UPGHomeScreenWidget::SetReceiptBalance(int32 Balance) const
{
	if (ReceiptBalanceText)
	{
		ReceiptBalanceText->SetText(FText::AsNumber(FMath::Max(0, Balance)));
	}
}

void UPGHomeScreenWidget::HandleStartDeliveryClicked()
{
	OnStartDeliveryRequested.Broadcast();
}

void UPGHomeScreenWidget::HandleSkinsClicked()
{
}

void UPGHomeScreenWidget::HandleSettingsClicked()
{
	OnSettingsRequested.Broadcast();
}

void UPGHomeScreenWidget::HandleHelpClicked()
{
	OnHelpRequested.Broadcast();
}

void UPGHomeScreenWidget::ApplyExitAnimation(float Alpha)
{
	const float AnimationTime = FMath::Max(0.0f, ExitAnimationDuration) * FMath::Clamp(Alpha, 0.0f, 1.0f);
	ApplyWidgetExitAnimation(LogoImage, LogoExitDelay, LogoExitOffset, AnimationTime);
	ApplyWidgetExitAnimation(StartDeliveryButton, StartButtonExitDelay, StartButtonExitOffset, AnimationTime);
	ApplyWidgetExitAnimation(SkinsButton, SubButtonExitDelay, SubButtonExitOffset, AnimationTime);
	ApplyWidgetExitAnimation(SettingsButton, SubButtonExitDelay, SubButtonExitOffset, AnimationTime);
	ApplyWidgetExitAnimation(HelpButton, SubButtonExitDelay, SubButtonExitOffset, AnimationTime);
	ApplyWidgetExitAnimation(ReceiptBalanceText, LogoExitDelay, LogoExitOffset, AnimationTime);
}

void UPGHomeScreenWidget::ApplyWidgetExitAnimation(UWidget* Widget, float Delay, const FVector2D& TargetOffset, float AnimationTime) const
{
	if (!Widget)
	{
		return;
	}

	const float EffectiveDuration = FMath::Max(KINDA_SMALL_NUMBER, ExitAnimationDuration - Delay);
	const float LocalAlpha = FMath::Clamp((AnimationTime - Delay) / EffectiveDuration, 0.0f, 1.0f);
	const float EasedAlpha = EaseInCubicHome(LocalAlpha);
	const float Scale = FMath::Lerp(1.0f, 0.92f, EasedAlpha);

	Widget->SetRenderTranslation(TargetOffset * EasedAlpha);
	Widget->SetRenderOpacity(1.0f - EasedAlpha);
	Widget->SetRenderScale(FVector2D(Scale, Scale));
}
