// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PGRunHUDWidget.h"

#include "Audio/PGAudioStatics.h"
#include "Cargo/PGCargoStackComponent.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "UI/PGCargoDeliveryItemWidget.h"
#include "UI/PGCargoScoreIndicatorWidget.h"
#include "UI/PGUIAnimationUtils.h"

void UPGRunHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (PauseButton)
	{
		PauseButton->OnClicked.AddDynamic(this, &UPGRunHUDWidget::HandlePauseClicked);
	}

	// 디자이너가 BP에서 보이게 둬도 경고는 런타임 상태로만 켠다. Slate 위젯이 다시 만들어져도 현재 상태를 그대로 반영한다.
	if (CargoDangerImage)
	{
		CargoDangerImage->SetVisibility(bIsCargoDangerActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
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

	if (bIsCargoDangerActive)
	{
		UpdateCargoDangerPulse(InDeltaTime);
	}

	if (!bIsEntering && !bIsExiting)
	{
		return;
	}

	AnimationElapsed += InDeltaTime;
	const float Alpha = AnimationDuration > KINDA_SMALL_NUMBER
		? FMath::Clamp(AnimationElapsed / AnimationDuration, 0.0f, 1.0f)
		: 1.0f;
	ApplyAnimationState(PGUIAnimation::EaseOutCubic(Alpha), bIsExiting);

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

	// 활성 적재물은 몇 개 수준이라 선형 탐색으로 충분하다. 짧은 주기로 호출되므로 임시 Set/Array 할당을 두지 않는다.
	for (auto ItemIt = ActiveItemWidgets.CreateIterator(); ItemIt; ++ItemIt)
	{
		const int32 InstanceId = ItemIt.Key();
		const bool bStillActive = ActiveCargoInfos.ContainsByPredicate([InstanceId](const FPGActiveCargoInfo& CargoInfo)
		{
			return CargoInfo.InstanceId == InstanceId;
		});
		if (bStillActive)
		{
			continue;
		}

		if (UPGCargoDeliveryItemWidget* RemovedWidget = ItemIt.Value())
		{
			RemovedWidget->PlayExitAnimation();
		}
		ItemIt.RemoveCurrent();
	}

	for (const FPGActiveCargoInfo& CargoInfo : ActiveCargoInfos)
	{
		if (CargoInfo.InstanceId == INDEX_NONE)
		{
			continue;
		}

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

void UPGRunHUDWidget::SetCargoDangerWarningActive(bool bActive)
{
	if (bIsCargoDangerActive == bActive)
	{
		return;
	}

	bIsCargoDangerActive = bActive;
	CargoDangerPulseElapsed = 0.0f;
	if (CargoDangerImage)
	{
		CargoDangerImage->SetVisibility(bActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
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

void UPGRunHUDWidget::UpdateCargoDangerPulse(float InDeltaTime)
{
	if (!CargoDangerImage)
	{
		return;
	}

	// 초당 2.5회 사인 펄스. 완전히 꺼지지 않도록 하한을 둬 경고가 끊겨 보이지 않게 한다.
	CargoDangerPulseElapsed += InDeltaTime;
	const float Pulse = 0.5f + 0.5f * FMath::Sin(CargoDangerPulseElapsed * 2.0f * PI * 2.5f);
	CargoDangerImage->SetRenderOpacity(FMath::Lerp(0.25f, 1.0f, Pulse));
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
