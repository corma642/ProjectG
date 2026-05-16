// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PGRunPauseWidget.h"

#include "Audio/PGAudioStatics.h"
#include "Components/BackgroundBlur.h"
#include "Components/Button.h"
#include "Components/Widget.h"

namespace
{
constexpr float PauseRootStartScale = 0.94f;
constexpr float PauseRootStartOffsetY = 36.0f;
constexpr float PausePanelDelay = 0.08f;
constexpr float PauseButtonDelay = 0.12f;
constexpr float PauseButtonStagger = 0.05f;
constexpr float PauseButtonSlideDistance = 220.0f;

float RunPauseEaseOutCubic(float Alpha)
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	return 1.0f - FMath::Pow(1.0f - ClampedAlpha, 3.0f);
}

float RunPauseDelayedAlpha(float Alpha, float Delay)
{
	if (Alpha <= Delay)
	{
		return 0.0f;
	}

	return FMath::Clamp((Alpha - Delay) / FMath::Max(1.0f - Delay, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
}

void ApplyRunPauseWidgetState(UWidget* Widget, float Alpha, const FVector2D& StartTranslation)
{
	if (!Widget)
	{
		return;
	}

	const float EasedAlpha = RunPauseEaseOutCubic(Alpha);
	Widget->SetRenderOpacity(EasedAlpha);
	Widget->SetRenderTranslation(FMath::Lerp(StartTranslation, FVector2D::ZeroVector, EasedAlpha));
}
}

void UPGRunPauseWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ResumeButton)
	{
		ResumeButton->OnClicked.AddDynamic(this, &UPGRunPauseWidget::HandleResumeClicked);
	}
	if (SettingsButton)
	{
		SettingsButton->OnClicked.AddDynamic(this, &UPGRunPauseWidget::HandleSettingsClicked);
	}
	if (HomeButton)
	{
		HomeButton->OnClicked.AddDynamic(this, &UPGRunPauseWidget::HandleHomeClicked);
	}

	ApplyAnimationState(0.0f, false);
}

void UPGRunPauseWidget::NativeDestruct()
{
	if (ResumeButton)
	{
		ResumeButton->OnClicked.RemoveDynamic(this, &UPGRunPauseWidget::HandleResumeClicked);
	}
	if (SettingsButton)
	{
		SettingsButton->OnClicked.RemoveDynamic(this, &UPGRunPauseWidget::HandleSettingsClicked);
	}
	if (HomeButton)
	{
		HomeButton->OnClicked.RemoveDynamic(this, &UPGRunPauseWidget::HandleHomeClicked);
	}

	Super::NativeDestruct();
}

void UPGRunPauseWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bIsOpening && !bIsClosingForResume)
	{
		return;
	}

	AnimationElapsed += InDeltaTime;
	const float Alpha = AnimationDuration > KINDA_SMALL_NUMBER
		? FMath::Clamp(AnimationElapsed / AnimationDuration, 0.0f, 1.0f)
		: 1.0f;
	ApplyAnimationState(RunPauseEaseOutCubic(Alpha), bIsClosingForResume);

	if (Alpha >= 1.0f)
	{
		const bool bResumeAfterClose = bIsClosingForResume;
		bIsOpening = false;
		bIsClosingForResume = false;
		if (bResumeAfterClose)
		{
			OnResumeRequested.Broadcast();
		}
	}
}

void UPGRunPauseWidget::PlayOpenAnimation(float InTargetBlurStrength, float InTargetDimAlpha)
{
	TargetBlurStrength = FMath::Max(0.0f, InTargetBlurStrength);
	TargetDimAlpha = FMath::Clamp(InTargetDimAlpha, 0.0f, 1.0f);
	bIsOpening = true;
	bIsClosingForResume = false;
	AnimationElapsed = 0.0f;
	SetPauseButtonsEnabled(false);
	ApplyAnimationState(0.0f, false);
}

void UPGRunPauseWidget::PlayCloseForResume()
{
	bIsOpening = false;
	bIsClosingForResume = true;
	AnimationElapsed = 0.0f;
	SetPauseButtonsEnabled(false);
	ApplyAnimationState(0.0f, true);
}

void UPGRunPauseWidget::SetPauseButtonsEnabled(bool bEnabled) const
{
	if (ResumeButton)
	{
		ResumeButton->SetIsEnabled(bEnabled);
	}
	if (SettingsButton)
	{
		SettingsButton->SetIsEnabled(bEnabled);
	}
	if (HomeButton)
	{
		HomeButton->SetIsEnabled(bEnabled);
	}
}

void UPGRunPauseWidget::HandleResumeClicked()
{
	UPGAudioStatics::PlayCommonButtonClick(this);
	PlayCloseForResume();
}

void UPGRunPauseWidget::HandleSettingsClicked()
{
	UPGAudioStatics::PlayCommonButtonClick(this);
	OnSettingsRequested.Broadcast();
}

void UPGRunPauseWidget::HandleHomeClicked()
{
	UPGAudioStatics::PlayCommonButtonClick(this);
	SetPauseButtonsEnabled(false);
	OnHomeRequested.Broadcast();
}

void UPGRunPauseWidget::ApplyAnimationState(float Alpha, bool bClosing)
{
	const float EffectiveAlpha = bClosing ? 1.0f - Alpha : Alpha;

	if (BackdropBlur)
	{
		BackdropBlur->SetBlurStrength(TargetBlurStrength * EffectiveAlpha);
	}

	if (BackdropDim)
	{
		BackdropDim->SetRenderOpacity(TargetDimAlpha * EffectiveAlpha);
	}

	if (PauseRootPanel)
	{
		PauseRootPanel->SetRenderOpacity(EffectiveAlpha);
		const float Scale = FMath::Lerp(PauseRootStartScale, 1.0f, EffectiveAlpha);
		PauseRootPanel->SetRenderScale(FVector2D(Scale, Scale));
		PauseRootPanel->SetRenderTranslation(FVector2D(0.0f, FMath::Lerp(PauseRootStartOffsetY, 0.0f, EffectiveAlpha)));
	}

	ApplyRunPauseWidgetState(PausePanel, RunPauseDelayedAlpha(EffectiveAlpha, PausePanelDelay), FVector2D::ZeroVector);
	ApplyRunPauseWidgetState(ResumeButton, RunPauseDelayedAlpha(EffectiveAlpha, PauseButtonDelay), FVector2D(-PauseButtonSlideDistance, 0.0f));
	ApplyRunPauseWidgetState(SettingsButton, RunPauseDelayedAlpha(EffectiveAlpha, PauseButtonDelay + PauseButtonStagger), FVector2D(PauseButtonSlideDistance, 0.0f));
	ApplyRunPauseWidgetState(HomeButton, RunPauseDelayedAlpha(EffectiveAlpha, PauseButtonDelay + PauseButtonStagger * 2.0f), FVector2D(0.0f, PauseButtonSlideDistance));

	if (!bClosing && Alpha >= 1.0f)
	{
		SetPauseButtonsEnabled(true);
	}
}
