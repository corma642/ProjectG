// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PGRunCountdownWidget.h"

#include "Components/TextBlock.h"
#include "Components/Widget.h"

namespace
{
float EaseOutBack(float Alpha)
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	constexpr float Overshoot = 1.70158f;
	const float Shifted = ClampedAlpha - 1.0f;
	return 1.0f + (Overshoot + 1.0f) * Shifted * Shifted * Shifted + Overshoot * Shifted * Shifted;
}

float EaseOutCubic(float Alpha)
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	return 1.0f - FMath::Pow(1.0f - ClampedAlpha, 3.0f);
}

float EaseInCubicCountdown(float Alpha)
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	return ClampedAlpha * ClampedAlpha * ClampedAlpha;
}
}

void UPGRunCountdownWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (RootPanel)
	{
		RootPanel->SetRenderOpacity(0.0f);
		RootPanel->SetRenderScale(FVector2D(1.0f));
	}
}

void UPGRunCountdownWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bIsCountdownActive)
	{
		return;
	}

	CountdownElapsed += InDeltaTime;
	const float StepDuration = CountdownDuration > KINDA_SMALL_NUMBER ? CountdownDuration / 4.0f : 0.0f;
	const int32 StepIndex = StepDuration > KINDA_SMALL_NUMBER
		? FMath::Clamp(FMath::FloorToInt(CountdownElapsed / StepDuration), 0, 3)
		: 3;
	const float StepElapsed = StepDuration > KINDA_SMALL_NUMBER ? CountdownElapsed - StepIndex * StepDuration : StepDuration;
	const float StepAlpha = StepDuration > KINDA_SMALL_NUMBER ? FMath::Clamp(StepElapsed / StepDuration, 0.0f, 1.0f) : 1.0f;

	if (StepIndex != CurrentStepIndex)
	{
		UpdateCountdownText(StepIndex);
		CurrentStepIndex = StepIndex;
	}

	ApplyStepAnimation(StepAlpha);

	if (CountdownElapsed >= CountdownDuration)
	{
		bIsCountdownActive = false;
		RemoveFromParent();
	}
}

void UPGRunCountdownWidget::StartCountdown(float Duration)
{
	CountdownDuration = FMath::Max(0.0f, Duration);
	CountdownElapsed = 0.0f;
	CurrentStepIndex = INDEX_NONE;
	bIsCountdownActive = true;
	UpdateCountdownText(0);
	ApplyStepAnimation(0.0f);

	if (CountdownDuration <= KINDA_SMALL_NUMBER)
	{
		bIsCountdownActive = false;
		RemoveFromParent();
	}
}

void UPGRunCountdownWidget::UpdateCountdownText(int32 StepIndex)
{
	if (!CountdownText)
	{
		return;
	}

	static const TCHAR* CountdownLabels[] = { TEXT("3"), TEXT("2"), TEXT("1"), TEXT("GO!") };
	CountdownText->SetText(FText::FromString(CountdownLabels[FMath::Clamp(StepIndex, 0, 3)]));

	if (RootPanel)
	{
		RootPanel->SetRenderOpacity(0.0f);
		RootPanel->SetRenderScale(FVector2D(2.4f));
		RootPanel->SetRenderTranslation(FVector2D::ZeroVector);
	}
}

void UPGRunCountdownWidget::ApplyStepAnimation(float StepAlpha)
{
	if (!RootPanel)
	{
		return;
	}

	const bool bIsGoStep = CurrentStepIndex == 3;
	const float ImpactEndAlpha = 0.22f;
	const float FadeOutStartAlpha = bIsGoStep ? 0.82f : 0.68f;

	float Scale = 1.0f;
	if (StepAlpha < ImpactEndAlpha)
	{
		const float ImpactAlpha = StepAlpha / ImpactEndAlpha;
		Scale = FMath::Lerp(2.4f, 1.15f, EaseOutBack(ImpactAlpha));
	}
	else if (StepAlpha < FadeOutStartAlpha)
	{
		const float HoldAlpha = (StepAlpha - ImpactEndAlpha) / FMath::Max(KINDA_SMALL_NUMBER, FadeOutStartAlpha - ImpactEndAlpha);
		Scale = FMath::Lerp(1.15f, 1.0f, EaseOutCubic(HoldAlpha));
	}
	else
	{
		const float ExitAlpha = (StepAlpha - FadeOutStartAlpha) / FMath::Max(KINDA_SMALL_NUMBER, 1.0f - FadeOutStartAlpha);
		Scale = FMath::Lerp(1.0f, 0.65f, EaseInCubicCountdown(ExitAlpha));
	}

	const float FadeInAlpha = FMath::Clamp(StepAlpha / 0.12f, 0.0f, 1.0f);
	const float FadeOutAlpha = 1.0f - FMath::Clamp((StepAlpha - FadeOutStartAlpha) / FMath::Max(KINDA_SMALL_NUMBER, 1.0f - FadeOutStartAlpha), 0.0f, 1.0f);
	const float Opacity = FMath::Min(FadeInAlpha, FadeOutAlpha);

	RootPanel->SetRenderOpacity(Opacity);
	RootPanel->SetRenderScale(FVector2D(Scale));
}
