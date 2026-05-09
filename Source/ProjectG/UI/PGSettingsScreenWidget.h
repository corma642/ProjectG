// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Settings/PGGameSettings.h"
#include "PGSettingsScreenWidget.generated.h"

class UButton;
class UBackgroundBlur;
class USlider;
class UTextBlock;
class UWidget;

DECLARE_MULTICAST_DELEGATE(FPGSettingsScreenClosedSignature);

/**
 * 홈 화면 Settings Blueprint Widget의 로직 클래스입니다.
 * BP가 레이아웃/스타일을 구성하고, C++은 품질 프리셋 적용과 열림/닫힘 애니메이션만 처리합니다.
 */
UCLASS()
class PROJECTG_API UPGSettingsScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	FPGSettingsScreenClosedSignature OnSettingsScreenClosed;

	// Settings 화면을 표시할 때 사용할 짧은 진입 애니메이션을 시작합니다.
	void PlayOpenAnimation();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<UWidget> RootPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<UBackgroundBlur> BackgroundBlur;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<UWidget> BackdropDim;

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

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<UButton> UltraPresetButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<USlider> ResolutionScaleSlider;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<UTextBlock> ResolutionScaleValueText;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<UButton> FrameRate30Button;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<UButton> FrameRate60Button;

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
	void HandleUltraPresetClicked();

	UFUNCTION()
	void HandleResolutionScaleChanged(float Value);

	UFUNCTION()
	void HandleResolutionScaleCommit();

	UFUNCTION()
	void HandleFrameRate30Clicked();

	UFUNCTION()
	void HandleFrameRate60Clicked();

	void RefreshControlsFromSettings();
	void ApplyPreset(EPGMobileQualityPreset Preset);
	void ApplyFrameRate(int32 FrameRateLimit);
	void ApplyAnimationState(float Alpha);
	void SetAllButtonsEnabled(bool bEnabled) const;
	void SetPresetSelectionState(EPGMobileQualityPreset Preset) const;
	void SetFrameRateSelectionState(int32 FrameRateLimit) const;
	void UpdateResolutionScaleText(float Value) const;

	bool bIsOpening = false;
	bool bIsClosing = false;
	bool bIsUpdatingControls = false;
	float AnimationElapsed = 0.0f;
	float AnimationDuration = 0.18f;
};
