// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "Settings/PGGameSettings.h"
#include "PGGameUserSettings.generated.h"

/** FPS 오버레이 설정이 바뀌어 표시 컴포넌트가 위젯 수명을 갱신할 때 발생합니다. */
DECLARE_MULTICAST_DELEGATE_OneParam(FPGShowFPSDisplayChangedSignature, bool);
/** 0~1 음악 볼륨이 바뀌어 활성 음악 컴포넌트가 믹스를 갱신할 때 발생합니다. */
DECLARE_MULTICAST_DELEGATE_OneParam(FPGMusicVolumeChangedSignature, float);
/** 0~1 효과음 볼륨이 바뀌어 이후 UI와 게임 SFX에 반영될 때 발생합니다. */
DECLARE_MULTICAST_DELEGATE_OneParam(FPGSFXVolumeChangedSignature, float);

/**
 * 모바일 품질 프리셋, FPS 표시, 오디오 볼륨처럼 사용자별로 저장되는 런타임 설정입니다.
 * 프로젝트 기본값은 UPGGameSettings/DeviceProfile이 담당하고, 사용자가 Settings UI에서 고른 값만 저장합니다.
 * 프레임 제한은 그래픽 품질과 분리하여 항상 60FPS로 유지합니다.
 */
UCLASS(Config = GameUserSettings)
class PROJECTG_API UPGGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	/** 프로젝트 모바일 기본값과 사용자 선택 플래그의 초기 저장 상태를 구성합니다. */
	UPGGameUserSettings();

	/** FPS 표시 설정 변경을 런타임 오버레이 소유 컴포넌트에 전달합니다. */
	FPGShowFPSDisplayChangedSignature OnShowFPSDisplayChanged;
	/** 0~1 음악 배율 변경을 활성 음악 컴포넌트에 전달합니다. */
	FPGMusicVolumeChangedSignature OnMusicVolumeChanged;
	/** 0~1 효과음 배율 변경을 이후 SFX 재생 경로에 전달합니다. */
	FPGSFXVolumeChangedSignature OnSFXVolumeChanged;

	/** 엔진 기본 reset 이후 Project Settings의 모바일 기본값을 사용자 설정에 복사합니다. */
	virtual void SetToDefaults() override;

	/** 사용자가 품질을 선택한 경우에만 저장값을 현재 렌더링 환경에 적용하고 선택적으로 저장합니다. */
	UFUNCTION(BlueprintCallable, Category = "Quality")
	void ApplySavedMobileQualitySettings(bool bSaveAfterApply);

	/** 지정 프리셋 값을 저장 상태로 복사한 뒤 렌더링 CVar에 적용하고 선택적으로 저장합니다. */
	UFUNCTION(BlueprintCallable, Category = "Quality")
	void ApplyMobileQualityPreset(EPGMobileQualityPreset Preset, bool bSaveAfterApply = true);

	/** 선택 프리셋은 유지하고 설정 clamp 범위의 내부 렌더 해상도 백분율만 적용합니다. */
	UFUNCTION(BlueprintCallable, Category = "Quality")
	void SetMobileResolutionScalePercent(float NewResolutionScalePercent, bool bSaveAfterApply = true);

	/** Android 실기기 FPS 오버레이 표시 여부를 적용하고 변경 delegate를 발생시킨 뒤 선택적으로 저장합니다. */
	UFUNCTION(BlueprintCallable, Category = "UI")
	void SetShowFPSDisplay(bool bShow, bool bSaveAfterApply = true);

	/** 배경음 볼륨을 0~150%로 clamp해 적용하고 변경 delegate를 발생시킨 뒤 선택적으로 저장합니다. */
	UFUNCTION(BlueprintCallable, Category = "Audio")
	void SetMusicVolumePercent(float NewMusicVolumePercent, bool bSaveAfterApply = true);

	/** 효과음 볼륨을 0~150%로 clamp해 적용하고 변경 delegate를 발생시킨 뒤 선택적으로 저장합니다. */
	UFUNCTION(BlueprintCallable, Category = "Audio")
	void SetSFXVolumePercent(float NewSFXVolumePercent, bool bSaveAfterApply = true);

	/** 사용자가 마지막으로 선택한 모바일 품질 프리셋을 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "Quality")
	EPGMobileQualityPreset GetSelectedMobileQualityPreset() const { return SelectedMobileQualityPreset; }

	/** 현재 내부 렌더 해상도 비율을 백분율로 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "Quality")
	float GetMobileResolutionScalePercent() const { return ResolutionScalePercent; }

	/** 사용자가 품질 프리셋 또는 해상도를 직접 선택했는지 반환합니다. false이면 Android DeviceProfile 품질을 유지합니다. */
	UFUNCTION(BlueprintPure, Category = "Quality")
	bool HasUserSelectedMobileQuality() const { return bHasUserSelectedMobileQuality; }

	/** Android 실기기 FPS 오버레이 표시 설정을 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "UI")
	bool GetShowFPSDisplay() const { return bShowFPSDisplay; }

	/** 저장된 음악 볼륨을 0~150 백분율로 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "Audio")
	float GetMusicVolumePercent() const { return MusicVolumePercent; }

	/** 저장된 효과음 볼륨을 0~150 백분율로 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "Audio")
	float GetSFXVolumePercent() const { return SFXVolumePercent; }

	/** 저장된 음악 백분율을 사운드 재생용 0~1.5 배율로 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "Audio")
	float GetMusicVolumeScale() const { return MusicVolumePercent / 100.0f; }

	/** 저장된 효과음 백분율을 사운드 재생용 0~1.5 배율로 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "Audio")
	float GetSFXVolumeScale() const { return SFXVolumePercent / 100.0f; }

	/** 현재 전역 GameUserSettings의 음악 배율을 반환하며 설정 타입이 다르면 1을 반환합니다. */
	static float GetCurrentMusicVolumeScale();
	/** 현재 전역 GameUserSettings의 효과음 배율을 반환하며 설정 타입이 다르면 1을 반환합니다. */
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

	// 모바일 안티앨리어싱 방식 CVar 값입니다.
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

	// 신규 설치에서는 DeviceProfile 품질을 우선합니다. Settings에서 품질을 직접 바꾼 뒤에만 저장 프리셋을 적용합니다.
	UPROPERTY(Config)
	bool bHasUserSelectedMobileQuality = false;

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
