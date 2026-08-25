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
	/** 지정한 초 단위 전체 시간 동안 3, 2, 1, GO!를 순차 재생합니다. */
	void StartCountdown(float Duration);

protected:
	/** 카운트다운 텍스트와 루트 패널을 재생 전 투명 상태로 준비합니다. */
	virtual void NativeConstruct() override;

	/** 활성 카운트다운 동안 3-2-1-GO 단계와 scale 임팩트만 갱신합니다. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 전체 카운트다운 scale과 opacity를 적용하는 필수 BP 루트입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Countdown")
	TObjectPtr<UWidget> RootPanel;

	// 3, 2, 1, GO! 문구를 단계별로 표시하는 필수 텍스트입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Countdown")
	TObjectPtr<UTextBlock> CountdownText;

private:
	void UpdateCountdownText(int32 StepIndex);
	void ApplyStepAnimation(float StepAlpha);

	bool bIsCountdownActive = false;
	float CountdownElapsed = 0.0f;
	float CountdownDuration = 3.0f;
	int32 CurrentStepIndex = INDEX_NONE;
};
