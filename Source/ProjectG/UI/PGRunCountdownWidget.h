// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PGRunCountdownWidget.generated.h"

class UTextBlock;
class UWidget;

/**
 * 러닝 시작 전 3-2-1-Go 카운트다운 Blueprint Widget의 로직 클래스입니다.
 * BP는 위치, 폰트, 스타일을 담당하고 C++은 텍스트 전환과 임팩트 애니메이션만 처리합니다.
 */
UCLASS()
class PROJECTG_API UPGRunCountdownWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 지정한 전체 시간 동안 3, 2, 1, GO!를 순차 재생합니다.
	void StartCountdown(float Duration);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Countdown")
	TObjectPtr<UWidget> RootPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Countdown")
	TObjectPtr<UTextBlock> CountdownText;

private:
	void ValidateRequiredWidgets() const;
	void UpdateCountdownText(int32 StepIndex);
	void ApplyStepAnimation(float StepAlpha);

	bool bIsCountdownActive = false;
	float CountdownElapsed = 0.0f;
	float CountdownDuration = 3.0f;
	int32 CurrentStepIndex = INDEX_NONE;
};
