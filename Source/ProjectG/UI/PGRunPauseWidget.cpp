// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PGRunPauseWidget.h"

#include "Components/BackgroundBlur.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Widget.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGRunPauseWidget, Log, All);

namespace
{
float RunPauseEaseOutCubic(float Alpha)
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	return 1.0f - FMath::Pow(1.0f - ClampedAlpha, 3.0f);
}
}

void UPGRunPauseWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ValidateRequiredWidgets();
	if (ResumeButton)
	{
		ResumeButton->OnClicked.AddDynamic(this, &UPGRunPauseWidget::HandleResumeClicked);
	}
	if (SettingsButton)
	{
		SettingsButton->OnClicked.AddDynamic(this, &UPGRunPauseWidget::HandleSettingsClicked);
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
}

void UPGRunPauseWidget::HandleResumeClicked()
{
	PlayCloseForResume();
}

void UPGRunPauseWidget::HandleSettingsClicked()
{
	OnSettingsRequested.Broadcast();
}

void UPGRunPauseWidget::ValidateRequiredWidgets() const
{
	if (!RootPanel)
	{
		UE_LOG(LogPGRunPauseWidget, Warning, TEXT("%s requires BindWidget 'RootPanel'."), *GetNameSafe(this));
	}
	if (!BackdropBlur)
	{
		UE_LOG(LogPGRunPauseWidget, Warning, TEXT("%s requires BindWidget 'BackdropBlur'."), *GetNameSafe(this));
	}
	if (!BackdropDim)
	{
		UE_LOG(LogPGRunPauseWidget, Warning, TEXT("%s requires BindWidget 'BackdropDim'."), *GetNameSafe(this));
	}
	if (!PausePanel)
	{
		UE_LOG(LogPGRunPauseWidget, Warning, TEXT("%s requires BindWidget 'PausePanel'."), *GetNameSafe(this));
	}
	if (!ResumeButton)
	{
		UE_LOG(LogPGRunPauseWidget, Warning, TEXT("%s requires BindWidget 'ResumeButton'."), *GetNameSafe(this));
	}
	if (!SettingsButton)
	{
		UE_LOG(LogPGRunPauseWidget, Warning, TEXT("%s requires BindWidget 'SettingsButton'."), *GetNameSafe(this));
	}
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
		BackdropDim->SetColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, TargetDimAlpha * EffectiveAlpha));
	}

	if (PausePanel)
	{
		PausePanel->SetRenderOpacity(EffectiveAlpha);
		const float Scale = FMath::Lerp(0.92f, 1.0f, EffectiveAlpha);
		PausePanel->SetRenderScale(FVector2D(Scale, Scale));
		PausePanel->SetRenderTranslation(FVector2D(0.0f, FMath::Lerp(30.0f, 0.0f, EffectiveAlpha)));
	}

	if (!bClosing && Alpha >= 1.0f)
	{
		SetPauseButtonsEnabled(true);
	}
}
