// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/PGRunEventFeedbackWidget.h"
#include "PGRunEventBadgeWidget.generated.h"

class UWidget;

/**
 * 런 이벤트 전조와 진행을 화면 상단 배지 하나로 알리는 위젯입니다.
 * 전조 동안 아이콘 둘레의 링이 차오르고, 발동 뒤에는 배지가 작은 칩으로 줄어 링이 남은 시간만큼 비워집니다.
 * BP가 아이콘·이름·색을 정하고(이벤트마다 WBP 하나), C++은 방향 표시 선택, 링 진행, 칩 축소 애니메이션만 맡습니다.
 */
UCLASS(Abstract)
class PROJECTG_API UPGRunEventBadgeWidget : public UPGRunEventFeedbackWidget
{
	GENERATED_BODY()

protected:
	/** 상태(전조/발동)와 시간을 저장하고 방향 -1/0/+1에 맞는 표시 하나만 켭니다. 발동 상태에서는 이름을 숨기고 칩 축소를 시작합니다. */
	virtual void ConfigureRunEventFeedback_Implementation(EPGRunEventType EventType, EPGRunEventState EventState, float TimeUntilTrigger, float Duration, float Direction) override;

	/** 경과 시간으로 링 진행과 칩 축소 배율을 갱신합니다. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** RingAnchor 둘레에 진행 링(전조: 차오름, 발동: 비워짐)을 선으로 그립니다. */
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	// 아이콘·이름·방향 표시를 담는 배지 전체입니다. 발동 뒤 윗변 중앙을 기준으로 칩 크기로 줄어듭니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run Event")
	TObjectPtr<UWidget> BadgePanel;

	// 진행 링을 그릴 기준 위젯입니다. 이 위젯에 내접하는 원 바로 바깥에 링을 그립니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run Event")
	TObjectPtr<UWidget> RingAnchor;

	// 전조 동안만 보이는 이벤트 이름입니다. 칩 상태에서는 숨깁니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run Event")
	TObjectPtr<UWidget> NameText;

	// 방향이 -1 / 0 / +1일 때 각각 하나만 보이는 표시입니다. 방향이 없는 이벤트의 WBP는 두지 않습니다(바람은 ±1만, 눈보라는 없음).
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional), Category = "Run Event")
	TObjectPtr<UWidget> DirectionNegative;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional), Category = "Run Event")
	TObjectPtr<UWidget> DirectionZero;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional), Category = "Run Event")
	TObjectPtr<UWidget> DirectionPositive;

private:
	EPGRunEventState EventState = EPGRunEventState::Inactive;
	float ElapsedSeconds = 0.0f;
	// 전조면 발동까지, 발동이면 끝날 때까지의 전체 시간(초)입니다.
	float TotalSeconds = 0.0f;
	// 매 그리기마다 다시 채우는 링 꼭짓점 버퍼입니다. NativePaint가 const라 mutable로 둡니다.
	mutable TArray<FVector2D> RingPoints;
};
