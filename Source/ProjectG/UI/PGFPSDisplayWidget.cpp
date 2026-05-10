// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PGFPSDisplayWidget.h"

#include "Components/TextBlock.h"

void UPGFPSDisplayWidget::NativeConstruct()
{
	Super::NativeConstruct();

	AccumulatedTime = 0.0f;
	AccumulatedFrames = 0;
	UpdateFPSText(0.0f);
}

void UPGFPSDisplayWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	AccumulatedTime += FMath::Max(0.0f, InDeltaTime);
	++AccumulatedFrames;

	if (AccumulatedTime < UpdateInterval)
	{
		return;
	}

	const float AverageFPS = AccumulatedTime > KINDA_SMALL_NUMBER
		? static_cast<float>(AccumulatedFrames) / AccumulatedTime
		: 0.0f;

	UpdateFPSText(AverageFPS);
	AccumulatedTime = 0.0f;
	AccumulatedFrames = 0;
}

void UPGFPSDisplayWidget::UpdateFPSText(float FPS) const
{
	if (!FPSText)
	{
		return;
	}

	FPSText->SetText(FText::Format(
		NSLOCTEXT("PGFPSDisplay", "FPSFormat", "{0} FPS"),
		FText::AsNumber(FMath::RoundToInt(FPS))));
}
