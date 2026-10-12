// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PGRunGuideWidget.h"

#include "Components/Widget.h"
#include "UI/PGUIAnimationUtils.h"

namespace
{
	// 페이드 인/아웃에 걸리는 시간(초)
	constexpr float FadeDuration = 0.25f;

	// 드래그 아이콘의 좌우 왕복 주기(초)와 이동 폭(px). 실제 드래그처럼 천천히 넓게 오간다.
	constexpr float DragCycleSeconds = 1.6f;
	constexpr float DragAmplitude = 90.0f;

	// 플릭 아이콘의 반복 주기(초)와 위로 튀는 거리(px). 주기의 앞 35%만 움직여 '짧고 빠른' 플릭으로 읽히게 한다.
	constexpr float FlickCycleSeconds = 1.2f;
	constexpr float FlickDistance = 100.0f;
	constexpr float FlickMoveRatio = 0.35f;
	constexpr float FlickFadeRatio = 0.25f;
}

void UPGRunGuideWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 가이드는 Run HUD 위에 겹치지만 터치는 모두 게임 조작으로 흘려보내야 한다.
	SetVisibility(ESlateVisibility::HitTestInvisible);
	ApplyStepVisibility();
	if (RootPanel)
	{
		RootPanel->SetRenderOpacity(FadeAlpha);
	}
}

void UPGRunGuideWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (CurrentStep == EPGRunGuideStep::None && !bIsDismissing)
	{
		return;
	}

	const float FadeTarget = bIsDismissing ? 0.0f : 1.0f;
	FadeAlpha = FMath::FInterpConstantTo(FadeAlpha, FadeTarget, InDeltaTime, 1.0f / FadeDuration);
	if (RootPanel)
	{
		RootPanel->SetRenderOpacity(FadeAlpha);
	}
	if (bIsDismissing)
	{
		if (FadeAlpha <= 0.0f)
		{
			RemoveFromParent();
		}
		return;
	}

	GestureElapsed += InDeltaTime;
	if (CurrentStep == EPGRunGuideStep::Drag && DragGestureIcon)
	{
		const float Phase = FMath::Fmod(GestureElapsed, DragCycleSeconds) / DragCycleSeconds;
		DragGestureIcon->SetRenderTranslation(FVector2D(FMath::Sin(Phase * 2.0f * PI) * DragAmplitude, 0.0f));
	}
	else if (CurrentStep == EPGRunGuideStep::Flick && FlickGestureIcon)
	{
		const float Phase = FMath::Fmod(GestureElapsed, FlickCycleSeconds) / FlickCycleSeconds;
		const float MoveAlpha = PGUIAnimation::EaseOutCubic(FMath::Clamp(Phase / FlickMoveRatio, 0.0f, 1.0f));
		FlickGestureIcon->SetRenderTranslation(FVector2D(0.0f, -MoveAlpha * FlickDistance));
		// 튀어 오른 뒤에는 사라졌다가 바닥에서 다시 나타나 한 번의 플릭이 반복되는 것처럼 보이게 한다.
		FlickGestureIcon->SetRenderOpacity(1.0f - FMath::Clamp((Phase - FlickMoveRatio) / FlickFadeRatio, 0.0f, 1.0f));
	}
}

void UPGRunGuideWidget::ShowStep(EPGRunGuideStep Step)
{
	CurrentStep = Step;
	bIsDismissing = false;
	GestureElapsed = 0.0f;
	ApplyStepVisibility();
}

void UPGRunGuideWidget::Dismiss()
{
	bIsDismissing = true;
}

void UPGRunGuideWidget::ApplyStepVisibility() const
{
	const auto SetShown = [](UWidget* Widget, bool bShown)
	{
		if (Widget)
		{
			Widget->SetVisibility(bShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	};

	SetShown(DragPrompt, CurrentStep == EPGRunGuideStep::Drag);
	SetShown(FlickPrompt, CurrentStep == EPGRunGuideStep::Flick);
}
