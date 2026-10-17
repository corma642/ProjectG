// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * 터치 궤적을 플릭 방향으로 분류하는 순수 함수 모음입니다.
 * 컨트롤러가 터치 시작점 또는 최근 샘플 창의 이동량을 넘기고, 단위(px)와 기준값은 호출자가 맞춥니다. 자동화 테스트가 같은 함수를 검사합니다.
 */
namespace PGTouchGesture
{
	enum class EFlickDirection : uint8
	{
		None,
		Left,
		Right,
		Up,
	};

	/**
	 * 이동량·시간이 플릭 기준(최소 거리, 최대 시간, 최소 속도)을 모두 넘고 한 축이 다른 축보다 DirectionDominance배 이상 클 때만 방향을 돌려줍니다.
	 * bAllowLateral이 false면 좌우 플릭은 None입니다(드래그 중에는 균형 이동과 구분이 안 돼 위 플릭만 받는다). 아래 방향 플릭은 없습니다.
	 */
	inline EFlickDirection ClassifyFlick(const FVector2D& DeltaPx, float DurationSeconds, float MinDistancePx, float MaxDurationSeconds, float MinSpeedPxPerSec, float DirectionDominance, bool bAllowLateral)
	{
		const float Distance = DeltaPx.Size();
		const float Speed = Distance / FMath::Max(DurationSeconds, KINDA_SMALL_NUMBER);
		if (DurationSeconds > MaxDurationSeconds || Distance < MinDistancePx || Speed < MinSpeedPxPerSec)
		{
			return EFlickDirection::None;
		}

		const float AbsX = FMath::Abs(DeltaPx.X);
		const float AbsY = FMath::Abs(DeltaPx.Y);
		const float Dominance = FMath::Max(1.0f, DirectionDominance);
		if (AbsX >= AbsY * Dominance)
		{
			if (!bAllowLateral)
			{
				return EFlickDirection::None;
			}
			return DeltaPx.X > 0.0f ? EFlickDirection::Right : EFlickDirection::Left;
		}

		// UE 화면 좌표는 Y가 아래로 증가하므로 위쪽 플릭은 음수 Y 이동입니다.
		if (DeltaPx.Y < 0.0f && AbsY >= AbsX * Dominance)
		{
			return EFlickDirection::Up;
		}

		return EFlickDirection::None;
	}
}
