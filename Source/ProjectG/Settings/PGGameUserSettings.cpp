// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/PGGameUserSettings.h"

#include "HAL/IConsoleManager.h"
#include "Scalability.h"

namespace
{
constexpr float MinMobileResolutionScale = 50.0f;
constexpr float MaxMobileResolutionScale = 100.0f;
}

UPGGameUserSettings::UPGGameUserSettings()
{
	const UPGGameSettings* ProjectSettings = GetDefault<UPGGameSettings>();
	const EPGMobileQualityPreset DefaultPreset = ProjectSettings ? ProjectSettings->MobileQuality.DefaultPreset : EPGMobileQualityPreset::Ultra;
	CopyPresetToSavedSettings(DefaultPreset);
}

void UPGGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();

	const UPGGameSettings* ProjectSettings = GetDefault<UPGGameSettings>();
	const EPGMobileQualityPreset DefaultPreset = ProjectSettings ? ProjectSettings->MobileQuality.DefaultPreset : EPGMobileQualityPreset::Ultra;
	CopyPresetToSavedSettings(DefaultPreset);
}

void UPGGameUserSettings::ApplySavedMobileQualitySettings(bool bSaveAfterApply)
{
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
	SavedMobileFrameRateLimit = FMath::Max(0, NewFrameRateLimit);
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

void UPGGameUserSettings::CopyPresetToSavedSettings(EPGMobileQualityPreset Preset)
{
	const FPGMobileQualityPresetSettings& PresetSettings = GetPresetSettings(Preset);

	SelectedMobileQualityPreset = Preset;
	ResolutionScalePercent = FMath::Clamp(PresetSettings.ResolutionScalePercent, MinMobileResolutionScale, MaxMobileResolutionScale);
	SavedMobileFrameRateLimit = FMath::Max(0, PresetSettings.FrameRateLimit);
	MobileAntiAliasingMode = FMath::Clamp(PresetSettings.MobileAntiAliasingMode, 0, 3);
	AntiAliasingQuality = ClampScalabilityQuality(PresetSettings.AntiAliasingQuality);
	TextureQuality = ClampScalabilityQuality(PresetSettings.TextureQuality);
	ShadowQuality = ClampScalabilityQuality(PresetSettings.ShadowQuality);
	PostProcessQuality = ClampScalabilityQuality(PresetSettings.PostProcessQuality);
	EffectsQuality = ClampScalabilityQuality(PresetSettings.EffectsQuality);
	ViewDistanceQuality = ClampScalabilityQuality(PresetSettings.ViewDistanceQuality);
	MaxAnisotropy = FMath::Clamp(PresetSettings.MaxAnisotropy, 1, 8);
}

void UPGGameUserSettings::ApplySavedMobileQualityToEngine()
{
	ResolutionScalePercent = FMath::Clamp(ResolutionScalePercent, MinMobileResolutionScale, MaxMobileResolutionScale);
	SavedMobileFrameRateLimit = FMath::Max(0, SavedMobileFrameRateLimit);
	MobileAntiAliasingMode = FMath::Clamp(MobileAntiAliasingMode, 0, 3);
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

	SetConsoleVariableInt(TEXT("r.Mobile.AntiAliasing"), MobileAntiAliasingMode);
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
		return ProjectSettings->MobileQuality.Ultra;
	case EPGMobileQualityPreset::Balanced:
	default:
		return ProjectSettings->MobileQuality.Balanced;
	}
}

void UPGGameUserSettings::SetConsoleVariableInt(const TCHAR* Name, int32 Value)
{
	if (IConsoleVariable* ConsoleVariable = IConsoleManager::Get().FindConsoleVariable(Name))
	{
		ConsoleVariable->Set(Value, ECVF_SetByGameSetting);
	}
}

int32 UPGGameUserSettings::ClampScalabilityQuality(int32 Value)
{
	return FMath::Clamp(Value, 0, 4);
}
