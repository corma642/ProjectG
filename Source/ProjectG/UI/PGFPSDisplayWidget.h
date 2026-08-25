// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PGFPSDisplayWidget.generated.h"

class UTextBlock;
class UWidget;

/**
 * Android 실기기 프레임 테스트를 위해 현재 평균 FPS를 표시하는 위젯 로직 클래스입니다.
 * 실제 위치, 배경, 폰트, SafeZone 대응은 Widget Blueprint가 담당합니다.
 */
UCLASS()
class PROJECTG_API UPGFPSDisplayWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	/** 누적 프레임과 갱신 시간을 초기화하고 첫 표시값을 준비합니다. */
	virtual void NativeConstruct() override;

	/** 매 프레임 문자열을 만들지 않고 설정된 간격마다 평균 FPS 텍스트를 갱신합니다. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// FPS 오버레이의 SafeZone 대응과 배경 배치를 담당하는 필수 BP 루트입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "FPS Display")
	TObjectPtr<UWidget> RootPanel;

	// 측정한 평균 FPS 숫자를 표시하는 필수 텍스트입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "FPS Display")
	TObjectPtr<UTextBlock> FPSText;

private:
	void UpdateFPSText(float FPS) const;

	float AccumulatedTime = 0.0f;
	int32 AccumulatedFrames = 0;
	float UpdateInterval = 0.25f;
};
