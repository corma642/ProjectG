// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PGRunPauseWidget.generated.h"

class UBackgroundBlur;
class UButton;
class UWidget;

/** Resume 버튼 입력을 HUD 흐름에 전달합니다. */
DECLARE_MULTICAST_DELEGATE(FPGRunPauseResumeRequestedSignature);
/** Settings 버튼 입력을 HUD 흐름에 전달합니다. */
DECLARE_MULTICAST_DELEGATE(FPGRunPauseSettingsRequestedSignature);
/** Home 버튼 입력을 HUD 흐름에 전달합니다. */
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
	/** 일시정지 해제 요청을 상위 HUD 컴포넌트에 전달합니다. */
	FPGRunPauseResumeRequestedSignature OnResumeRequested;
	/** Pause 위에 Settings 모달을 열도록 상위 HUD 컴포넌트에 요청합니다. */
	FPGRunPauseSettingsRequestedSignature OnSettingsRequested;
	/** 현재 런을 정산하고 Home으로 이동하도록 상위 HUD 컴포넌트에 요청합니다. */
	FPGRunPauseHomeRequestedSignature OnHomeRequested;

	/** 목표 blur 강도와 dim opacity를 적용하는 Pause 진입 애니메이션을 시작합니다. */
	void PlayOpenAnimation(float InTargetBlurStrength, float InTargetDimAlpha);

	/** Resume 입력을 잠그고 역방향 퇴장 애니메이션을 시작합니다. */
	void PlayCloseForResume();

	/** Settings 모달 또는 화면 전환 중 세 Pause 버튼 입력을 일괄 잠급니다. */
	void SetPauseButtonsEnabled(bool bEnabled) const;

protected:
	/** BP 버튼 delegate를 연결하고 투명한 초기 상태를 준비합니다. */
	virtual void NativeConstruct() override;

	/** 위젯 제거 전에 세 버튼 delegate를 해제합니다. */
	virtual void NativeDestruct() override;

	/** Pause 진입 또는 Resume 퇴장 애니메이션 동안만 blur/dim/panel 상태를 갱신합니다. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** 전체 Pause 모달의 표시 여부와 opacity 기준이 되는 필수 BP 루트입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Pause")
	TObjectPtr<UWidget> RootPanel;

	/** 일시 정지된 게임 화면에 적용할 필수 배경 블러입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Pause")
	TObjectPtr<UBackgroundBlur> BackdropBlur;

	/** 블러 위 목표 opacity를 적용하는 필수 반투명 딤 레이어입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Pause")
	TObjectPtr<UWidget> BackdropDim;

	/** Pause 패널 진입 scale과 이동을 적용하는 필수 애니메이션 루트입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Pause")
	TObjectPtr<UWidget> PauseRootPanel;

	/** 제목과 세 액션 버튼을 담는 필수 Pause 콘텐츠 패널입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Pause")
	TObjectPtr<UWidget> PausePanel;

	/** 런을 같은 상태에서 재개하도록 요청하는 필수 버튼입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Pause")
	TObjectPtr<UButton> ResumeButton;

	/** Pause 위에 Settings 모달을 열도록 요청하는 필수 버튼입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Pause")
	TObjectPtr<UButton> SettingsButton;

	/** 현재 런을 정산하고 Home으로 복귀하도록 요청하는 필수 버튼입니다. */
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
