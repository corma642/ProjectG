// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PGScreenTransitionFadeWidget.h"

#include "Components/Widget.h"

namespace
{
float EaseInOutCubicTransition(float Alpha)
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	return ClampedAlpha < 0.5f
		? 4.0f * ClampedAlpha * ClampedAlpha * ClampedAlpha
		: 1.0f - FMath::Pow(-2.0f * ClampedAlpha + 2.0f, 3.0f) * 0.5f;
}
}

void UPGScreenTransitionFadeWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyFadeOpacity(0.0f);
}

void UPGScreenTransitionFadeWidget::NativeDestruct()
{
	OnFadeCovered.Clear();
	OnFadeFinished.Clear();
	Super::NativeDestruct();
}

void UPGScreenTransitionFadeWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bIsPlaying)
	{
		return;
	}

	ElapsedTime += InDeltaTime;

	const float CoverEndTime = FadeInDuration;
	const float HoldEndTime = FadeInDuration + HoldDuration;
	const float TotalDuration = FadeInDuration + HoldDuration + FadeOutDuration;

	if (ElapsedTime < CoverEndTime)
	{
		const float Alpha = FadeInDuration > KINDA_SMALL_NUMBER ? ElapsedTime / FadeInDuration : 1.0f;
		ApplyFadeOpacity(MaxOpacity * EaseInOutCubicTransition(Alpha));
	}
	else if (ElapsedTime < HoldEndTime)
	{
		ApplyFadeOpacity(MaxOpacity);
		if (!bHasBroadcastCovered)
		{
			bHasBroadcastCovered = true;
			OnFadeCovered.Broadcast();
		}
	}
	else
	{
		if (!bHasBroadcastCovered)
		{
			bHasBroadcastCovered = true;
			OnFadeCovered.Broadcast();
		}

		const float FadeOutElapsed = ElapsedTime - HoldEndTime;
		const float Alpha = FadeOutDuration > KINDA_SMALL_NUMBER ? FadeOutElapsed / FadeOutDuration : 1.0f;
		ApplyFadeOpacity(MaxOpacity * (1.0f - EaseInOutCubicTransition(Alpha)));
	}

	if (ElapsedTime >= TotalDuration)
	{
		bIsPlaying = false;
		ApplyFadeOpacity(0.0f);
		OnFadeFinished.Broadcast();
		RemoveFromParent();
	}
}

void UPGScreenTransitionFadeWidget::PlayCoverThenReveal(float InFadeInDuration, float InHoldDuration, float InFadeOutDuration, float InMaxOpacity)
{
	FadeInDuration = FMath::Max(0.0f, InFadeInDuration);
	HoldDuration = FMath::Max(0.0f, InHoldDuration);
	FadeOutDuration = FMath::Max(0.0f, InFadeOutDuration);
	MaxOpacity = FMath::Clamp(InMaxOpacity, 0.0f, 1.0f);
	ElapsedTime = 0.0f;
	bIsPlaying = true;
	bHasBroadcastCovered = false;

	ApplyFadeOpacity(0.0f);

	// 모든 구간이 0이면 같은 프레임에 전환만 수행하고 위젯을 바로 정리합니다.
	if (FadeInDuration <= KINDA_SMALL_NUMBER && HoldDuration <= KINDA_SMALL_NUMBER && FadeOutDuration <= KINDA_SMALL_NUMBER)
	{
		bHasBroadcastCovered = true;
		OnFadeCovered.Broadcast();
		bIsPlaying = false;
		OnFadeFinished.Broadcast();
		RemoveFromParent();
	}
}

void UPGScreenTransitionFadeWidget::ApplyFadeOpacity(float Opacity) const
{
	if (FadePanel)
	{
		FadePanel->SetRenderOpacity(FMath::Clamp(Opacity, 0.0f, 1.0f));
	}
}
