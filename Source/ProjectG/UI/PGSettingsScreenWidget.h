// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Settings/PGGameSettings.h"
#include "PGSettingsScreenWidget.generated.h"

class UButton;
class UBackgroundBlur;
class UCheckBox;
class USlider;
class UTextBlock;
class UWidget;

DECLARE_MULTICAST_DELEGATE(FPGSettingsScreenClosedSignature);

/**
 * Home과 Pause에서 공용으로 사용하는 Settings 화면 로직 위젯입니다.
 * BP는 패널 배치와 스타일을 담당하고, C++은 품질/볼륨/FPS 설정 반영과 열림/닫힘 애니메이션만 처리합니다.
 */
UCLASS()
class PROJECTG_API UPGSettingsScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	FPGSettingsScreenClosedSignature OnSettingsScreenClosed;

	// 위젯을 화면에 추가한 직후 배경 블러/딤과 패널 진입 애니메이션을 시작합니다.
	void PlayOpenAnimation();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// Settings 화면 전체 루트입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<UWidget> RootPanel;

	// 뒤 게임 화면을 약하게 흐리게 만드는 전체 화면 블러입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<UBackgroundBlur> BackgroundBlur;

	// 블러 위에 얹는 반투명 딤 레이어입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<UWidget> BackdropDim;

	// 실제 설정 항목들을 담는 중앙 패널입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<UWidget> SettingsPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<UButton> LowPresetButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<UButton> BalancedPresetButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<UButton> HighPresetButton;

	// 내부 렌더 해상도 비율을 조절하는 슬라이더입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<USlider> ResolutionScaleSlider;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<UTextBlock> ResolutionScaleValueText;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<UButton> FrameRate30Button;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<UButton> FrameRate60Button;

	// Android 실기기 성능 확인용 FPS 오버레이 표시 토글입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<UCheckBox> ShowFPSCheckBox;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<USlider> MusicVolumeSlider;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<UTextBlock> MusicVolumeValueText;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<USlider> SFXVolumeSlider;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<UTextBlock> SFXVolumeValueText;

private:
	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleLowPresetClicked();

	UFUNCTION()
	void HandleBalancedPresetClicked();

	UFUNCTION()
	void HandleHighPresetClicked();

	UFUNCTION()
	void HandleResolutionScaleChanged(float Value);

	UFUNCTION()
	void HandleResolutionScaleCommit();

	UFUNCTION()
	void HandleFrameRate30Clicked();

	UFUNCTION()
	void HandleFrameRate60Clicked();

	UFUNCTION()
	void HandleShowFPSCheckStateChanged(bool bIsChecked);

	UFUNCTION()
	void HandleMusicVolumeChanged(float Value);

	UFUNCTION()
	void HandleMusicVolumeCommit();

	UFUNCTION()
	void HandleSFXVolumeChanged(float Value);

	UFUNCTION()
	void HandleSFXVolumeCommit();

	void RefreshControlsFromSettings();
	void ApplyPreset(EPGMobileQualityPreset Preset);
	void ApplyFrameRate(int32 FrameRateLimit);
	void ApplyAnimationState(float Alpha);
	void SetAllButtonsEnabled(bool bEnabled) const;
	void SetPresetSelectionState(EPGMobileQualityPreset Preset) const;
	void SetFrameRateSelectionState(int32 FrameRateLimit) const;
	void SetShowFPSCheckState(bool bShowFPS) const;
	void UpdateResolutionScaleText(float Value) const;
	void UpdateMusicVolumeText(float Value) const;
	void UpdateSFXVolumeText(float Value) const;

	bool bIsOpening = false;
	bool bIsClosing = false;
	bool bIsUpdatingControls = false;
	float AnimationElapsed = 0.0f;
	float AnimationDuration = 0.18f;
};
