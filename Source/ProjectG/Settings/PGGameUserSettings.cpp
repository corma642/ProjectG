// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/PGGameUserSettings.h"

#include "HAL/IConsoleManager.h"
#include "Scalability.h"

namespace
{
constexpr float MinMobileResolutionScale = 50.0f;
constexpr float MaxMobileResolutionScale = 100.0f;
constexpr int32 MaxMobileFrameRateLimit = 60;
constexpr float MinUserVolumePercent = 0.0f;
constexpr float MaxUserVolumePercent = 150.0f;
}

UPGGameUserSettings::UPGGameUserSettings()
{
	const UPGGameSettings* ProjectSettings = GetDefault<UPGGameSettings>();
	const EPGMobileQualityPreset DefaultPreset = ProjectSettings ? ProjectSettings->MobileQuality.DefaultPreset : EPGMobileQualityPreset::High;
	CopyPresetToSavedSettings(DefaultPreset);
	MusicVolumePercent = 100.0f;
	SFXVolumePercent = 100.0f;
}

void UPGGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();

	const UPGGameSettings* ProjectSettings = GetDefault<UPGGameSettings>();
	const EPGMobileQualityPreset DefaultPreset = ProjectSettings ? ProjectSettings->MobileQuality.DefaultPreset : EPGMobileQualityPreset::High;
	CopyPresetToSavedSettings(DefaultPreset);
}

void UPGGameUserSettings::ApplySavedMobileQualitySettings(bool bSaveAfterApply)
{
	NormalizeSavedMobileQualitySettings();
	ApplySavedMobileQualityToEngine();
	if (bSaveAfterApply)
	{
		SaveSettings();
	}
}

void UPGGameUserSettings::ApplyMobileQualityPreset(EPGMobileQualityPreset Preset, bool bSaveAfterApply)
{
	CopyPresetToSavedSettings(Preset);
	ApplySavedMobileQualitySettings(bSaveAfterApply);
}

void UPGGameUserSettings::SetMobileResolutionScalePercent(float NewResolutionScalePercent, bool bSaveAfterApply)
{
	ResolutionScalePercent = FMath::Clamp(NewResolutionScalePercent, MinMobileResolutionScale, MaxMobileResolutionScale);
	ApplySavedMobileQualitySettings(bSaveAfterApply);
}

void UPGGameUserSettings::SetMobileFrameRateLimit(int32 NewFrameRateLimit, bool bSaveAfterApply)
{
	SavedMobileFrameRateLimit = FMath::Clamp(NewFrameRateLimit, 0, MaxMobileFrameRateLimit);
	ApplySavedMobileQualitySettings(bSaveAfterApply);
}

void UPGGameUserSettings::SetShowFPSDisplay(bool bShow, bool bSaveAfterApply)
{
	if (bShowFPSDisplay == bShow)
	{
		return;
	}

	bShowFPSDisplay = bShow;
	if (bSaveAfterApply)
	{
		SaveSettings();
	}

	OnShowFPSDisplayChanged.Broadcast(bShowFPSDisplay);
}

void UPGGameUserSettings::SetMusicVolumePercent(float NewMusicVolumePercent, bool bSaveAfterApply)
{
	const float ClampedVolume = FMath::Clamp(NewMusicVolumePercent, MinUserVolumePercent, MaxUserVolumePercent);
	if (FMath::IsNearlyEqual(MusicVolumePercent, ClampedVolume))
	{
		return;
	}

	MusicVolumePercent = ClampedVolume;
	if (bSaveAfterApply)
	{
		SaveSettings();
	}

	OnMusicVolumeChanged.Broadcast(GetMusicVolumeScale());
}

void UPGGameUserSettings::SetSFXVolumePercent(float NewSFXVolumePercent, bool bSaveAfterApply)
{
	const float ClampedVolume = FMath::Clamp(NewSFXVolumePercent, MinUserVolumePercent, MaxUserVolumePercent);
	if (FMath::IsNearlyEqual(SFXVolumePercent, ClampedVolume))
	{
		return;
	}

	SFXVolumePercent = ClampedVolume;
	if (bSaveAfterApply)
	{
		SaveSettings();
	}

	OnSFXVolumeChanged.Broadcast(GetSFXVolumeScale());
}

float UPGGameUserSettings::GetCurrentMusicVolumeScale()
{
	const UPGGameUserSettings* UserSettings = Cast<UPGGameUserSettings>(UGameUserSettings::GetGameUserSettings());
	return UserSettings ? UserSettings->GetMusicVolumeScale() : 1.0f;
}

float UPGGameUserSettings::GetCurrentSFXVolumeScale()
{
	const UPGGameUserSettings* UserSettings = Cast<UPGGameUserSettings>(UGameUserSettings::GetGameUserSettings());
	return UserSettings ? UserSettings->GetSFXVolumeScale() : 1.0f;
}

void UPGGameUserSettings::CopyPresetToSavedSettings(EPGMobileQualityPreset Preset)
{
	if (Preset == EPGMobileQualityPreset::Ultra)
	{
		Preset = EPGMobileQualityPreset::High;
	}

	const FPGMobileQualityPresetSettings& PresetSettings = GetPresetSettings(Preset);

	SelectedMobileQualityPreset = Preset;
	ResolutionScalePercent = FMath::Clamp(PresetSettings.ResolutionScalePercent, MinMobileResolutionScale, MaxMobileResolutionScale);
	SavedMobileFrameRateLimit = FMath::Clamp(PresetSettings.FrameRateLimit, 0, MaxMobileFrameRateLimit);
	AntiAliasingQuality = ClampScalabilityQuality(PresetSettings.AntiAliasingQuality);
	TextureQuality = ClampScalabilityQuality(PresetSettings.TextureQuality);
	ShadowQuality = ClampScalabilityQuality(PresetSettings.ShadowQuality);
	PostProcessQuality = ClampScalabilityQuality(PresetSettings.PostProcessQuality);
	EffectsQuality = ClampScalabilityQuality(PresetSettings.EffectsQuality);
	ViewDistanceQuality = ClampScalabilityQuality(PresetSettings.ViewDistanceQuality);
	MaxAnisotropy = FMath::Clamp(PresetSettings.MaxAnisotropy, 1, 8);
}

void UPGGameUserSettings::NormalizeSavedMobileQualitySettings()
{
	if (SelectedMobileQualityPreset == EPGMobileQualityPreset::Ultra)
	{
		SelectedMobileQualityPreset = EPGMobileQualityPreset::High;
	}

	SavedMobileFrameRateLimit = FMath::Clamp(SavedMobileFrameRateLimit, 0, MaxMobileFrameRateLimit);
	AntiAliasingQuality = ClampScalabilityQuality(AntiAliasingQuality);
	TextureQuality = ClampScalabilityQuality(TextureQuality);
	ShadowQuality = ClampScalabilityQuality(ShadowQuality);
	PostProcessQuality = ClampScalabilityQuality(PostProcessQuality);
	EffectsQuality = ClampScalabilityQuality(EffectsQuality);
	ViewDistanceQuality = ClampScalabilityQuality(ViewDistanceQuality);
}

void UPGGameUserSettings::ApplySavedMobileQualityToEngine()
{
	NormalizeSavedMobileQualitySettings();
	ResolutionScalePercent = FMath::Clamp(ResolutionScalePercent, MinMobileResolutionScale, MaxMobileResolutionScale);
	MusicVolumePercent = FMath::Clamp(MusicVolumePercent, MinUserVolumePercent, MaxUserVolumePercent);
	SFXVolumePercent = FMath::Clamp(SFXVolumePercent, MinUserVolumePercent, MaxUserVolumePercent);
	SavedMobileFrameRateLimit = FMath::Clamp(SavedMobileFrameRateLimit, 0, MaxMobileFrameRateLimit);
	MaxAnisotropy = FMath::Clamp(MaxAnisotropy, 1, 8);

	SetResolutionScaleValueEx(ResolutionScalePercent);
	SetFrameRateLimit(static_cast<float>(SavedMobileFrameRateLimit));

	ScalabilityQuality = Scalability::GetQualityLevels();
	ScalabilityQuality.ResolutionQuality = ResolutionScalePercent;
	ScalabilityQuality.SetAntiAliasingQuality(ClampScalabilityQuality(AntiAliasingQuality));
	ScalabilityQuality.SetTextureQuality(ClampScalabilityQuality(TextureQuality));
	ScalabilityQuality.SetShadowQuality(ClampScalabilityQuality(ShadowQuality));
	ScalabilityQuality.SetPostProcessQuality(ClampScalabilityQuality(PostProcessQuality));
	ScalabilityQuality.SetEffectsQuality(ClampScalabilityQuality(EffectsQuality));
	ScalabilityQuality.SetViewDistanceQuality(ClampScalabilityQuality(ViewDistanceQuality));
	Scalability::SetQualityLevels(ScalabilityQuality, true);

	SetConsoleVariableInt(TEXT("r.MaxAnisotropy"), MaxAnisotropy);

	ApplySettings(false);
}

const FPGMobileQualityPresetSettings& UPGGameUserSettings::GetPresetSettings(EPGMobileQualityPreset Preset) const
{
	const UPGGameSettings* ProjectSettings = GetDefault<UPGGameSettings>();
	if (!ProjectSettings)
	{
		static const FPGMobileQualityPresetSettings FallbackSettings;
		return FallbackSettings;
	}

	switch (Preset)
	{
	case EPGMobileQualityPreset::Low:
		return ProjectSettings->MobileQuality.Low;
	case EPGMobileQualityPreset::High:
		return ProjectSettings->MobileQuality.High;
	case EPGMobileQualityPreset::Ultra:
		return ProjectSettings->MobileQuality.High;
	case EPGMobileQualityPreset::Balanced:
	default:
		return ProjectSettings->MobileQuality.Balanced;
	}
}

void UPGGameUserSettings::SetConsoleVariableInt(const TCHAR* Name, int32 Value)
{
	if (IConsoleVariable* ConsoleVariable = IConsoleManager::Get().FindConsoleVariable(Name))
	{
		ConsoleVariable->Set(Value, ECVF_SetByScalability);
	}
}

int32 UPGGameUserSettings::ClampScalabilityQuality(int32 Value)
{
	return FMath::Clamp(Value, 0, 3);
}
