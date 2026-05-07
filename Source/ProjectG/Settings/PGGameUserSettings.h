// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "Settings/PGGameSettings.h"
#include "PGGameUserSettings.generated.h"

/**
 * 모바일 품질 프리셋과 런타임 그래픽 설정을 저장/적용하는 프로젝트 GameUserSettings입니다.
 * 기기 기본값은 DeviceProfile이 담당하고, 사용자가 Home Settings에서 고른 값은 이 클래스가 유지합니다.
 */
UCLASS(Config = GameUserSettings)
class PROJECTG_API UPGGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	UPGGameUserSettings();

	virtual void SetToDefaults() override;

	// 저장된 모바일 품질 값을 현재 런타임에 적용합니다.
	UFUNCTION(BlueprintCallable, Category = "Mobile Quality")
	void ApplySavedMobileQualitySettings(bool bSaveAfterApply);

	// 지정한 프리셋 값을 저장값으로 복사한 뒤 런타임에 적용합니다.
	UFUNCTION(BlueprintCallable, Category = "Mobile Quality")
	void ApplyMobileQualityPreset(EPGMobileQualityPreset Preset, bool bSaveAfterApply = true);

	// 현재 선택된 프리셋은 유지하고 내부 렌더 해상도만 조정합니다.
	UFUNCTION(BlueprintCallable, Category = "Mobile Quality")
	void SetMobileResolutionScalePercent(float NewResolutionScalePercent, bool bSaveAfterApply = true);

	// 현재 선택된 프리셋은 유지하고 프레임 제한만 조정합니다.
	UFUNCTION(BlueprintCallable, Category = "Mobile Quality")
	void SetMobileFrameRateLimit(int32 NewFrameRateLimit, bool bSaveAfterApply = true);

	UFUNCTION(BlueprintPure, Category = "Mobile Quality")
	EPGMobileQualityPreset GetSelectedMobileQualityPreset() const { return SelectedMobileQualityPreset; }

	UFUNCTION(BlueprintPure, Category = "Mobile Quality")
	float GetMobileResolutionScalePercent() const { return ResolutionScalePercent; }

	UFUNCTION(BlueprintPure, Category = "Mobile Quality")
	int32 GetMobileFrameRateLimit() const { return SavedMobileFrameRateLimit; }

private:
	void CopyPresetToSavedSettings(EPGMobileQualityPreset Preset);
	void ApplySavedMobileQualityToEngine();
	const FPGMobileQualityPresetSettings& GetPresetSettings(EPGMobileQualityPreset Preset) const;
	static void SetConsoleVariableInt(const TCHAR* Name, int32 Value);
	static int32 ClampScalabilityQuality(int32 Value);

	// 사용자가 마지막으로 선택한 모바일 품질 프리셋
	UPROPERTY(Config)
	EPGMobileQualityPreset SelectedMobileQualityPreset = EPGMobileQualityPreset::Ultra;

	// 내부 렌더 해상도 비율
	UPROPERTY(Config)
	float ResolutionScalePercent = 100.0f;

	// 모바일 프레임 제한
	UPROPERTY(Config)
	int32 SavedMobileFrameRateLimit = 60;

	// 모바일 안티앨리어싱 방식 CVar 값
	UPROPERTY(Config)
	int32 MobileAntiAliasingMode = 2;

	UPROPERTY(Config)
	int32 AntiAliasingQuality = 4;

	UPROPERTY(Config)
	int32 TextureQuality = 4;

	UPROPERTY(Config)
	int32 ShadowQuality = 4;

	UPROPERTY(Config)
	int32 PostProcessQuality = 4;

	UPROPERTY(Config)
	int32 EffectsQuality = 4;

	UPROPERTY(Config)
	int32 ViewDistanceQuality = 4;

	UPROPERTY(Config)
	int32 MaxAnisotropy = 8;
};
