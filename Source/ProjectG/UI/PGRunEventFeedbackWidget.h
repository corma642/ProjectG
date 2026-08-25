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
	/** 생성 직후 이벤트 종류·상태·초 단위 시간·-1~1 방향을 BP 연출에 전달하는 자동 구성 이벤트입니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Run Event Feedback")
	void ConfigureRunEventFeedback(EPGRunEventType EventType, EPGRunEventState EventState, float TimeUntilTrigger, float Duration, float Direction);
};
