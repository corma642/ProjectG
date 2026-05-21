// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "Settings/PGGameSettings.h"
#include "PGGameUserSettings.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FPGShowFPSDisplayChangedSignature, bool);
DECLARE_MULTICAST_DELEGATE_OneParam(FPGMusicVolumeChangedSignature, float);
DECLARE_MULTICAST_DELEGATE_OneParam(FPGSFXVolumeChangedSignature, float);

/**
 * 모바일 품질 프리셋, FPS 표시, 오디오 볼륨처럼 사용자별로 저장되는 런타임 설정입니다.
 * 프로젝트 기본값은 UPGGameSettings/DeviceProfile이 담당하고, 사용자가 Settings UI에서 고른 값만 저장합니다.
 */
UCLASS(Config = GameUserSettings)
class PROJECTG_API UPGGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	UPGGameUserSettings();

	FPGShowFPSDisplayChangedSignature OnShowFPSDisplayChanged;
	FPGMusicVolumeChangedSignature OnMusicVolumeChanged;
	FPGSFXVolumeChangedSignature OnSFXVolumeChanged;

	virtual void SetToDefaults() override;

	// 저장된 모바일 품질 값을 현재 렌더링 환경에 적용합니다.
	UFUNCTION(BlueprintCallable, Category = "Quality")
	void ApplySavedMobileQualitySettings(bool bSaveAfterApply);

	// 지정한 품질 프리셋 값을 저장값으로 복사한 뒤 렌더링 환경에 적용합니다.
	UFUNCTION(BlueprintCallable, Category = "Quality")
	void ApplyMobileQualityPreset(EPGMobileQualityPreset Preset, bool bSaveAfterApply = true);

	// 현재 선택된 품질 프리셋은 유지하고 내부 렌더 해상도만 조정합니다.
	UFUNCTION(BlueprintCallable, Category = "Quality")
	void SetMobileResolutionScalePercent(float NewResolutionScalePercent, bool bSaveAfterApply = true);

	// 현재 선택된 품질 프리셋은 유지하고 프레임 제한만 조정합니다.
	UFUNCTION(BlueprintCallable, Category = "Quality")
	void SetMobileFrameRateLimit(int32 NewFrameRateLimit, bool bSaveAfterApply = true);

	// Android 실기기 프레임 확인용 FPS 표시 여부를 저장하고 즉시 반영합니다.
	UFUNCTION(BlueprintCallable, Category = "UI")
	void SetShowFPSDisplay(bool bShow, bool bSaveAfterApply = true);

	// 배경음 볼륨을 0~150% 범위로 저장하고 즉시 반영합니다.
	UFUNCTION(BlueprintCallable, Category = "Audio")
	void SetMusicVolumePercent(float NewMusicVolumePercent, bool bSaveAfterApply = true);

	// 효과음 볼륨을 0~150% 범위로 저장하고 즉시 반영합니다.
	UFUNCTION(BlueprintCallable, Category = "Audio")
	void SetSFXVolumePercent(float NewSFXVolumePercent, bool bSaveAfterApply = true);

	UFUNCTION(BlueprintPure, Category = "Quality")
	EPGMobileQualityPreset GetSelectedMobileQualityPreset() const { return SelectedMobileQualityPreset; }

	UFUNCTION(BlueprintPure, Category = "Quality")
	float GetMobileResolutionScalePercent() const { return ResolutionScalePercent; }

	UFUNCTION(BlueprintPure, Category = "Quality")
	int32 GetMobileFrameRateLimit() const { return SavedMobileFrameRateLimit; }

	UFUNCTION(BlueprintPure, Category = "UI")
	bool GetShowFPSDisplay() const { return bShowFPSDisplay; }

	UFUNCTION(BlueprintPure, Category = "Audio")
	float GetMusicVolumePercent() const { return MusicVolumePercent; }

	UFUNCTION(BlueprintPure, Category = "Audio")
	float GetSFXVolumePercent() const { return SFXVolumePercent; }

	UFUNCTION(BlueprintPure, Category = "Audio")
	float GetMusicVolumeScale() const { return MusicVolumePercent / 100.0f; }

	UFUNCTION(BlueprintPure, Category = "Audio")
	float GetSFXVolumeScale() const { return SFXVolumePercent / 100.0f; }

	static float GetCurrentMusicVolumeScale();
	static float GetCurrentSFXVolumeScale();

private:
	void CopyPresetToSavedSettings(EPGMobileQualityPreset Preset);
	void NormalizeSavedMobileQualitySettings();
	void ApplySavedMobileQualityToEngine();
	const FPGMobileQualityPresetSettings& GetPresetSettings(EPGMobileQualityPreset Preset) const;
	static void SetConsoleVariableInt(const TCHAR* Name, int32 Value);
	static int32 ClampScalabilityQuality(int32 Value);

	// 사용자가 마지막으로 선택한 모바일 품질 프리셋입니다.
	UPROPERTY(Config)
	EPGMobileQualityPreset SelectedMobileQualityPreset = EPGMobileQualityPreset::High;

	// 내부 렌더 해상도 비율입니다.
	UPROPERTY(Config)
	float ResolutionScalePercent = 100.0f;

	// 모바일 프레임 제한입니다.
	UPROPERTY(Config)
	int32 SavedMobileFrameRateLimit = 60;

	// 모바일 안티앨리어싱 방식 CVar 값입니다.
	UPROPERTY(Config)
	int32 MobileAntiAliasingMode = 2;

	UPROPERTY(Config)
	int32 AntiAliasingQuality = 3;

	UPROPERTY(Config)
	int32 TextureQuality = 3;

	UPROPERTY(Config)
	int32 ShadowQuality = 3;

	UPROPERTY(Config)
	int32 PostProcessQuality = 3;

	UPROPERTY(Config)
	int32 EffectsQuality = 3;

	UPROPERTY(Config)
	int32 ViewDistanceQuality = 3;

	UPROPERTY(Config)
	int32 MaxAnisotropy = 8;

	// Android 프레임 테스트용 FPS 오버레이 표시 여부입니다.
	UPROPERTY(Config)
	bool bShowFPSDisplay = false;

	// 사용자 배경음 볼륨입니다. 100은 기본 볼륨이며 Settings UI에서 0~150까지 조절합니다.
	UPROPERTY(Config)
	float MusicVolumePercent = 100.0f;

	// 사용자 효과음 볼륨입니다. 100은 기본 볼륨이며 Settings UI에서 0~150까지 조절합니다.
	UPROPERTY(Config)
	float SFXVolumePercent = 100.0f;
};
