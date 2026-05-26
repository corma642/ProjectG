// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PGScreenTransitionFadeWidget.generated.h"

class UWidget;

DECLARE_MULTICAST_DELEGATE(FPGScreenTransitionFadeCoveredSignature);
DECLARE_MULTICAST_DELEGATE(FPGScreenTransitionFadeFinishedSignature);

/**
 * 화면 전환 사이의 카메라/위젯 교체 간극을 덮는 경량 전체 화면 페이드 위젯입니다.
 * BP는 전체 화면 패널의 색상과 배치를 담당하고, C++은 전환 타이밍과 투명도만 제어합니다.
 */
UCLASS()
class PROJECTG_API UPGScreenTransitionFadeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	FPGScreenTransitionFadeCoveredSignature OnFadeCovered;
	FPGScreenTransitionFadeFinishedSignature OnFadeFinished;

	/** FadePanel을 어둡게 덮은 뒤 잠깐 유지하고 다시 걷어내는 전환 애니메이션을 시작합니다. */
	void PlayCoverThenReveal(float InFadeInDuration, float InHoldDuration, float InFadeOutDuration, float InMaxOpacity);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 전체 화면 레이어 루트입니다. BP에서 SafeZone 바깥까지 꽉 채우는 기준 패널로 둡니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Transition")
	TObjectPtr<UWidget> RootPanel;

	// 실제 어두운 페이드 이미지를 담당하는 패널입니다. C++은 RenderOpacity만 조절합니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Transition")
	TObjectPtr<UWidget> FadePanel;

private:
	void ApplyFadeOpacity(float Opacity) const;

	float FadeInDuration = 0.12f;
	float HoldDuration = 0.04f;
	float FadeOutDuration = 0.18f;
	float MaxOpacity = 0.85f;
	float ElapsedTime = 0.0f;
	bool bIsPlaying = false;
	bool bHasBroadcastCovered = false;
};
