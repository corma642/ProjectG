// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PGRunEventDirectionalPreviewWidget.h"

#include "Components/Widget.h"

void UPGRunEventDirectionalPreviewWidget::ConfigureRunEventFeedback_Implementation(EPGRunEventType EventType, EPGRunEventState EventState, float TimeUntilTrigger, float Duration, float Direction)
{
	const auto SetWarningShown = [](UWidget* Warning, bool bShown)
	{
		if (Warning)
		{
			Warning->SetVisibility(bShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	};

	// 방향은 -1(왼쪽)·0(중앙)·1(오른쪽) 구역 부호로 전달된다. 해당 구역 경고만 남겨 어느 쪽을 피해야 하는지 한눈에 보이게 한다.
	const bool bIsCenter = FMath::IsNearlyZero(Direction);
	SetWarningShown(LeftWarning, !bIsCenter && Direction < 0.0f);
	SetWarningShown(CenterWarning, bIsCenter);
	SetWarningShown(RightWarning, !bIsCenter && Direction > 0.0f);
}
