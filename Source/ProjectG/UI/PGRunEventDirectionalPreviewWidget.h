// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/PGRunEventFeedbackWidget.h"
#include "PGRunEventDirectionalPreviewWidget.generated.h"

class UWidget;

/**
 * 좌/중/우 가상 구역 중 한 곳으로 다가오는 런 이벤트(이동형 위험 구역)의 전조 경고 위젯입니다.
 * BP는 세 경고 위젯의 배치와 스타일을 담당하고, C++은 전달받은 방향에 해당하는 경고 하나만 표시합니다.
 */
UCLASS(Abstract, Blueprintable)
class PROJECTG_API UPGRunEventDirectionalPreviewWidget : public UPGRunEventFeedbackWidget
{
	GENERATED_BODY()

protected:
	/** Direction이 음수면 왼쪽, 0이면 중앙, 양수면 오른쪽 경고만 보이게 합니다. */
	virtual void ConfigureRunEventFeedback_Implementation(EPGRunEventType EventType, EPGRunEventState EventState, float TimeUntilTrigger, float Duration, float Direction) override;

	// 왼쪽 구역으로 위험이 다가올 때 표시할 경고입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> LeftWarning;

	// 중앙 구역으로 위험이 다가올 때 표시할 경고입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> CenterWarning;

	// 오른쪽 구역으로 위험이 다가올 때 표시할 경고입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> RightWarning;
};
