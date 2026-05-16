// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PGRunHUDWidget.h"

#include "Audio/PGAudioStatics.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "UI/PGCargoDeliveryItemWidget.h"
#include "UI/PGCargoScoreIndicatorWidget.h"

namespace
{
float RunHUDEaseOutCubic(float Alpha)
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	return 1.0f - FMath::Pow(1.0f - ClampedAlpha, 3.0f);
}
}

void UPGRunHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (PauseButton)
	{
		PauseButton->OnClicked.AddDynamic(this, &UPGRunHUDWidget::HandlePauseClicked);
	}
}

void UPGRunHUDWidget::NativeDestruct()
{
	if (PauseButton)
	{
		PauseButton->OnClicked.RemoveDynamic(this, &UPGRunHUDWidget::HandlePauseClicked);
	}

	Super::NativeDestruct();
}

void UPGRunHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bIsEntering && !bIsExiting)
	{
		return;
	}

	AnimationElapsed += InDeltaTime;
	const float Alpha = AnimationDuration > KINDA_SMALL_NUMBER
		? FMath::Clamp(AnimationElapsed / AnimationDuration, 0.0f, 1.0f)
		: 1.0f;
	ApplyAnimationState(RunHUDEaseOutCubic(Alpha), bIsExiting);

	if (Alpha >= 1.0f)
	{
		const bool bFinishedExit = bIsExiting;
		bIsEntering = false;
		bIsExiting = false;
		if (bFinishedExit)
		{
			RemoveFromParent();
		}
	}
}

void UPGRunHUDWidget::SetCargoItemWidgetClass(TSubclassOf<UPGCargoDeliveryItemWidget> InCargoItemWidgetClass)
{
	CargoItemWidgetClass = InCargoItemWidgetClass;
}

void UPGRunHUDWidget::SetCargoScoreIndicatorWidgetClass(TSubclassOf<UPGCargoScoreIndicatorWidget> InCargoScoreIndicatorWidgetClass)
{
	CargoScoreIndicatorWidgetClass = InCargoScoreIndicatorWidgetClass;
}

void UPGRunHUDWidget::SetScore(int32 Score)
{
	if (ScoreText)
	{
		ScoreText->SetText(FText::AsNumber(FMath::Max(0, Score)));
	}
}

void UPGRunHUDWidget::SetDistanceMeters(float DistanceMeters)
{
	if (DistanceText)
	{
		DistanceText->SetText(FText::Format(
			NSLOCTEXT("PGRunHUD", "DistanceMetersFormat", "{0} m"),
			FText::AsNumber(FMath::RoundToInt(FMath::Max(0.0f, DistanceMeters)))));
	}
}

void UPGRunHUDWidget::SetReceipts(int32 Receipts)
{
	if (ReceiptsText)
	{
		ReceiptsText->SetText(FText::AsNumber(FMath::Max(0, Receipts)));
	}
}

void UPGRunHUDWidget::SetPauseButtonEnabled(bool bEnabled) const
{
	if (PauseButton)
	{
		PauseButton->SetIsEnabled(bEnabled);
	}
}

void UPGRunHUDWidget::RefreshCargoList(const TArray<FPGActiveCargoInfo>& ActiveCargoInfos)
{
	if (!CargoListBox || !CargoItemWidgetClass)
	{
		return;
	}

	TSet<int32> SeenInstanceIds;
	for (const FPGActiveCargoInfo& CargoInfo : ActiveCargoInfos)
	{
		if (CargoInfo.InstanceId == INDEX_NONE)
		{
			continue;
		}

		SeenInstanceIds.Add(CargoInfo.InstanceId);
		UPGCargoDeliveryItemWidget* ItemWidget = ActiveItemWidgets.FindRef(CargoInfo.InstanceId);
		if (!ItemWidget)
		{
			ItemWidget = AcquireCargoItemWidget();
			if (!ItemWidget)
			{
				continue;
			}

			ActiveItemWidgets.Add(CargoInfo.InstanceId, ItemWidget);
			CargoListBox->AddChild(ItemWidget);
			ItemWidget->PlayEnterAnimation();
		}

		ItemWidget->ConfigureCargoItem(CargoInfo);
	}

	TArray<int32> RemovedInstanceIds;
	for (const TPair<int32, TObjectPtr<UPGCargoDeliveryItemWidget>>& Pair : ActiveItemWidgets)
	{
		if (!SeenInstanceIds.Contains(Pair.Key))
		{
			RemovedInstanceIds.Add(Pair.Key);
		}
	}

	for (const int32 RemovedInstanceId : RemovedInstanceIds)
	{
		if (UPGCargoDeliveryItemWidget* RemovedWidget = ActiveItemWidgets.FindRef(RemovedInstanceId))
		{
			RemovedWidget->PlayExitAnimation();
		}
		ActiveItemWidgets.Remove(RemovedInstanceId);
	}
}

void UPGRunHUDWidget::ShowCargoScoreIndicator(int32 CargoInstanceId, int32 Score)
{
	if (!CargoScoreIndicatorLayer || !CargoScoreIndicatorWidgetClass || Score <= 0)
	{
		return;
	}

	UPGCargoScoreIndicatorWidget* IndicatorWidget = CreateWidget<UPGCargoScoreIndicatorWidget>(GetOwningPlayer(), CargoScoreIndicatorWidgetClass);
	if (!IndicatorWidget)
	{
		return;
	}

	CargoScoreIndicatorLayer->AddChild(IndicatorWidget);
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(IndicatorWidget->Slot))
	{
		CanvasSlot->SetAutoSize(true);
		CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		CanvasSlot->SetPosition(ResolveCargoIndicatorPosition(CargoInstanceId));
	}

	IndicatorWidget->PlayScoreIndicator(Score);
}

void UPGRunHUDWidget::PlayEnterAnimation()
{
	bIsEntering = true;
	bIsExiting = false;
	AnimationElapsed = 0.0f;
	ApplyAnimationState(0.0f, false);
}

void UPGRunHUDWidget::PlayExitAnimation()
{
	bIsEntering = false;
	bIsExiting = true;
	AnimationElapsed = 0.0f;
	ApplyAnimationState(0.0f, true);
}

void UPGRunHUDWidget::HandlePauseClicked()
{
	UPGAudioStatics::PlayCommonButtonClick(this);
	OnPauseRequested.Broadcast();
}

void UPGRunHUDWidget::ApplyAnimationState(float Alpha, bool bExiting)
{
	const float Opacity = bExiting ? 1.0f - Alpha : Alpha;
	const float YOffset = bExiting ? -60.0f * Alpha : -60.0f * (1.0f - Alpha);
	SetRenderOpacity(Opacity);
	SetRenderTranslation(FVector2D(0.0f, YOffset));
}

UPGCargoDeliveryItemWidget* UPGRunHUDWidget::AcquireCargoItemWidget()
{
	if (!InactiveItemPool.IsEmpty())
	{
		UPGCargoDeliveryItemWidget* ItemWidget = InactiveItemPool.Pop();
		if (ItemWidget)
		{
			return ItemWidget;
		}
	}

	UPGCargoDeliveryItemWidget* NewItemWidget = CreateWidget<UPGCargoDeliveryItemWidget>(GetOwningPlayer(), CargoItemWidgetClass);
	if (NewItemWidget)
	{
		NewItemWidget->OnExitAnimationFinished.AddUObject(this, &UPGRunHUDWidget::HandleCargoItemExitFinished);
	}
	return NewItemWidget;
}

FVector2D UPGRunHUDWidget::ResolveCargoIndicatorPosition(int32 CargoInstanceId) const
{
	if (!CargoScoreIndicatorLayer)
	{
		return FVector2D::ZeroVector;
	}

	const FGeometry LayerGeometry = CargoScoreIndicatorLayer->GetCachedGeometry();
	if (const UPGCargoDeliveryItemWidget* ItemWidget = ActiveItemWidgets.FindRef(CargoInstanceId))
	{
		const FGeometry ItemGeometry = ItemWidget->GetCachedGeometry();
		const FVector2D ItemSize = ItemGeometry.GetLocalSize();
		if (ItemSize.X > KINDA_SMALL_NUMBER && ItemSize.Y > KINDA_SMALL_NUMBER)
		{
			const FVector2D ItemCenterAbsolute = ItemGeometry.LocalToAbsolute(ItemSize * 0.5f);
			return LayerGeometry.AbsoluteToLocal(ItemCenterAbsolute);
		}
	}

	if (CargoListBox)
	{
		const FGeometry ListGeometry = CargoListBox->GetCachedGeometry();
		const FVector2D ListSize = ListGeometry.GetLocalSize();
		if (ListSize.X > KINDA_SMALL_NUMBER && ListSize.Y > KINDA_SMALL_NUMBER)
		{
			const FVector2D ListCenterAbsolute = ListGeometry.LocalToAbsolute(ListSize * 0.5f);
			return LayerGeometry.AbsoluteToLocal(ListCenterAbsolute);
		}
	}

	return FVector2D::ZeroVector;
}

void UPGRunHUDWidget::HandleCargoItemExitFinished(UPGCargoDeliveryItemWidget* ItemWidget)
{
	if (!ItemWidget)
	{
		return;
	}

	ItemWidget->RemoveFromParent();
	InactiveItemPool.Add(ItemWidget);
}
