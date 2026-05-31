// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PGSettingsScreenWidget.h"

#include "Audio/PGAudioStatics.h"
#include "Components/BackgroundBlur.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "GameFramework/GameUserSettings.h"
#include "Settings/PGGameUserSettings.h"

namespace
{
constexpr float MinResolutionScalePercent = 50.0f;
constexpr float MaxResolutionScalePercent = 100.0f;
constexpr float MinVolumePercent = 0.0f;
constexpr float MaxVolumePercent = 150.0f;
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
	if (ResolutionScaleSlider)
	{
		ResolutionScaleSlider->SetMinValue(MinResolutionScalePercent);
		ResolutionScaleSlider->SetMaxValue(MaxResolutionScalePercent);
		ResolutionScaleSlider->OnValueChanged.AddDynamic(this, &UPGSettingsScreenWidget::HandleResolutionScaleChanged);
		ResolutionScaleSlider->OnMouseCaptureEnd.AddDynamic(this, &UPGSettingsScreenWidget::HandleResolutionScaleCommit);
		ResolutionScaleSlider->OnControllerCaptureEnd.AddDynamic(this, &UPGSettingsScreenWidget::HandleResolutionScaleCommit);
	}
	if (ShowFPSCheckBox)
	{
		ShowFPSCheckBox->OnCheckStateChanged.AddDynamic(this, &UPGSettingsScreenWidget::HandleShowFPSCheckStateChanged);
	}
	if (MusicVolumeSlider)
	{
		MusicVolumeSlider->SetMinValue(MinVolumePercent);
		MusicVolumeSlider->SetMaxValue(MaxVolumePercent);
		MusicVolumeSlider->OnValueChanged.AddDynamic(this, &UPGSettingsScreenWidget::HandleMusicVolumeChanged);
		MusicVolumeSlider->OnMouseCaptureEnd.AddDynamic(this, &UPGSettingsScreenWidget::HandleMusicVolumeCommit);
		MusicVolumeSlider->OnControllerCaptureEnd.AddDynamic(this, &UPGSettingsScreenWidget::HandleMusicVolumeCommit);
	}
	if (SFXVolumeSlider)
	{
		SFXVolumeSlider->SetMinValue(MinVolumePercent);
		SFXVolumeSlider->SetMaxValue(MaxVolumePercent);
		SFXVolumeSlider->OnValueChanged.AddDynamic(this, &UPGSettingsScreenWidget::HandleSFXVolumeChanged);
		SFXVolumeSlider->OnMouseCaptureEnd.AddDynamic(this, &UPGSettingsScreenWidget::HandleSFXVolumeCommit);
		SFXVolumeSlider->OnControllerCaptureEnd.AddDynamic(this, &UPGSettingsScreenWidget::HandleSFXVolumeCommit);
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
	if (ResolutionScaleSlider)
	{
		ResolutionScaleSlider->OnValueChanged.RemoveDynamic(this, &UPGSettingsScreenWidget::HandleResolutionScaleChanged);
		ResolutionScaleSlider->OnMouseCaptureEnd.RemoveDynamic(this, &UPGSettingsScreenWidget::HandleResolutionScaleCommit);
		ResolutionScaleSlider->OnControllerCaptureEnd.RemoveDynamic(this, &UPGSettingsScreenWidget::HandleResolutionScaleCommit);
	}
	if (ShowFPSCheckBox)
	{
		ShowFPSCheckBox->OnCheckStateChanged.RemoveDynamic(this, &UPGSettingsScreenWidget::HandleShowFPSCheckStateChanged);
	}
	if (MusicVolumeSlider)
	{
		MusicVolumeSlider->OnValueChanged.RemoveDynamic(this, &UPGSettingsScreenWidget::HandleMusicVolumeChanged);
		MusicVolumeSlider->OnMouseCaptureEnd.RemoveDynamic(this, &UPGSettingsScreenWidget::HandleMusicVolumeCommit);
		MusicVolumeSlider->OnControllerCaptureEnd.RemoveDynamic(this, &UPGSettingsScreenWidget::HandleMusicVolumeCommit);
	}
	if (SFXVolumeSlider)
	{
		SFXVolumeSlider->OnValueChanged.RemoveDynamic(this, &UPGSettingsScreenWidget::HandleSFXVolumeChanged);
		SFXVolumeSlider->OnMouseCaptureEnd.RemoveDynamic(this, &UPGSettingsScreenWidget::HandleSFXVolumeCommit);
		SFXVolumeSlider->OnControllerCaptureEnd.RemoveDynamic(this, &UPGSettingsScreenWidget::HandleSFXVolumeCommit);
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

	// RootPanel은 전체 화면 레이어로 고정하고, 실제 움직임은 블러/딤/SettingsPanel만 처리한다.
	// Pause와 Home 양쪽에서 재사용되므로 닫힘 완료 후에만 상위 흐름에 소유권을 돌려준다.
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
	UPGAudioStatics::PlayCommonButtonClick(this);
	if (UPGGameUserSettings* UserSettings = Cast<UPGGameUserSettings>(UGameUserSettings::GetGameUserSettings()))
	{
		// 슬라이더는 드래그 중 즉시 적용하지만, 닫을 때 한 번 더 저장해 터치 캡처 종료 누락에 대비한다.
		UserSettings->SaveSettings();
	}

	bIsOpening = false;
	bIsClosing = true;
	AnimationElapsed = 0.0f;
	SetAllButtonsEnabled(false);
}

void UPGSettingsScreenWidget::HandleLowPresetClicked()
{
	UPGAudioStatics::PlayCommonButtonClick(this);
	ApplyPreset(EPGMobileQualityPreset::Low);
}

void UPGSettingsScreenWidget::HandleBalancedPresetClicked()
{
	UPGAudioStatics::PlayCommonButtonClick(this);
	ApplyPreset(EPGMobileQualityPreset::Balanced);
}

void UPGSettingsScreenWidget::HandleHighPresetClicked()
{
	UPGAudioStatics::PlayCommonButtonClick(this);
	ApplyPreset(EPGMobileQualityPreset::High);
}

void UPGSettingsScreenWidget::HandleResolutionScaleChanged(float Value)
{
	if (bIsUpdatingControls)
	{
		return;
	}

	// 해상도 스케일은 드래그 중 즉시 반영해 사용자가 선명도/성능 변화를 바로 비교할 수 있게 한다.
	// 실제 저장은 Commit 또는 Close에서 처리한다.
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

void UPGSettingsScreenWidget::HandleShowFPSCheckStateChanged(bool bIsChecked)
{
	if (bIsUpdatingControls)
	{
		return;
	}

	UPGAudioStatics::PlayCommonButtonClick(this);
	if (UPGGameUserSettings* UserSettings = Cast<UPGGameUserSettings>(UGameUserSettings::GetGameUserSettings()))
	{
		UserSettings->SetShowFPSDisplay(bIsChecked, true);
	}
}

void UPGSettingsScreenWidget::HandleMusicVolumeChanged(float Value)
{
	if (bIsUpdatingControls)
	{
		return;
	}

	// 볼륨은 설정 화면 안에서 바로 청감 피드백이 필요하므로 저장 전에도 현재 재생 중인 사운드에 적용한다.
	UpdateMusicVolumeText(Value);
	if (UPGGameUserSettings* UserSettings = Cast<UPGGameUserSettings>(UGameUserSettings::GetGameUserSettings()))
	{
		UserSettings->SetMusicVolumePercent(Value, false);
	}
}

void UPGSettingsScreenWidget::HandleMusicVolumeCommit()
{
	if (MusicVolumeSlider)
	{
		if (UPGGameUserSettings* UserSettings = Cast<UPGGameUserSettings>(UGameUserSettings::GetGameUserSettings()))
		{
			UserSettings->SetMusicVolumePercent(MusicVolumeSlider->GetValue(), true);
		}
	}
}

void UPGSettingsScreenWidget::HandleSFXVolumeChanged(float Value)
{
	if (bIsUpdatingControls)
	{
		return;
	}

	UpdateSFXVolumeText(Value);
	if (UPGGameUserSettings* UserSettings = Cast<UPGGameUserSettings>(UGameUserSettings::GetGameUserSettings()))
	{
		UserSettings->SetSFXVolumePercent(Value, false);
	}
}

void UPGSettingsScreenWidget::HandleSFXVolumeCommit()
{
	if (SFXVolumeSlider)
	{
		if (UPGGameUserSettings* UserSettings = Cast<UPGGameUserSettings>(UGameUserSettings::GetGameUserSettings()))
		{
			UserSettings->SetSFXVolumePercent(SFXVolumeSlider->GetValue(), true);
		}
	}
}

void UPGSettingsScreenWidget::RefreshControlsFromSettings()
{
	UPGGameUserSettings* UserSettings = Cast<UPGGameUserSettings>(UGameUserSettings::GetGameUserSettings());
	if (!UserSettings)
	{
		return;
	}

	// SetValue/SetChecked가 다시 OnValueChanged를 호출할 수 있으므로, UI 동기화 중에는 저장/사운드 경로를 막는다.
	bIsUpdatingControls = true;
	if (ResolutionScaleSlider)
	{
		ResolutionScaleSlider->SetValue(UserSettings->GetMobileResolutionScalePercent());
	}
	UpdateResolutionScaleText(UserSettings->GetMobileResolutionScalePercent());
	if (MusicVolumeSlider)
	{
		MusicVolumeSlider->SetValue(UserSettings->GetMusicVolumePercent());
	}
	UpdateMusicVolumeText(UserSettings->GetMusicVolumePercent());
	if (SFXVolumeSlider)
	{
		SFXVolumeSlider->SetValue(UserSettings->GetSFXVolumePercent());
	}
	UpdateSFXVolumeText(UserSettings->GetSFXVolumePercent());
	SetShowFPSCheckState(UserSettings->GetShowFPSDisplay());
	bIsUpdatingControls = false;

	SetPresetSelectionState(UserSettings->GetSelectedMobileQualityPreset());
}

void UPGSettingsScreenWidget::ApplyPreset(EPGMobileQualityPreset Preset)
{
	if (UPGGameUserSettings* UserSettings = Cast<UPGGameUserSettings>(UGameUserSettings::GetGameUserSettings()))
	{
		// 프리셋은 해상도와 스케일러블리티 값을 한 번에 바꾸므로, 적용 후 전체 컨트롤을 다시 읽어 UI 불일치를 막는다.
		UserSettings->ApplyMobileQualityPreset(Preset, true);
		RefreshControlsFromSettings();
	}
}

void UPGSettingsScreenWidget::ApplyAnimationState(float Alpha)
{
	if (RootPanel)
	{
		// 루트는 화면 전체 입력 레이어이므로 이동시키지 않는다.
		// 패널만 움직여야 BackgroundBlur와 딤이 뷰포트 전체를 계속 덮는다.
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
	if (ResolutionScaleSlider)
	{
		ResolutionScaleSlider->SetIsEnabled(bEnabled);
	}
	if (ShowFPSCheckBox)
	{
		ShowFPSCheckBox->SetIsEnabled(bEnabled);
	}
	if (MusicVolumeSlider)
	{
		MusicVolumeSlider->SetIsEnabled(bEnabled);
	}
	if (SFXVolumeSlider)
	{
		SFXVolumeSlider->SetIsEnabled(bEnabled);
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
}

void UPGSettingsScreenWidget::SetShowFPSCheckState(bool bShowFPS) const
{
	if (ShowFPSCheckBox)
	{
		ShowFPSCheckBox->SetIsChecked(bShowFPS);
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

void UPGSettingsScreenWidget::UpdateMusicVolumeText(float Value) const
{
	if (MusicVolumeValueText)
	{
		MusicVolumeValueText->SetText(FText::Format(
			NSLOCTEXT("PGSettingsScreen", "MusicVolumeFormat", "{0}%"),
			FText::AsNumber(FMath::RoundToInt(Value))));
	}
}

void UPGSettingsScreenWidget::UpdateSFXVolumeText(float Value) const
{
	if (SFXVolumeValueText)
	{
		SFXVolumeValueText->SetText(FText::Format(
			NSLOCTEXT("PGSettingsScreen", "SFXVolumeFormat", "{0}%"),
			FText::AsNumber(FMath::RoundToInt(Value))));
	}
}
