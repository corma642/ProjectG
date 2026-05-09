// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PGRunResultWidget.h"

#include "Components/BackgroundBlur.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Kismet/GameplayStatics.h"

namespace
{
constexpr float BackdropBlurStrength = 8.0f;
constexpr float BackdropDimOpacity = 0.45f;
constexpr float BackdropAnimationDuration = 0.45f;
constexpr float BackdropResultStartAlpha = 0.8f;
constexpr float RootPanelStartYOffset = -1000.0f;
constexpr float RowStartXOffset = -700.0f;
constexpr float ButtonStartYOffset = 420.0f;
constexpr float RootPanelAnimationDuration = 0.45f;
constexpr float BackgroundAnimationDuration = 0.35f;
constexpr float TitleAnimationDuration = 0.35f;
constexpr float TitleAnimationDelay = 0.16f;
constexpr float RowAnimationDuration = 0.28f;
constexpr float RowAnimationDelay = 0.09f;
constexpr float RowAnimationStartDelay = 0.26f;
constexpr float ButtonAnimationDuration = 0.28f;
constexpr float ButtonAnimationDelay = 0.12f;
constexpr float ButtonAnimationStartDelay = 0.72f;
constexpr float ButtonNormalScale = 1.0f;
constexpr float ButtonPressedScale = 0.96f;
constexpr float BackgroundStartScale = 0.96f;
constexpr float TitleStartScale = 1.35f;
constexpr float TitleEndScale = 1.0f;

float RunResultEaseOutCubic(float Alpha)
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	return 1.0f - FMath::Pow(1.0f - ClampedAlpha, 3.0f);
}
}

void UPGRunResultWidget::NativeConstruct()
{
	Super::NativeConstruct();

	AnimatedRows.Reset();
	AnimatedRows.Add(ScoreRow);
	AnimatedRows.Add(DistanceRow);
	AnimatedRows.Add(ReceiptsRow);
	AnimatedRows.Add(OrdersRow);

	AnimatedButtons.Reset();
	AnimatedButtons.Add(RetryButtonPanel);
	AnimatedButtons.Add(HomeButtonPanel);

	if (RetryButton)
	{
		RetryButton->OnClicked.AddDynamic(this, &UPGRunResultWidget::HandleRetryClicked);
		RetryButton->OnPressed.AddDynamic(this, &UPGRunResultWidget::HandleRetryPressed);
		RetryButton->OnReleased.AddDynamic(this, &UPGRunResultWidget::HandleRetryReleased);
	}
	if (HomeButton)
	{
		HomeButton->OnClicked.AddDynamic(this, &UPGRunResultWidget::HandleHomeClicked);
		HomeButton->OnPressed.AddDynamic(this, &UPGRunResultWidget::HandleHomePressed);
		HomeButton->OnReleased.AddDynamic(this, &UPGRunResultWidget::HandleHomeReleased);
	}

	ApplyCachedResultTexts();
	StartBackdropAnimation();
}

void UPGRunResultWidget::NativeDestruct()
{
	if (RetryButton)
	{
		RetryButton->OnClicked.RemoveDynamic(this, &UPGRunResultWidget::HandleRetryClicked);
		RetryButton->OnPressed.RemoveDynamic(this, &UPGRunResultWidget::HandleRetryPressed);
		RetryButton->OnReleased.RemoveDynamic(this, &UPGRunResultWidget::HandleRetryReleased);
	}
	if (HomeButton)
	{
		HomeButton->OnClicked.RemoveDynamic(this, &UPGRunResultWidget::HandleHomeClicked);
		HomeButton->OnPressed.RemoveDynamic(this, &UPGRunResultWidget::HandleHomePressed);
		HomeButton->OnReleased.RemoveDynamic(this, &UPGRunResultWidget::HandleHomeReleased);
	}

	Super::NativeDestruct();
}

void UPGRunResultWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bBackdropAnimationFinished)
	{
		UpdateBackdropAnimation(InDeltaTime);
	}

	if (bResultEntryStarted && !bEntryAnimationFinished)
	{
		UpdateEntryAnimation(InDeltaTime);
	}
}

void UPGRunResultWidget::ConfigureRunResult(FName FailureReason, float RunTimeSeconds, float DistanceMeters, int32 Score, int32 Receipts, int32 OrdersDelivered)
{
	CachedFailureReason = FailureReason;
	CachedRunTimeSeconds = RunTimeSeconds;
	CachedDistanceMeters = DistanceMeters;
	CachedScore = Score;
	CachedReceipts = Receipts;
	CachedOrdersDelivered = OrdersDelivered;

	ApplyCachedResultTexts();
	StartBackdropAnimation();
	OnRunResultConfigured(FailureReason, RunTimeSeconds, DistanceMeters, Score, Receipts, OrdersDelivered);
}

void UPGRunResultWidget::ApplyCachedResultTexts()
{
	if (ScoreValueText)
	{
		ScoreValueText->SetText(FormatNumber(CachedScore));
	}
	if (DistanceValueText)
	{
		DistanceValueText->SetText(FText::Format(NSLOCTEXT("PGRunResult", "DistanceMetersFormat", "{0} m"), FormatNumber(FMath::RoundToInt(CachedDistanceMeters))));
	}
	if (ReceiptsValueText)
	{
		ReceiptsValueText->SetText(FormatNumber(CachedReceipts));
	}
	if (OrdersValueText)
	{
		OrdersValueText->SetText(FormatNumber(CachedOrdersDelivered));
	}
}

void UPGRunResultWidget::StartBackdropAnimation()
{
	BackdropAnimationElapsed = 0.0f;
	EntryAnimationElapsed = 0.0f;
	bBackdropAnimationFinished = false;
	bResultEntryStarted = false;
	bEntryAnimationFinished = true;

	if (BackdropBlur)
	{
		BackdropBlur->SetBlurStrength(0.0f);
	}
	if (BackdropDim)
	{
		BackdropDim->SetRenderOpacity(0.0f);
	}

	ResetEntryAnimationState();
}

void UPGRunResultWidget::UpdateBackdropAnimation(float DeltaTime)
{
	BackdropAnimationElapsed += DeltaTime;

	const float Alpha = FMath::Clamp(BackdropAnimationElapsed / BackdropAnimationDuration, 0.0f, 1.0f);
	const float EasedAlpha = RunResultEaseOutCubic(Alpha);

	if (BackdropBlur)
	{
		BackdropBlur->SetBlurStrength(BackdropBlurStrength * EasedAlpha);
	}
	if (BackdropDim)
	{
		BackdropDim->SetRenderOpacity(BackdropDimOpacity * EasedAlpha);
	}

	if (!bResultEntryStarted && Alpha >= BackdropResultStartAlpha)
	{
		StartEntryAnimation();
	}

	if (Alpha >= 1.0f)
	{
		bBackdropAnimationFinished = true;
	}
}

void UPGRunResultWidget::StartEntryAnimation()
{
	EntryAnimationElapsed = 0.0f;
	bResultEntryStarted = true;
	bEntryAnimationFinished = false;
	SetActionButtonsEnabled(false);
	ResetEntryAnimationState();
}

void UPGRunResultWidget::ResetEntryAnimationState()
{
	ApplyWidgetAnimationState(ResultRootPanel, FVector2D(0.0f, RootPanelStartYOffset), 0.0f);
	ApplyWidgetAnimationState(ResultPanel, FVector2D::ZeroVector, 0.0f);
	ApplyWidgetAnimationState(TitleText, FVector2D::ZeroVector, 0.0f);
	ApplyWidgetScale(ResultPanel, BackgroundStartScale);
	ApplyWidgetScale(TitleText, TitleStartScale);

	for (UWidget* RowWidget : AnimatedRows)
	{
		ApplyWidgetAnimationState(RowWidget, FVector2D(RowStartXOffset, 0.0f), 0.0f);
		ApplyWidgetScale(RowWidget, 1.0f);
	}

	for (UWidget* ButtonWidget : AnimatedButtons)
	{
		ApplyWidgetAnimationState(ButtonWidget, FVector2D(0.0f, ButtonStartYOffset), 0.0f);
	}

	SetActionButtonsEnabled(false);
	ApplyWidgetScale(RetryButtonPanel ? RetryButtonPanel.Get() : RetryButton.Get(), ButtonNormalScale);
	ApplyWidgetScale(HomeButtonPanel ? HomeButtonPanel.Get() : HomeButton.Get(), ButtonNormalScale);
}

void UPGRunResultWidget::UpdateEntryAnimation(float DeltaTime)
{
	EntryAnimationElapsed += DeltaTime;

	const float RootAlpha = RunResultEaseOutCubic(EntryAnimationElapsed / RootPanelAnimationDuration);
	ApplyWidgetAnimationState(ResultRootPanel, FVector2D(0.0f, FMath::Lerp(RootPanelStartYOffset, 0.0f, RootAlpha)), RootAlpha);

	const float BackgroundAlpha = RunResultEaseOutCubic(EntryAnimationElapsed / BackgroundAnimationDuration);
	ApplyWidgetAnimationState(ResultPanel, FVector2D::ZeroVector, BackgroundAlpha);
	ApplyWidgetScale(ResultPanel, FMath::Lerp(BackgroundStartScale, 1.0f, BackgroundAlpha));

	const float TitleTime = EntryAnimationElapsed - TitleAnimationDelay;
	const float TitleAlpha = RunResultEaseOutCubic(TitleTime / TitleAnimationDuration);
	ApplyWidgetAnimationState(TitleText, FVector2D::ZeroVector, TitleAlpha);
	ApplyWidgetScale(TitleText, FMath::Lerp(TitleStartScale, TitleEndScale, TitleAlpha));

	bool bAllRowsFinished = true;
	for (int32 Index = 0; Index < AnimatedRows.Num(); ++Index)
	{
		const float RowTime = EntryAnimationElapsed - RowAnimationStartDelay - static_cast<float>(Index) * RowAnimationDelay;
		const float RowAlpha = RunResultEaseOutCubic(RowTime / RowAnimationDuration);
		bAllRowsFinished &= RowAlpha >= 1.0f;
		ApplyWidgetAnimationState(AnimatedRows[Index], FVector2D(FMath::Lerp(RowStartXOffset, 0.0f, RowAlpha), 0.0f), RowAlpha);
	}

	bool bAllButtonsFinished = true;
	for (int32 Index = 0; Index < AnimatedButtons.Num(); ++Index)
	{
		const float ButtonTime = EntryAnimationElapsed - ButtonAnimationStartDelay - static_cast<float>(Index) * ButtonAnimationDelay;
		const float ButtonAlpha = RunResultEaseOutCubic(ButtonTime / ButtonAnimationDuration);
		bAllButtonsFinished &= ButtonAlpha >= 1.0f;
		ApplyWidgetAnimationState(AnimatedButtons[Index], FVector2D(0.0f, FMath::Lerp(ButtonStartYOffset, 0.0f, ButtonAlpha)), ButtonAlpha);
	}

	if (RootAlpha >= 1.0f && BackgroundAlpha >= 1.0f && TitleAlpha >= 1.0f && bAllRowsFinished && bAllButtonsFinished)
	{
		bEntryAnimationFinished = true;
		SetActionButtonsEnabled(true);
	}
}

void UPGRunResultWidget::ApplyWidgetAnimationState(UWidget* Widget, const FVector2D& Offset, float Opacity) const
{
	if (Widget)
	{
		Widget->SetRenderTranslation(Offset);
		Widget->SetRenderOpacity(FMath::Clamp(Opacity, 0.0f, 1.0f));
	}
}

void UPGRunResultWidget::SetActionButtonsEnabled(bool bEnabled) const
{
	if (RetryButton)
	{
		RetryButton->SetIsEnabled(bEnabled);
	}
	if (HomeButton)
	{
		HomeButton->SetIsEnabled(bEnabled);
	}
}

void UPGRunResultWidget::ApplyWidgetScale(UWidget* Widget, float Scale) const
{
	if (Widget)
	{
		const float ClampedScale = FMath::Max(0.01f, Scale);
		Widget->SetRenderScale(FVector2D(ClampedScale, ClampedScale));
	}
}

FText UPGRunResultWidget::FormatNumber(int32 Value) const
{
	FNumberFormattingOptions FormattingOptions;
	FormattingOptions.UseGrouping = true;
	FormattingOptions.MaximumFractionalDigits = 0;
	return FText::AsNumber(Value, &FormattingOptions);
}

void UPGRunResultWidget::HandleRetryClicked()
{
	const FString CurrentLevelName = UGameplayStatics::GetCurrentLevelName(this, true);
	if (!CurrentLevelName.IsEmpty())
	{
		UGameplayStatics::OpenLevel(this, FName(*CurrentLevelName), true, TEXT("StartFlow=RetryCountdown"));
	}
}

void UPGRunResultWidget::HandleRetryPressed()
{
	ApplyWidgetScale(RetryButtonPanel ? RetryButtonPanel.Get() : RetryButton.Get(), ButtonPressedScale);
}

void UPGRunResultWidget::HandleRetryReleased()
{
	ApplyWidgetScale(RetryButtonPanel ? RetryButtonPanel.Get() : RetryButton.Get(), ButtonNormalScale);
}

void UPGRunResultWidget::HandleHomeClicked()
{
	const FString CurrentLevelName = UGameplayStatics::GetCurrentLevelName(this, true);
	if (!CurrentLevelName.IsEmpty())
	{
		UGameplayStatics::OpenLevel(this, FName(*CurrentLevelName));
	}
}

void UPGRunResultWidget::HandleHomePressed()
{
	ApplyWidgetScale(HomeButtonPanel ? HomeButtonPanel.Get() : HomeButton.Get(), ButtonPressedScale);
}

void UPGRunResultWidget::HandleHomeReleased()
{
	ApplyWidgetScale(HomeButtonPanel ? HomeButtonPanel.Get() : HomeButton.Get(), ButtonNormalScale);
}
