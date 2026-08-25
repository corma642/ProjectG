// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PGCargoScoreIndicatorWidget.h"

#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "UI/PGUIAnimationUtils.h"

void UPGCargoScoreIndicatorWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetRenderOpacity(0.0f);
}

void UPGCargoScoreIndicatorWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bIsAnimating)
	{
		return;
	}

	AnimationElapsed += InDeltaTime;
	const float RawAlpha = AnimationDuration > KINDA_SMALL_NUMBER
		? FMath::Clamp(AnimationElapsed / AnimationDuration, 0.0f, 1.0f)
		: 1.0f;

	ApplyAnimationState(RawAlpha);

	if (RawAlpha >= 1.0f)
	{
		bIsAnimating = false;
		RemoveFromParent();
	}
}

void UPGCargoScoreIndicatorWidget::PlayScoreIndicator(int32 Score)
{
	if (ScoreText)
	{
		ScoreText->SetText(FText::Format(NSLOCTEXT("PGCargoScoreIndicator", "ScoreFormat", "+{0}"), FText::AsNumber(FMath::Max(0, Score))));
	}

	bIsAnimating = true;
	AnimationElapsed = 0.0f;
	ApplyAnimationState(0.0f);
}

void UPGCargoScoreIndicatorWidget::ApplyAnimationState(float Alpha)
{
	const float MoveAlpha = PGUIAnimation::EaseOutCubic(Alpha);
	const float FadeAlpha = FMath::Clamp((Alpha - 0.45f) / 0.55f, 0.0f, 1.0f);
	const float Opacity = 1.0f - FadeAlpha;
	const float YOffset = FMath::Lerp(0.0f, -72.0f, MoveAlpha);
	const float Scale = FMath::Lerp(1.18f, 1.0f, MoveAlpha);

	SetRenderOpacity(Opacity);
	SetRenderTranslation(FVector2D(0.0f, YOffset));
	SetRenderScale(FVector2D(Scale, Scale));
}
