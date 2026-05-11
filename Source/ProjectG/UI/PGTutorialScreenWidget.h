// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PGTutorialScreenWidget.generated.h"

class UButton;
class UImage;
class UPGTutorialPageSet;
class UWidget;

DECLARE_MULTICAST_DELEGATE(FPGTutorialClosedSignature);

/**
 * Home Help에서 표시하는 책자형 튜토리얼 위젯입니다.
 * BP가 전체 레이아웃/버튼 스타일/이미지 배치를 담당하고, C++은 페이지 데이터 적용과 넘김 애니메이션만 처리합니다.
 */
UCLASS()
class PROJECTG_API UPGTutorialScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	FPGTutorialClosedSignature OnTutorialClosed;

	// 튜토리얼 페이지셋을 연결하고 첫 페이지를 표시합니다.
	void InitializeTutorial(UPGTutorialPageSet* InPageSet);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Tutorial")
	TObjectPtr<UWidget> RootPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Tutorial")
	TObjectPtr<UImage> CurrentPageImage;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Tutorial")
	TObjectPtr<UImage> IncomingPageImage;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Tutorial")
	TObjectPtr<UButton> PrevButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Tutorial")
	TObjectPtr<UButton> NextButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Tutorial")
	TObjectPtr<UButton> CloseButton;

private:
	UFUNCTION()
	void HandlePrevClicked();

	UFUNCTION()
	void HandleNextClicked();

	UFUNCTION()
	void HandleCloseClicked();

	void StartPageTransition(int32 NewPageIndex, int32 Direction);
	void ApplyPageToImage(UImage* Image, int32 PageIndex) const;
	void ApplyTransitionState(float Alpha);
	void FinishPageTransition();
	void UpdateButtonState() const;
	void SetNavigationButtonsEnabled(bool bEnabled) const;
	void ApplyButtonEnabledState(UButton* Button, bool bEnabled) const;
	float GetPageTravelDistance() const;

	UPROPERTY(Transient)
	TObjectPtr<UPGTutorialPageSet> PageSet;

	int32 CurrentPageIndex = 0;
	int32 TargetPageIndex = INDEX_NONE;
	int32 TransitionDirection = 1;
	bool bIsTransitioning = false;
	float TransitionElapsed = 0.0f;
	float TransitionDuration = 0.32f;
};
