// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PGRunResultWidget.generated.h"

/**
 * 런 실패 후 결과 화면에 사용하는 공통 위젯 베이스입니다.
 * C++ 결과 피드백 컴포넌트가 생성 직후 실패 원인, 시간, 거리, 점수를 전달합니다.
 */
UCLASS(Abstract, Blueprintable)
class PROJECTG_API UPGRunResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 결과 위젯 생성 직후 현재 런의 실패 정보와 기본 점수 정보를 전달합니다.
	UFUNCTION(BlueprintImplementableEvent, Category = "ProjectG | Run Result")
	void ConfigureRunResult(FName FailureReason, float RunTimeSeconds, float DistanceMeters, int32 Score);
};
