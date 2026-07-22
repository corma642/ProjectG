// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PGHomeScreenWidget.h"

#include "Audio/PGAudioStatics.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Settings/PGGameSettings.h"

namespace
{
constexpr float LogoAnimationDelay = 0.0f;
constexpr float MenuAnimationDelay = 0.08f;
constexpr float ReceiptAnimationDelay = 0.14f;
const FVector2D LogoEntryOffset(0.0f, -520.0f);
const FVector2D MenuEntryOffset(0.0f, 620.0f);
const FVector2D ReceiptEntryOffset(280.0f, -120.0f);
const FVector2D LogoExitOffset(0.0f, -620.0f);
const FVector2D MenuExitOffset(0.0f, 760.0f);
const FVector2D ReceiptExitOffset(320.0f, -160.0f);

float EaseInCubicHome(float Alpha)
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	return ClampedAlpha * ClampedAlpha * ClampedAlpha;
}

float EaseOutCubicHome(float Alpha)
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	const float InverseAlpha = 1.0f - ClampedAlpha;
	return 1.0f - InverseAlpha * InverseAlpha * InverseAlpha;
}
}

void UPGHomeScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (StartDeliveryButton)
	{
		StartDeliveryButton->OnClicked.AddDynamic(this, &UPGHomeScreenWidget::HandleStartDeliveryClicked);
	}
	if (ShopButton)
	{
		ShopButton->OnClicked.AddDynamic(this, &UPGHomeScreenWidget::HandleShopClicked);
	}
	if (SettingsButton)
	{
		SettingsButton->OnClicked.AddDynamic(this, &UPGHomeScreenWidget::HandleSettingsClicked);
	}
	if (HelpButton)
	{
		HelpButton->OnClicked.AddDynamic(this, &UPGHomeScreenWidget::HandleHelpClicked);
	}
	if (ReceiptAddButton)
	{
		ReceiptAddButton->OnClicked.AddDynamic(this, &UPGHomeScreenWidget::HandleReceiptAddClicked);
	}

	StartEntryAnimation();
}

void UPGHomeScreenWidget::NativeDestruct()
{
	if (StartDeliveryButton)
	{
		StartDeliveryButton->OnClicked.RemoveDynamic(this, &UPGHomeScreenWidget::HandleStartDeliveryClicked);
	}
	if (ShopButton)
	{
		ShopButton->OnClicked.RemoveDynamic(this, &UPGHomeScreenWidget::HandleShopClicked);
	}
	if (SettingsButton)
	{
		SettingsButton->OnClicked.RemoveDynamic(this, &UPGHomeScreenWidget::HandleSettingsClicked);
	}
	if (HelpButton)
	{
		HelpButton->OnClicked.RemoveDynamic(this, &UPGHomeScreenWidget::HandleHelpClicked);
	}
	if (ReceiptAddButton)
	{
		ReceiptAddButton->OnClicked.RemoveDynamic(this, &UPGHomeScreenWidget::HandleReceiptAddClicked);
	}

	Super::NativeDestruct();
}

void UPGHomeScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	ApplyReceiptShopUnavailableFeedback(InDeltaTime);

	if (bIsEntryAnimating)
	{
		EntryAnimationElapsed += InDeltaTime;
		const float Alpha = EntryAnimationDuration > KINDA_SMALL_NUMBER
			? FMath::Clamp(EntryAnimationElapsed / EntryAnimationDuration, 0.0f, 1.0f)
			: 1.0f;
		ApplyEntryAnimation(Alpha);

		if (Alpha >= 1.0f)
		{
			bIsEntryAnimating = false;
			SetHomeButtonsEnabled(true);
		}
	}

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
	bIsEntryAnimating = false;
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
	if (ShopButton)
	{
		ShopButton->SetIsEnabled(bEnabled);
	}
	if (SettingsButton)
	{
		SettingsButton->SetIsEnabled(bEnabled);
	}
	if (HelpButton)
	{
		HelpButton->SetIsEnabled(bEnabled);
	}
	if (ReceiptAddButton)
	{
		ReceiptAddButton->SetIsEnabled(bEnabled);
	}
}

void UPGHomeScreenWidget::SetReceiptBalance(int32 Balance) const
{
	if (ReceiptBalanceText)
	{
		ReceiptBalanceText->SetText(FText::AsNumber(FMath::Max(0, Balance)));
	}
}

void UPGHomeScreenWidget::SetBestRunScore(int32 Score) const
{
	if (BestScoreText)
	{
		BestScoreText->SetText(FText::AsNumber(FMath::Max(0, Score)));
	}
}

void UPGHomeScreenWidget::HandleStartDeliveryClicked()
{
	UPGAudioStatics::PlayCommonButtonClick(this);
	OnStartDeliveryRequested.Broadcast();
}

void UPGHomeScreenWidget::HandleShopClicked()
{
	UPGAudioStatics::PlayCommonButtonClick(this);
	OnShopRequested.Broadcast();
}

void UPGHomeScreenWidget::HandleSettingsClicked()
{
	UPGAudioStatics::PlayCommonButtonClick(this);
	OnSettingsRequested.Broadcast();
}

void UPGHomeScreenWidget::HandleHelpClicked()
{
	UPGAudioStatics::PlayCommonButtonClick(this);
	OnHelpRequested.Broadcast();
}

void UPGHomeScreenWidget::HandleReceiptAddClicked()
{
	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	if (Settings && Settings->Shop.IsReceiptShopTestGrantEnabled())
	{
		UPGAudioStatics::PlayCommonButtonClick(this);
		OnReceiptShopRequested.Broadcast();
		return;
	}

	// 공개 빌드에서는 가격 화면을 열지 않고 기존 구매 실패와 같은 피드백만 제공한다.
	if (Settings)
	{
		UPGAudioStatics::PlayUIButtonSound(this, Settings->SFX.ShopPurchaseFailSound.Get());
	}
	else
	{
		UPGAudioStatics::PlayCommonButtonClick(this);
	}

	bReceiptShopUnavailableFeedbackActive = true;
	ReceiptShopUnavailableFeedbackElapsed = 0.0f;
	ApplyReceiptShopUnavailableFeedback(0.0f);
}

void UPGHomeScreenWidget::ApplyReceiptShopUnavailableFeedback(float DeltaSeconds)
{
	if (!bReceiptShopUnavailableFeedbackActive || !ReceiptAddButton)
	{
		return;
	}

	ReceiptShopUnavailableFeedbackElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(
		ReceiptShopUnavailableFeedbackElapsed / FMath::Max(KINDA_SMALL_NUMBER, ReceiptShopUnavailableFeedbackDuration),
		0.0f,
		1.0f);
	const float Shake = FMath::Sin(Alpha * UE_PI * 6.0f) * 12.0f * (1.0f - Alpha);
	ReceiptAddButton->SetRenderTranslation(FVector2D(Shake, 0.0f));

	if (Alpha >= 1.0f)
	{
		bReceiptShopUnavailableFeedbackActive = false;
		ReceiptAddButton->SetRenderTranslation(FVector2D::ZeroVector);
		ReceiptAddButton->SetRenderScale(FVector2D(1.0f, 1.0f));
	}
}

void UPGHomeScreenWidget::StartEntryAnimation()
{
	EntryAnimationElapsed = 0.0f;
	bIsEntryAnimating = true;
	bIsExitAnimating = false;
	SetHomeButtonsEnabled(false);
	ApplyEntryAnimation(0.0f);
}

void UPGHomeScreenWidget::ApplyEntryAnimation(float Alpha)
{
	const float AnimationTime = FMath::Max(0.0f, EntryAnimationDuration) * FMath::Clamp(Alpha, 0.0f, 1.0f);
	ApplyWidgetAnimationState(LogoImage, LogoEntryOffset, LogoAnimationDelay, AnimationTime, false, 0.96f, 1.0f);
	ApplyWidgetAnimationState(MenuPanel, MenuEntryOffset, MenuAnimationDelay, AnimationTime, false, 0.96f, 1.0f);
	ApplyWidgetAnimationState(ReceiptPanel, ReceiptEntryOffset, ReceiptAnimationDelay, AnimationTime, false, 1.0f, 1.0f);
}

void UPGHomeScreenWidget::ApplyExitAnimation(float Alpha)
{
	const float AnimationTime = FMath::Max(0.0f, ExitAnimationDuration) * FMath::Clamp(Alpha, 0.0f, 1.0f);
	ApplyWidgetAnimationState(LogoImage, LogoExitOffset, LogoAnimationDelay, AnimationTime, true, 1.0f, 0.92f);
	ApplyWidgetAnimationState(MenuPanel, MenuExitOffset, MenuAnimationDelay, AnimationTime, true, 1.0f, 0.92f);
	ApplyWidgetAnimationState(ReceiptPanel, ReceiptExitOffset, ReceiptAnimationDelay, AnimationTime, true, 1.0f, 0.92f);
}

void UPGHomeScreenWidget::ApplyWidgetAnimationState(UWidget* Widget, const FVector2D& StartOffset, float Delay, float AnimationTime, bool bReverse, float StartScale, float EndScale) const
{
	if (!Widget)
	{
		return;
	}

	const float TotalDuration = bReverse ? ExitAnimationDuration : EntryAnimationDuration;
	const float EffectiveDuration = FMath::Max(KINDA_SMALL_NUMBER, TotalDuration - Delay);
	const float LocalAlpha = FMath::Clamp((AnimationTime - Delay) / EffectiveDuration, 0.0f, 1.0f);
	const float EasedAlpha = bReverse ? EaseInCubicHome(LocalAlpha) : EaseOutCubicHome(LocalAlpha);
	const float Scale = FMath::Lerp(StartScale, EndScale, EasedAlpha);
	const FVector2D Translation = bReverse
		? StartOffset * EasedAlpha
		: StartOffset * (1.0f - EasedAlpha);
	const float Opacity = bReverse ? 1.0f - EasedAlpha : EasedAlpha;

	Widget->SetRenderTranslation(Translation);
	Widget->SetRenderOpacity(Opacity);
	Widget->SetRenderScale(FVector2D(Scale, Scale));
}
