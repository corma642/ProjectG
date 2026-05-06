// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PGHomeScreenWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Widget.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGHomeScreenWidget, Log, All);

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

	ValidateRequiredWidgets();

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

	if (StartDeliveryButton)
	{
		StartDeliveryButton->SetIsEnabled(false);
	}
	if (SkinsButton)
	{
		SkinsButton->SetIsEnabled(false);
	}
	if (SettingsButton)
	{
		SettingsButton->SetIsEnabled(false);
	}

	ApplyExitAnimation(0.0f);
	if (ExitAnimationDuration <= KINDA_SMALL_NUMBER)
	{
		ApplyExitAnimation(1.0f);
		bIsExitAnimating = false;
		RemoveFromParent();
	}
}

void UPGHomeScreenWidget::HandleStartDeliveryClicked()
{
	OnStartDeliveryRequested.Broadcast();
}

void UPGHomeScreenWidget::HandleSkinsClicked()
{
	UE_LOG(LogPGHomeScreenWidget, Log, TEXT("Skins button clicked. TODO: open skins/shop screen."));
}

void UPGHomeScreenWidget::HandleSettingsClicked()
{
	UE_LOG(LogPGHomeScreenWidget, Log, TEXT("Settings button clicked. TODO: open settings screen."));
}

void UPGHomeScreenWidget::ValidateRequiredWidgets() const
{
	if (!RootPanel)
	{
		UE_LOG(LogPGHomeScreenWidget, Warning, TEXT("%s requires BindWidget 'RootPanel'."), *GetNameSafe(this));
	}
	if (!LogoImage)
	{
		UE_LOG(LogPGHomeScreenWidget, Warning, TEXT("%s requires BindWidget 'LogoImage'."), *GetNameSafe(this));
	}
	if (!StartDeliveryButton)
	{
		UE_LOG(LogPGHomeScreenWidget, Warning, TEXT("%s requires BindWidget 'StartDeliveryButton'."), *GetNameSafe(this));
	}
	if (!SkinsButton)
	{
		UE_LOG(LogPGHomeScreenWidget, Warning, TEXT("%s requires BindWidget 'SkinsButton'."), *GetNameSafe(this));
	}
	if (!SettingsButton)
	{
		UE_LOG(LogPGHomeScreenWidget, Warning, TEXT("%s requires BindWidget 'SettingsButton'."), *GetNameSafe(this));
	}
}

void UPGHomeScreenWidget::ApplyExitAnimation(float Alpha)
{
	const float AnimationTime = FMath::Max(0.0f, ExitAnimationDuration) * FMath::Clamp(Alpha, 0.0f, 1.0f);
	ApplyWidgetExitAnimation(LogoImage, LogoExitDelay, LogoExitOffset, AnimationTime);
	ApplyWidgetExitAnimation(StartDeliveryButton, StartButtonExitDelay, StartButtonExitOffset, AnimationTime);
	ApplyWidgetExitAnimation(SkinsButton, SubButtonExitDelay, SubButtonExitOffset, AnimationTime);
	ApplyWidgetExitAnimation(SettingsButton, SubButtonExitDelay, SubButtonExitOffset, AnimationTime);
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
