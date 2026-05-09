// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PGSettingsScreenWidget.h"

#include "Components/BackgroundBlur.h"
#include "Components/Button.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "GameFramework/GameUserSettings.h"
#include "Settings/PGGameUserSettings.h"

namespace
{
constexpr float MinResolutionScalePercent = 50.0f;
constexpr float MaxResolutionScalePercent = 100.0f;
constexpr float SettingsBackdropBlurStrength = 5.0f;
constexpr float SettingsBackdropDimOpacity = 0.35f;
constexpr float SettingsPanelStartScale = 0.94f;
constexpr float SettingsPanelStartOffsetY = 40.0f;

float EaseOutCubicSettings(float Alpha)
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	const float InverseAlpha = 1.0f - ClampedAlpha;
	return 1.0f - InverseAlpha * InverseAlpha * InverseAlpha;
}
}

void UPGSettingsScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (CloseButton)
	{
		CloseButton->OnClicked.AddDynamic(this, &UPGSettingsScreenWidget::HandleCloseClicked);
	}
	if (LowPresetButton)
	{
		LowPresetButton->OnClicked.AddDynamic(this, &UPGSettingsScreenWidget::HandleLowPresetClicked);
	}
	if (BalancedPresetButton)
	{
		BalancedPresetButton->OnClicked.AddDynamic(this, &UPGSettingsScreenWidget::HandleBalancedPresetClicked);
	}
	if (HighPresetButton)
	{
		HighPresetButton->OnClicked.AddDynamic(this, &UPGSettingsScreenWidget::HandleHighPresetClicked);
	}
	if (UltraPresetButton)
	{
		UltraPresetButton->OnClicked.AddDynamic(this, &UPGSettingsScreenWidget::HandleUltraPresetClicked);
	}
	if (ResolutionScaleSlider)
	{
		ResolutionScaleSlider->SetMinValue(MinResolutionScalePercent);
		ResolutionScaleSlider->SetMaxValue(MaxResolutionScalePercent);
		ResolutionScaleSlider->OnValueChanged.AddDynamic(this, &UPGSettingsScreenWidget::HandleResolutionScaleChanged);
		ResolutionScaleSlider->OnMouseCaptureEnd.AddDynamic(this, &UPGSettingsScreenWidget::HandleResolutionScaleCommit);
		ResolutionScaleSlider->OnControllerCaptureEnd.AddDynamic(this, &UPGSettingsScreenWidget::HandleResolutionScaleCommit);
	}
	if (FrameRate30Button)
	{
		FrameRate30Button->OnClicked.AddDynamic(this, &UPGSettingsScreenWidget::HandleFrameRate30Clicked);
	}
	if (FrameRate60Button)
	{
		FrameRate60Button->OnClicked.AddDynamic(this, &UPGSettingsScreenWidget::HandleFrameRate60Clicked);
	}

	RefreshControlsFromSettings();
	ApplyAnimationState(0.0f);
}

void UPGSettingsScreenWidget::NativeDestruct()
{
	if (CloseButton)
	{
		CloseButton->OnClicked.RemoveDynamic(this, &UPGSettingsScreenWidget::HandleCloseClicked);
	}
	if (LowPresetButton)
	{
		LowPresetButton->OnClicked.RemoveDynamic(this, &UPGSettingsScreenWidget::HandleLowPresetClicked);
	}
	if (BalancedPresetButton)
	{
		BalancedPresetButton->OnClicked.RemoveDynamic(this, &UPGSettingsScreenWidget::HandleBalancedPresetClicked);
	}
	if (HighPresetButton)
	{
		HighPresetButton->OnClicked.RemoveDynamic(this, &UPGSettingsScreenWidget::HandleHighPresetClicked);
	}
	if (UltraPresetButton)
	{
		UltraPresetButton->OnClicked.RemoveDynamic(this, &UPGSettingsScreenWidget::HandleUltraPresetClicked);
	}
	if (ResolutionScaleSlider)
	{
		ResolutionScaleSlider->OnValueChanged.RemoveDynamic(this, &UPGSettingsScreenWidget::HandleResolutionScaleChanged);
		ResolutionScaleSlider->OnMouseCaptureEnd.RemoveDynamic(this, &UPGSettingsScreenWidget::HandleResolutionScaleCommit);
		ResolutionScaleSlider->OnControllerCaptureEnd.RemoveDynamic(this, &UPGSettingsScreenWidget::HandleResolutionScaleCommit);
	}
	if (FrameRate30Button)
	{
		FrameRate30Button->OnClicked.RemoveDynamic(this, &UPGSettingsScreenWidget::HandleFrameRate30Clicked);
	}
	if (FrameRate60Button)
	{
		FrameRate60Button->OnClicked.RemoveDynamic(this, &UPGSettingsScreenWidget::HandleFrameRate60Clicked);
	}

	Super::NativeDestruct();
}

void UPGSettingsScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bIsOpening && !bIsClosing)
	{
		return;
	}

	AnimationElapsed += InDeltaTime;
	const float RawAlpha = AnimationDuration > KINDA_SMALL_NUMBER
		? FMath::Clamp(AnimationElapsed / AnimationDuration, 0.0f, 1.0f)
		: 1.0f;
	const float AnimationAlpha = bIsClosing ? 1.0f - RawAlpha : RawAlpha;
	ApplyAnimationState(AnimationAlpha);

	if (RawAlpha >= 1.0f)
	{
		if (bIsClosing)
		{
			OnSettingsScreenClosed.Broadcast();
			RemoveFromParent();
		}
		else
		{
			SetAllButtonsEnabled(true);
			RefreshControlsFromSettings();
		}

		bIsOpening = false;
		bIsClosing = false;
	}
}

void UPGSettingsScreenWidget::PlayOpenAnimation()
{
	bIsOpening = true;
	bIsClosing = false;
	AnimationElapsed = 0.0f;
	SetAllButtonsEnabled(false);
	ApplyAnimationState(0.0f);
}

void UPGSettingsScreenWidget::HandleCloseClicked()
{
	if (UPGGameUserSettings* UserSettings = Cast<UPGGameUserSettings>(UGameUserSettings::GetGameUserSettings()))
	{
		UserSettings->SaveSettings();
	}

	bIsOpening = false;
	bIsClosing = true;
	AnimationElapsed = 0.0f;
	SetAllButtonsEnabled(false);
}

void UPGSettingsScreenWidget::HandleLowPresetClicked()
{
	ApplyPreset(EPGMobileQualityPreset::Low);
}

void UPGSettingsScreenWidget::HandleBalancedPresetClicked()
{
	ApplyPreset(EPGMobileQualityPreset::Balanced);
}

void UPGSettingsScreenWidget::HandleHighPresetClicked()
{
	ApplyPreset(EPGMobileQualityPreset::High);
}

void UPGSettingsScreenWidget::HandleUltraPresetClicked()
{
	ApplyPreset(EPGMobileQualityPreset::Ultra);
}

void UPGSettingsScreenWidget::HandleResolutionScaleChanged(float Value)
{
	if (bIsUpdatingControls)
	{
		return;
	}

	UpdateResolutionScaleText(Value);
	if (UPGGameUserSettings* UserSettings = Cast<UPGGameUserSettings>(UGameUserSettings::GetGameUserSettings()))
	{
		UserSettings->SetMobileResolutionScalePercent(Value, false);
	}
}

void UPGSettingsScreenWidget::HandleResolutionScaleCommit()
{
	if (ResolutionScaleSlider)
	{
		if (UPGGameUserSettings* UserSettings = Cast<UPGGameUserSettings>(UGameUserSettings::GetGameUserSettings()))
		{
			UserSettings->SetMobileResolutionScalePercent(ResolutionScaleSlider->GetValue(), true);
		}
	}
}

void UPGSettingsScreenWidget::HandleFrameRate30Clicked()
{
	ApplyFrameRate(30);
}

void UPGSettingsScreenWidget::HandleFrameRate60Clicked()
{
	ApplyFrameRate(60);
}

void UPGSettingsScreenWidget::RefreshControlsFromSettings()
{
	UPGGameUserSettings* UserSettings = Cast<UPGGameUserSettings>(UGameUserSettings::GetGameUserSettings());
	if (!UserSettings)
	{
		return;
	}

	bIsUpdatingControls = true;
	if (ResolutionScaleSlider)
	{
		ResolutionScaleSlider->SetValue(UserSettings->GetMobileResolutionScalePercent());
	}
	UpdateResolutionScaleText(UserSettings->GetMobileResolutionScalePercent());
	bIsUpdatingControls = false;

	SetPresetSelectionState(UserSettings->GetSelectedMobileQualityPreset());
	SetFrameRateSelectionState(UserSettings->GetMobileFrameRateLimit());
}

void UPGSettingsScreenWidget::ApplyPreset(EPGMobileQualityPreset Preset)
{
	if (UPGGameUserSettings* UserSettings = Cast<UPGGameUserSettings>(UGameUserSettings::GetGameUserSettings()))
	{
		UserSettings->ApplyMobileQualityPreset(Preset, true);
		RefreshControlsFromSettings();
	}
}

void UPGSettingsScreenWidget::ApplyFrameRate(int32 FrameRateLimit)
{
	if (UPGGameUserSettings* UserSettings = Cast<UPGGameUserSettings>(UGameUserSettings::GetGameUserSettings()))
	{
		UserSettings->SetMobileFrameRateLimit(FrameRateLimit, true);
		SetFrameRateSelectionState(UserSettings->GetMobileFrameRateLimit());
	}
}

void UPGSettingsScreenWidget::ApplyAnimationState(float Alpha)
{
	if (RootPanel)
	{
		RootPanel->SetRenderOpacity(1.0f);
		RootPanel->SetRenderScale(FVector2D(1.0f, 1.0f));
		RootPanel->SetRenderTranslation(FVector2D::ZeroVector);
	}

	const float EasedAlpha = EaseOutCubicSettings(Alpha);

	if (BackgroundBlur)
	{
		BackgroundBlur->SetBlurStrength(SettingsBackdropBlurStrength * EasedAlpha);
	}

	if (BackdropDim)
	{
		BackdropDim->SetRenderOpacity(SettingsBackdropDimOpacity * EasedAlpha);
	}

	if (SettingsPanel)
	{
		const float Scale = FMath::Lerp(SettingsPanelStartScale, 1.0f, EasedAlpha);
		SettingsPanel->SetRenderOpacity(EasedAlpha);
		SettingsPanel->SetRenderScale(FVector2D(Scale, Scale));
		SettingsPanel->SetRenderTranslation(FVector2D(0.0f, FMath::Lerp(SettingsPanelStartOffsetY, 0.0f, EasedAlpha)));
	}
}

void UPGSettingsScreenWidget::SetAllButtonsEnabled(bool bEnabled) const
{
	if (CloseButton)
	{
		CloseButton->SetIsEnabled(bEnabled);
	}
	if (LowPresetButton)
	{
		LowPresetButton->SetIsEnabled(bEnabled);
	}
	if (BalancedPresetButton)
	{
		BalancedPresetButton->SetIsEnabled(bEnabled);
	}
	if (HighPresetButton)
	{
		HighPresetButton->SetIsEnabled(bEnabled);
	}
	if (UltraPresetButton)
	{
		UltraPresetButton->SetIsEnabled(bEnabled);
	}
	if (FrameRate30Button)
	{
		FrameRate30Button->SetIsEnabled(bEnabled);
	}
	if (FrameRate60Button)
	{
		FrameRate60Button->SetIsEnabled(bEnabled);
	}
	if (ResolutionScaleSlider)
	{
		ResolutionScaleSlider->SetIsEnabled(bEnabled);
	}
}

void UPGSettingsScreenWidget::SetPresetSelectionState(EPGMobileQualityPreset Preset) const
{
	if (LowPresetButton)
	{
		LowPresetButton->SetIsEnabled(Preset != EPGMobileQualityPreset::Low);
	}
	if (BalancedPresetButton)
	{
		BalancedPresetButton->SetIsEnabled(Preset != EPGMobileQualityPreset::Balanced);
	}
	if (HighPresetButton)
	{
		HighPresetButton->SetIsEnabled(Preset != EPGMobileQualityPreset::High);
	}
	if (UltraPresetButton)
	{
		UltraPresetButton->SetIsEnabled(Preset != EPGMobileQualityPreset::Ultra);
	}
}

void UPGSettingsScreenWidget::SetFrameRateSelectionState(int32 InFrameRateLimit) const
{
	if (FrameRate30Button)
	{
		FrameRate30Button->SetIsEnabled(InFrameRateLimit != 30);
	}
	if (FrameRate60Button)
	{
		FrameRate60Button->SetIsEnabled(InFrameRateLimit != 60);
	}
}

void UPGSettingsScreenWidget::UpdateResolutionScaleText(float Value) const
{
	if (ResolutionScaleValueText)
	{
		ResolutionScaleValueText->SetText(FText::Format(
			NSLOCTEXT("PGSettingsScreen", "ResolutionScaleFormat", "{0}%"),
			FText::AsNumber(FMath::RoundToInt(Value))));
	}
}
