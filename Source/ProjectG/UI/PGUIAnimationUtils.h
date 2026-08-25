// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** UI 위젯의 짧은 상태 전환에 공통으로 사용하는 내부 easing 함수 모음입니다. */
namespace PGUIAnimation
{
	/** 0~1 입력을 초반에는 느리고 후반에는 빠른 cubic 곡선으로 변환합니다. */
	FORCEINLINE float EaseInCubic(float Alpha)
	{
		const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
		return ClampedAlpha * ClampedAlpha * ClampedAlpha;
	}

	/** 0~1 입력을 초반에는 빠르고 후반에는 느린 cubic 곡선으로 변환합니다. */
	FORCEINLINE float EaseOutCubic(float Alpha)
	{
		const float InverseAlpha = 1.0f - FMath::Clamp(Alpha, 0.0f, 1.0f);
		return 1.0f - InverseAlpha * InverseAlpha * InverseAlpha;
	}

	/** 0~1 입력의 양 끝을 부드럽게 감속하는 대칭 cubic 곡선으로 변환합니다. */
	FORCEINLINE float EaseInOutCubic(float Alpha)
	{
		const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
		if (ClampedAlpha < 0.5f)
		{
			return 4.0f * ClampedAlpha * ClampedAlpha * ClampedAlpha;
		}

		const float InverseAlpha = -2.0f * ClampedAlpha + 2.0f;
		return 1.0f - InverseAlpha * InverseAlpha * InverseAlpha * 0.5f;
	}

	/**
	 * 종료 지점을 잠시 넘어갔다 되돌아오는 back 곡선을 반환합니다.
	 * Overshoot가 클수록 목표 크기를 넘어서는 반동이 강해집니다.
	 */
	FORCEINLINE float EaseOutBack(float Alpha, float Overshoot = 1.70158f)
	{
		const float ShiftedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f) - 1.0f;
		return 1.0f + ShiftedAlpha * ShiftedAlpha * ((Overshoot + 1.0f) * ShiftedAlpha + Overshoot);
	}
}
