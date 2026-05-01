// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Cargo/PGCargoPresentationSet.h"
#include "PGCargoFeedbackWidget.generated.h"

/**
 * 적재물 자동 피드백 위젯의 공통 베이스입니다.
 * C++이 생성 직후 이벤트 종류와 적재물 인덱스, 표시 시간을 전달합니다.
 */
UCLASS(Abstract, Blueprintable)
class PROJECTG_API UPGCargoFeedbackWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 자동 생성된 적재물 피드백 위젯에 현재 이벤트 정보를 전달합니다.
	UFUNCTION(BlueprintImplementableEvent, Category = "ProjectG | Cargo Feedback")
	void ConfigureCargoFeedback(EPGCargoFeedbackType FeedbackType, int32 CargoIndex, float DisplayDuration);
};
