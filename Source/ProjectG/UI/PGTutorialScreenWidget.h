// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PGTutorialScreenWidget.generated.h"

class UButton;
class UImage;
class UPGTutorialPageSet;
class UWidget;

/** 튜토리얼 닫기 입력을 Home 흐름에 전달합니다. */
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
	/** 닫힌 튜토리얼 모달을 Home 흐름에 알립니다. */
	FPGTutorialClosedSignature OnTutorialClosed;

	/** 튜토리얼 페이지셋을 연결하고 첫 페이지를 표시합니다. null이면 빈 상태를 유지합니다. */
	void InitializeTutorial(UPGTutorialPageSet* InPageSet);

protected:
	/** 이전·다음·닫기 버튼 delegate와 두 페이지 이미지의 초기 상태를 준비합니다. */
	virtual void NativeConstruct() override;

	/** 버튼 delegate를 제거 전에 해제합니다. */
	virtual void NativeDestruct() override;

	/** 페이지 넘김 중 현재/다음 이미지의 이동과 opacity만 갱신합니다. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 튜토리얼 전체 화면 레이아웃의 필수 BP 루트입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Tutorial")
	TObjectPtr<UWidget> RootPanel;

	// 현재 페이지를 표시하고 화면 밖으로 이동시키는 필수 이미지입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Tutorial")
	TObjectPtr<UImage> CurrentPageImage;

	// 다음 페이지를 화면 밖에서 준비해 안으로 이동시키는 필수 이미지입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Tutorial")
	TObjectPtr<UImage> IncomingPageImage;

	/** 첫 페이지에서는 비활성화되는 필수 이전 페이지 버튼입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Tutorial")
	TObjectPtr<UButton> PrevButton;

	/** 마지막 페이지에서는 비활성화되는 필수 다음 페이지 버튼입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Tutorial")
	TObjectPtr<UButton> NextButton;

	/** 튜토리얼을 즉시 닫고 Home 입력을 복구하는 필수 버튼입니다. */
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
