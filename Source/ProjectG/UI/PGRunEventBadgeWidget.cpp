// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PGRunEventBadgeWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Widget.h"

namespace
{
	// 발동 뒤 배지가 줄어드는 칩 배율과 축소에 걸리는 시간(초).
	constexpr float ChipScale = 0.62f;
	constexpr float ChipShrinkSeconds = 0.25f;
	// 진행 링 선 두께(px)와 아이콘 원 바깥 여백(px), 호를 이루는 선분 수.
	constexpr float RingThickness = 8.0f;
	constexpr float RingGap = 10.0f;
	constexpr int32 RingSegments = 48;
}

void UPGRunEventBadgeWidget::ConfigureRunEventFeedback_Implementation(EPGRunEventType EventType, EPGRunEventState InEventState, float TimeUntilTrigger, float Duration, float Direction)
{
	EventState = InEventState;
	ElapsedSeconds = 0.0f;
	TotalSeconds = FMath::Max(0.0f, EventState == EPGRunEventState::Preview ? TimeUntilTrigger : Duration);

	const int32 DirectionSign = FMath::IsNearlyZero(Direction) ? 0 : (Direction < 0.0f ? -1 : 1);
	const TPair<UWidget*, int32> DirectionWidgets[] = { { DirectionNegative, -1 }, { DirectionZero, 0 }, { DirectionPositive, 1 } };
	for (const TPair<UWidget*, int32>& Entry : DirectionWidgets)
	{
		if (Entry.Key)
		{
			Entry.Key->SetVisibility(Entry.Value == DirectionSign ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}

	const bool bPreview = EventState == EPGRunEventState::Preview;
	NameText->SetVisibility(bPreview ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	BadgePanel->SetRenderTransformPivot(FVector2D(0.5f, 0.0f));
	BadgePanel->SetRenderScale(FVector2D(1.0f, 1.0f));
}

void UPGRunEventBadgeWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	ElapsedSeconds += InDeltaTime;

	if (EventState == EPGRunEventState::Triggered && ElapsedSeconds <= ChipShrinkSeconds + InDeltaTime)
	{
		// 전조 배지가 있던 자리에서 그대로 줄어들어 칩으로 이어진다.
		const float Scale = FMath::Lerp(1.0f, ChipScale, FMath::Clamp(ElapsedSeconds / ChipShrinkSeconds, 0.0f, 1.0f));
		BadgePanel->SetRenderScale(FVector2D(Scale, Scale));
	}
}

int32 UPGRunEventBadgeWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (!RingAnchor || EventState == EPGRunEventState::Inactive || TotalSeconds <= KINDA_SMALL_NUMBER)
	{
		return LayerId;
	}

	// 전조는 발동 시점에 한 바퀴가 차고, 발동 뒤에는 남은 시간만큼만 남는다.
	const float TimeFraction = FMath::Clamp(ElapsedSeconds / TotalSeconds, 0.0f, 1.0f);
	const float Progress = EventState == EPGRunEventState::Preview ? TimeFraction : 1.0f - TimeFraction;
	if (Progress <= KINDA_SMALL_NUMBER)
	{
		return LayerId;
	}

	// 지난 프레임의 앵커 지오메트리를 절대 좌표로 읽어 칩 축소 배율이 걸린 뒤에도 원 둘레를 따라간다.
	const FGeometry& AnchorGeometry = RingAnchor->GetCachedGeometry();
	const FVector2D CenterAbsolute = AnchorGeometry.GetAbsolutePositionAtCoordinates(FVector2D(0.5f, 0.5f));
	const FVector2D TopAbsolute = AnchorGeometry.GetAbsolutePositionAtCoordinates(FVector2D(0.5f, 0.0f));
	const float RadiusLocal = (AllottedGeometry.AbsoluteToLocal(CenterAbsolute) - AllottedGeometry.AbsoluteToLocal(TopAbsolute)).Size() + RingGap;
	if (RadiusLocal < RingGap + 1.0f)
	{
		return LayerId;
	}

	const FVector2D CenterLocal = AllottedGeometry.AbsoluteToLocal(CenterAbsolute);
	const int32 Segments = FMath::Max(2, FMath::CeilToInt(RingSegments * Progress));
	RingPoints.Reset(Segments + 1);
	for (int32 Index = 0; Index <= Segments; ++Index)
	{
		// 12시 방향에서 시계 방향으로 돈다.
		const float Angle = -HALF_PI + TWO_PI * Progress * (static_cast<float>(Index) / Segments);
		RingPoints.Add(CenterLocal + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * RadiusLocal);
	}

	FPaintContext Context(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId + 1, InWidgetStyle, bParentEnabled);
	UWidgetBlueprintLibrary::DrawLines(Context, RingPoints, FLinearColor::White, true, RingThickness);
	return Context.MaxLayer;
}
