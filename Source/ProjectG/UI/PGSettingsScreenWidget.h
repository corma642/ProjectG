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

/** Settings 닫힘 애니메이션이 끝나 상위 Home/Pause 흐름이 입력을 복구할 때 발생합니다. */
DECLARE_MULTICAST_DELEGATE(FPGSettingsScreenClosedSignature);

/**
 * Home과 Pause에서 공용으로 사용하는 Settings 화면 로직 위젯입니다.
 * BP는 패널 배치와 스타일을 담당하고, C++은 품질/볼륨/FPS 표시 설정 반영과 열림/닫힘 애니메이션만 처리합니다.
 */
UCLASS()
class PROJECTG_API UPGSettingsScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 닫힌 Settings 모달을 상위 화면 흐름에 알립니다. */
	FPGSettingsScreenClosedSignature OnSettingsScreenClosed;

	/** 위젯을 화면에 추가한 직후 배경 블러/딤과 패널 진입 애니메이션을 시작합니다. */
	void PlayOpenAnimation();

protected:
	/** BP 컨트롤 delegate를 연결하고 저장된 품질·볼륨 값을 UI에 반영합니다. */
	virtual void NativeConstruct() override;

	/** 모든 컨트롤 delegate를 제거 전에 해제합니다. */
	virtual void NativeDestruct() override;

	/** Settings 열림 또는 닫힘 애니메이션 동안만 blur/dim/panel 상태를 갱신합니다. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Settings 모달 전체의 opacity 기준이 되는 필수 BP 루트입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<UWidget> RootPanel;

	/** 뒤 Home/Pause 화면을 약하게 흐리게 만드는 필수 전체 화면 블러입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<UBackgroundBlur> BackgroundBlur;

	/** 블러 위에 목표 opacity를 적용하는 필수 반투명 딤 레이어입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<UWidget> BackdropDim;

	/** 설정 항목에 진입 scale과 이동 애니메이션을 적용하는 필수 중앙 패널입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<UWidget> SettingsPanel;

	/** 설정을 저장하고 닫힘 애니메이션을 시작하는 필수 버튼입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<UButton> CloseButton;

	/** 모바일 Low 품질 프리셋을 즉시 적용하는 필수 버튼입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<UButton> LowPresetButton;

	/** 모바일 Balanced 품질 프리셋을 즉시 적용하는 필수 버튼입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<UButton> BalancedPresetButton;

	/** 모바일 High 품질 프리셋을 즉시 적용하는 필수 버튼입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<UButton> HighPresetButton;

	/** 설정의 최소·최대 범위 안에서 내부 렌더 해상도 비율을 조절하는 필수 슬라이더입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<USlider> ResolutionScaleSlider;

	/** 현재 렌더 해상도 비율을 백분율로 표시하는 필수 텍스트입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<UTextBlock> ResolutionScaleValueText;

	/** Android 실기기 성능 확인용 FPS 오버레이 표시를 전환하는 필수 체크박스입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<UCheckBox> ShowFPSCheckBox;

	/** 음악 볼륨을 0~150% 범위로 조절하는 필수 슬라이더입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<USlider> MusicVolumeSlider;

	/** 현재 음악 볼륨을 백분율로 표시하는 필수 텍스트입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<UTextBlock> MusicVolumeValueText;

	/** 효과음 볼륨을 0~150% 범위로 조절하는 필수 슬라이더입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<USlider> SFXVolumeSlider;

	/** 현재 효과음 볼륨을 백분율로 표시하는 필수 텍스트입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<UTextBlock> SFXVolumeValueText;

	/** 개인정보처리방침(`Go Deliver! Settings > Legal > PrivacyPolicyUrl`)을 외부 브라우저로 여는 필수 버튼입니다. Google Play는 앱 안에서도 방침 링크를 요구합니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Settings")
	TObjectPtr<UButton> PrivacyPolicyButton;

private:
	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandlePrivacyPolicyClicked();

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
	void ApplyAnimationState(float Alpha);
	void SetAllButtonsEnabled(bool bEnabled) const;
	void SetPresetSelectionState(EPGMobileQualityPreset Preset) const;
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
