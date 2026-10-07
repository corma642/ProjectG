// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/PGGameUserSettings.h"

#include "HAL/IConsoleManager.h"
#include "Scalability.h"

namespace
{
constexpr float MinMobileResolutionScale = 50.0f;
constexpr float MaxMobileResolutionScale = 100.0f;
constexpr float FixedMobileFrameRateLimit = 60.0f;
constexpr float MinUserVolumePercent = 0.0f;
constexpr float MaxUserVolumePercent = 150.0f;
}

UPGGameUserSettings::UPGGameUserSettings()
{
	const UPGGameSettings* ProjectSettings = GetDefault<UPGGameSettings>();
	const EPGMobileQualityPreset DefaultPreset = ProjectSettings ? ProjectSettings->MobileQuality.DefaultPreset : EPGMobileQualityPreset::High;
	CopyPresetToSavedSettings(DefaultPreset);
	SetFrameRateLimit(FixedMobileFrameRateLimit);
	MusicVolumePercent = 100.0f;
	SFXVolumePercent = 100.0f;
}

void UPGGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();

	const UPGGameSettings* ProjectSettings = GetDefault<UPGGameSettings>();
	const EPGMobileQualityPreset DefaultPreset = ProjectSettings ? ProjectSettings->MobileQuality.DefaultPreset : EPGMobileQualityPreset::High;
	CopyPresetToSavedSettings(DefaultPreset);
	SetFrameRateLimit(FixedMobileFrameRateLimit);
	bHasUserSelectedMobileQuality = false;
}

void UPGGameUserSettings::ApplySavedMobileQualitySettings(bool bSaveAfterApply)
{
	NormalizeSavedMobileQualitySettings();
	if (bHasUserSelectedMobileQuality)
	{
		ApplySavedMobileQualityToEngine();
	}
	else
	{
		// 첫 실행에는 기기 프로필이 고른 Scalability를 덮지 않습니다.
		// 프레임 제한은 그래픽 품질과 분리하여 모든 기기에서 60FPS 상한을 유지합니다.
		SetFrameRateLimit(FixedMobileFrameRateLimit);
		SetFrameRateLimitCVar(GetEffectiveFrameRateLimit());
	}
	if (bSaveAfterApply)
	{
		SaveSettings();
	}
}

void UPGGameUserSettings::ApplyMobileQualityPreset(EPGMobileQualityPreset Preset, bool bSaveAfterApply)
{
	bHasUserSelectedMobileQuality = true;
	CopyPresetToSavedSettings(Preset);
	ApplySavedMobileQualitySettings(bSaveAfterApply);
}

void UPGGameUserSettings::SetMobileResolutionScalePercent(float NewResolutionScalePercent, bool bSaveAfterApply)
{
	bHasUserSelectedMobileQuality = true;
	ResolutionScalePercent = FMath::Clamp(NewResolutionScalePercent, MinMobileResolutionScale, MaxMobileResolutionScale);
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
	const bool bValueChanged = !FMath::IsNearlyEqual(MusicVolumePercent, ClampedVolume);
	if (bValueChanged)
	{
		MusicVolumePercent = ClampedVolume;
	}

	SavePendingVolumeChange(bValueChanged, bSaveAfterApply);

	if (bValueChanged)
	{
		OnMusicVolumeChanged.Broadcast(GetMusicVolumeScale());
	}
}

void UPGGameUserSettings::SetSFXVolumePercent(float NewSFXVolumePercent, bool bSaveAfterApply)
{
	const float ClampedVolume = FMath::Clamp(NewSFXVolumePercent, MinUserVolumePercent, MaxUserVolumePercent);
	const bool bValueChanged = !FMath::IsNearlyEqual(SFXVolumePercent, ClampedVolume);
	if (bValueChanged)
	{
		SFXVolumePercent = ClampedVolume;
	}

	SavePendingVolumeChange(bValueChanged, bSaveAfterApply);

	if (bValueChanged)
	{
		OnSFXVolumeChanged.Broadcast(GetSFXVolumeScale());
	}
}

void UPGGameUserSettings::SavePendingVolumeChange(bool bValueChanged, bool bSaveAfterApply)
{
	// 슬라이더는 드래그 중 저장 없이 값을 먼저 반영하고, 손을 뗄 때 같은 값으로 저장을 요청합니다.
	// 값 비교만으로는 그 저장이 누락되므로 저장하지 않은 변경이 남았는지를 따로 기억합니다.
	bHasUnsavedVolumeChange |= bValueChanged;
	if (bSaveAfterApply && bHasUnsavedVolumeChange)
	{
		SaveSettings();
		bHasUnsavedVolumeChange = false;
	}
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
	MaxAnisotropy = FMath::Clamp(MaxAnisotropy, 1, 8);

	SetResolutionScaleValueEx(ResolutionScalePercent);
	SetFrameRateLimit(FixedMobileFrameRateLimit);

	ScalabilityQuality = Scalability::GetQualityLevels();
	ScalabilityQuality.ResolutionQuality = ResolutionScalePercent;
	ScalabilityQuality.SetAntiAliasingQuality(ClampScalabilityQuality(AntiAliasingQuality));
	ScalabilityQuality.SetTextureQuality(ClampScalabilityQuality(TextureQuality));
	ScalabilityQuality.SetShadowQuality(ClampScalabilityQuality(ShadowQuality));
	ScalabilityQuality.SetPostProcessQuality(ClampScalabilityQuality(PostProcessQuality));
	ScalabilityQuality.SetEffectsQuality(ClampScalabilityQuality(EffectsQuality));
	ScalabilityQuality.SetViewDistanceQuality(ClampScalabilityQuality(ViewDistanceQuality));
	// 해상도 슬라이더는 드래그마다 이 경로를 타므로, 값이 바뀐 그룹만 적용해 텍스처·그림자 재설정을 피한다.
	Scalability::SetQualityLevels(ScalabilityQuality);

	SetConsoleVariableInt(TEXT("r.MaxAnisotropy"), MaxAnisotropy);

	// 엔진 ApplySettings()는 매번 설정을 디스크에 저장하므로 쓰지 않는다. 저장은 호출자의 bSaveAfterApply가 결정한다.
	SetFrameRateLimitCVar(GetEffectiveFrameRateLimit());
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
