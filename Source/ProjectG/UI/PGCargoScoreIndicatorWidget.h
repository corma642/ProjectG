// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PGCargoScoreIndicatorWidget.generated.h"

class UTextBlock;
class UWidget;

/**
 * 적재물 배달 완료 시 HUD 적재물 카드 근처에 추가 점수를 떠오르게 표시하는 위젯입니다.
 * BP가 폰트/색상/외곽선/배치를 담당하고, C++은 점수 텍스트와 짧은 상승/페이드 애니메이션만 처리합니다.
 */
UCLASS()
class PROJECTG_API UPGCargoScoreIndicatorWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 표시할 점수를 설정하고 상승/페이드 애니메이션을 시작합니다.
	void PlayScoreIndicator(int32 Score);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Cargo Score")
	TObjectPtr<UWidget> RootPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Cargo Score")
	TObjectPtr<UTextBlock> ScoreText;

private:
	void ApplyAnimationState(float Alpha);

	bool bIsAnimating = false;
	float AnimationElapsed = 0.0f;
	float AnimationDuration = 0.85f;
};
