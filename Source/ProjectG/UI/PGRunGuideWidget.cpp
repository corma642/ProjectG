// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PGRunGuideWidget.h"

#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "UI/PGUIAnimationUtils.h"

namespace
{
	// 페이드 인/아웃에 걸리는 시간(초)
	constexpr float FadeDuration = 0.25f;

	// 드래그 아이콘의 좌우 왕복 주기(초)와 이동 폭(px). 실제 드래그처럼 천천히 넓게 오간다.
	constexpr float DragCycleSeconds = 1.6f;
	constexpr float DragAmplitude = 90.0f;

	// 플릭 아이콘의 반복 주기(초)와 튀는 거리(px). 주기의 앞 35%만 움직여 '짧고 빠른' 플릭으로 읽히게 한다.
	constexpr float FlickCycleSeconds = 1.2f;
	constexpr float FlickDistance = 100.0f;
	constexpr float FlickMoveRatio = 0.35f;
	constexpr float FlickFadeRatio = 0.25f;

	// 완료 문구를 보여 주는 시간(초). 보상 숫자를 읽을 만큼만 두고 사라진다.
	constexpr float CompletionHoldSeconds = 3.0f;
}

void UPGRunGuideWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 안내는 Run HUD 위에 겹치지만 터치는 모두 게임 조작으로 흘려보내야 한다.
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

	const bool bShowing = (CurrentStep != EPGTutorialStep::None || bShowingCompletion) && !bIsDismissing;
	if (!bShowing && !bIsDismissing && FadeAlpha <= 0.0f)
	{
		// 단계 사이: 숨긴 채 다음 단계를 기다린다.
		return;
	}

	FadeAlpha = FMath::FInterpConstantTo(FadeAlpha, bShowing ? 1.0f : 0.0f, InDeltaTime, 1.0f / FadeDuration);
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
	if (!bShowing)
	{
		return;
	}

	if (bShowingCompletion)
	{
		CompletionRemaining -= InDeltaTime;
		if (CompletionRemaining <= 0.0f)
		{
			Dismiss();
		}
		return;
	}
	UpdateGestureIcons(InDeltaTime);
}

void UPGRunGuideWidget::UpdateGestureIcons(float DeltaSeconds)
{
	GestureElapsed += DeltaSeconds;
	if (CurrentStep == EPGTutorialStep::Balance && DragGestureIcon)
	{
		const float Phase = FMath::Fmod(GestureElapsed, DragCycleSeconds) / DragCycleSeconds;
		DragGestureIcon->SetRenderTranslation(FVector2D(FMath::Sin(Phase * 2.0f * PI) * DragAmplitude, 0.0f));
	}
	else if (CurrentStep == EPGTutorialStep::Jump && FlickGestureIcon)
	{
		const float Phase = FMath::Fmod(GestureElapsed, FlickCycleSeconds) / FlickCycleSeconds;
		const float MoveAlpha = PGUIAnimation::EaseOutCubic(FMath::Clamp(Phase / FlickMoveRatio, 0.0f, 1.0f));
		FlickGestureIcon->SetRenderTranslation(FVector2D(0.0f, -MoveAlpha * FlickDistance));
		// 튀어 오른 뒤에는 사라졌다가 바닥에서 다시 나타나 한 번의 플릭이 반복되는 것처럼 보이게 한다.
		FlickGestureIcon->SetRenderOpacity(1.0f - FMath::Clamp((Phase - FlickMoveRatio) / FlickFadeRatio, 0.0f, 1.0f));
	}
	else if (CurrentStep == EPGTutorialStep::Lateral && LateralGestureIcon)
	{
		// 한 주기는 오른쪽, 다음 주기는 왼쪽으로 튕겨 양쪽 길이 모두 있음을 보여 준다.
		const float Cycles = GestureElapsed / FlickCycleSeconds;
		const float Direction = FMath::FloorToInt(Cycles) % 2 == 0 ? 1.0f : -1.0f;
		const float Phase = FMath::Frac(Cycles);
		const float MoveAlpha = PGUIAnimation::EaseOutCubic(FMath::Clamp(Phase / FlickMoveRatio, 0.0f, 1.0f));
		LateralGestureIcon->SetRenderTranslation(FVector2D(Direction * MoveAlpha * FlickDistance, 0.0f));
		LateralGestureIcon->SetRenderOpacity(1.0f - FMath::Clamp((Phase - FlickMoveRatio) / FlickFadeRatio, 0.0f, 1.0f));
	}
}

void UPGRunGuideWidget::ShowStep(EPGTutorialStep Step)
{
	CurrentStep = Step;
	bShowingCompletion = false;
	bIsDismissing = false;
	GestureElapsed = 0.0f;
	ApplyStepVisibility();
}

void UPGRunGuideWidget::ShowCompletion(int32 RewardReceipts)
{
	CurrentStep = EPGTutorialStep::None;
	bShowingCompletion = true;
	bIsDismissing = false;
	CompletionRemaining = CompletionHoldSeconds;
	if (CompleteRewardText)
	{
		if (RewardReceipts > 0)
		{
			CompleteRewardText->SetText(FText::Format(NSLOCTEXT("PGRunGuide", "TutorialRewardFormat", "영수증 +{0}"), FText::AsNumber(RewardReceipts)));
			CompleteRewardText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			// 이미 완료했던 플레이(도움말로 다시 연 튜토리얼)는 보상이 없으므로 줄을 비운다.
			CompleteRewardText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
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

	SetShown(DragPrompt, CurrentStep == EPGTutorialStep::Balance);
	SetShown(FlickPrompt, CurrentStep == EPGTutorialStep::Jump);
	SetShown(LateralPrompt, CurrentStep == EPGTutorialStep::Lateral);
	SetShown(CargoPrompt, CurrentStep == EPGTutorialStep::Cargo);
	SetShown(WindPrompt, CurrentStep == EPGTutorialStep::Wind);
	SetShown(CompletePrompt, bShowingCompletion);
}
