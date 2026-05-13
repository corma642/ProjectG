// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PGRunPauseWidget.generated.h"

class UBackgroundBlur;
class UButton;
class UWidget;

DECLARE_MULTICAST_DELEGATE(FPGRunPauseResumeRequestedSignature);
DECLARE_MULTICAST_DELEGATE(FPGRunPauseSettingsRequestedSignature);
DECLARE_MULTICAST_DELEGATE(FPGRunPauseHomeRequestedSignature);

/**
 * 러닝 중 일시 정지 화면의 로직 위젯입니다.
 * BP가 패널과 버튼 디자인을 구성하고, C++은 Resume/Settings 바인딩과 블러/딤/패널 애니메이션만 처리합니다.
 */
UCLASS()
class PROJECTG_API UPGRunPauseWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	FPGRunPauseResumeRequestedSignature OnResumeRequested;
	FPGRunPauseSettingsRequestedSignature OnSettingsRequested;
	FPGRunPauseHomeRequestedSignature OnHomeRequested;

	void PlayOpenAnimation(float InTargetBlurStrength, float InTargetDimAlpha);
	void PlayCloseForResume();
	void SetPauseButtonsEnabled(bool bEnabled) const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Pause")
	TObjectPtr<UWidget> RootPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Pause")
	TObjectPtr<UBackgroundBlur> BackdropBlur;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Pause")
	TObjectPtr<UWidget> BackdropDim;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Pause")
	TObjectPtr<UWidget> PauseRootPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Pause")
	TObjectPtr<UWidget> PausePanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Pause")
	TObjectPtr<UButton> ResumeButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Pause")
	TObjectPtr<UButton> SettingsButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Pause")
	TObjectPtr<UButton> HomeButton;

private:
	UFUNCTION()
	void HandleResumeClicked();

	UFUNCTION()
	void HandleSettingsClicked();

	UFUNCTION()
	void HandleHomeClicked();

	void ApplyAnimationState(float Alpha, bool bClosing);

	bool bIsOpening = false;
	bool bIsClosingForResume = false;
	float AnimationElapsed = 0.0f;
	float AnimationDuration = 0.22f;
	float TargetBlurStrength = 4.0f;
	float TargetDimAlpha = 0.28f;
};
