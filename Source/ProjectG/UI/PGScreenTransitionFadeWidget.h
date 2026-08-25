// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PGScreenTransitionFadeWidget.generated.h"

class UWidget;

/** fade가 화면을 완전히 덮어 카메라·위젯 교체가 안전해진 순간 발생합니다. */
DECLARE_MULTICAST_DELEGATE(FPGScreenTransitionFadeCoveredSignature);
/** fade-out까지 끝나 화면 전환 입력을 다시 받을 수 있을 때 발생합니다. */
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
	/** 화면이 완전히 가려진 순간 전환 작업을 수행하도록 알립니다. */
	FPGScreenTransitionFadeCoveredSignature OnFadeCovered;
	/** 전체 fade 전환이 끝난 순간 상위 흐름의 잠금을 해제하도록 알립니다. */
	FPGScreenTransitionFadeFinishedSignature OnFadeFinished;

	/** FadePanel을 어둡게 덮은 뒤 잠깐 유지하고 다시 걷어내는 전환 애니메이션을 시작합니다. */
	void PlayCoverThenReveal(float InFadeInDuration, float InHoldDuration, float InFadeOutDuration, float InMaxOpacity);

protected:
	/** fade 패널 opacity를 투명한 시작 상태로 초기화합니다. */
	virtual void NativeConstruct() override;

	/** 제거 전에 외부 delegate를 비워 수명 종료 후 호출을 방지합니다. */
	virtual void NativeDestruct() override;

	/** fade-in, hold, fade-out 세 구간을 순서대로 진행하고 경계 delegate를 한 번씩 발생시킵니다. */
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
