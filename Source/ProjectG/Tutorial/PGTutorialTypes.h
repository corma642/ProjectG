// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PGTutorialTypes.generated.h"

/**
 * 튜토리얼 스테이지가 차례로 안내하는 조작 단계입니다.
 * 안내 위젯(UPGRunGuideWidget)의 표시 단계와 Tutorial.Chapters의 구간 종류가 같은 값을 씁니다.
 */
UENUM(BlueprintType)
enum class EPGTutorialStep : uint8
{
	None,
	// 드래그로 균형 잡기
	Balance,
	// 위로 튕겨 점프
	Jump,
	// 좌우 플릭·드래그로 옆 길로 이동
	Lateral,
	// 떨어지는 적재물을 바구니에 받기
	Cargo,
	// 바람 이벤트를 버티기
	Wind,
	// 모든 단계 완료(보상 표시)
	Completed
};
