// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RunEvents/PGRunEventSet.h"
#include "PGRunEventFeedbackWidget.generated.h"

/**
 * 런 이벤트 자동 연출 위젯의 선택형 베이스 클래스입니다.
 * 사용자가 이 클래스를 부모로 위젯을 만들면 이벤트 타입, 상태, 방향, 시간이 자동으로 전달됩니다.
 */
UCLASS(Abstract, Blueprintable)
class PROJECTG_API UPGRunEventFeedbackWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// FeedbackComponent가 위젯 생성 직후 호출하는 자동 구성 이벤트입니다.
	UFUNCTION(BlueprintImplementableEvent, Category = "Run Event Feedback")
	void ConfigureRunEventFeedback(EPGRunEventType EventType, EPGRunEventState EventState, float TimeUntilTrigger, float Duration, float Direction);
};
