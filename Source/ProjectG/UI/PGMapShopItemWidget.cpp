// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PGMapShopItemWidget.h"

#include "Audio/PGAudioStatics.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Widget.h"
#include "Shop/PGMapDefinition.h"
#include "UI/PGUIAnimationUtils.h"

void UPGMapShopItemWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (SelectButton)
	{
		SelectButton->OnClicked.AddDynamic(this, &UPGMapShopItemWidget::HandleSelectButtonClicked);
	}
}

void UPGMapShopItemWidget::NativeDestruct()
{
	if (SelectButton)
	{
		SelectButton->OnClicked.RemoveDynamic(this, &UPGMapShopItemWidget::HandleSelectButtonClicked);
	}

	Super::NativeDestruct();
}

void UPGMapShopItemWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bStateAnimationActive)
	{
		return;
	}

	StateAnimationElapsed += InDeltaTime;
	const float Alpha = FMath::Clamp(StateAnimationElapsed / FMath::Max(KINDA_SMALL_NUMBER, StateAnimationDuration), 0.0f, 1.0f);
	ApplyStateAnimation(Alpha);

	if (Alpha >= 1.0f)
	{
		bStateAnimationActive = false;
		ApplyFinalState();
	}
}

void UPGMapShopItemWidget::SetMapDefinition(const UPGMapDefinition* InMapDefinition, bool bOwned, bool bEquipped, bool bSelected)
{
	MapId = InMapDefinition ? InMapDefinition->MapId : NAME_None;

	if (ThumbnailImage)
	{
		ThumbnailImage->SetBrushFromTexture(InMapDefinition ? InMapDefinition->Thumbnail.Get() : nullptr);
	}

	StartStateAnimation(bOwned, bEquipped, bSelected);
}

void UPGMapShopItemWidget::SetSelectionEnabled(bool bEnabled) const
{
	if (SelectButton)
	{
		SelectButton->SetIsEnabled(bEnabled);
	}
}

void UPGMapShopItemWidget::HandleSelectButtonClicked()
{
	UPGAudioStatics::PlayCommonButtonClick(this);
	if (!MapId.IsNone())
	{
		OnMapSelected.Broadcast(MapId);
	}
}

void UPGMapShopItemWidget::StartStateAnimation(bool bOwned, bool bEquipped, bool bSelected)
{
	const bool bStateChanged = !bHasState || bLastOwned != bOwned || bLastEquipped != bEquipped || bLastSelected != bSelected;

	StartRootScale = RootPanel ? RootPanel->GetRenderTransform().Scale.X : (bLastSelected ? 1.05f : 1.0f);
	StartRootOpacity = RootPanel ? RootPanel->GetRenderOpacity() : (bLastSelected ? 1.0f : 0.86f);
	StartThumbnailScale = ThumbnailImage ? ThumbnailImage->GetRenderTransform().Scale.X : (bLastSelected ? 1.04f : 1.0f);
	StartLockOpacity = LockImage ? LockImage->GetRenderOpacity() : (bLastOwned ? 0.0f : 1.0f);
	StartMarkerOpacity = CurrentMarker ? CurrentMarker->GetRenderOpacity() : (bLastEquipped ? 1.0f : 0.0f);
	StartMarkerTranslation = CurrentMarker ? CurrentMarker->GetRenderTransform().Translation : FVector2D::ZeroVector;

	if (LockImage && (!bOwned || !bHasState || !bLastOwned))
	{
		LockImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (CurrentMarker && (bEquipped || !bHasState || bLastEquipped))
	{
		CurrentMarker->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	bLastOwned = bOwned;
	bLastEquipped = bEquipped;
	bLastSelected = bSelected;
	bHasState = true;

	if (!bStateChanged)
	{
		ApplyFinalState();
		return;
	}

	StateAnimationElapsed = 0.0f;
	bStateAnimationActive = true;
	ApplyStateAnimation(0.0f);
}

void UPGMapShopItemWidget::ApplyStateAnimation(float Alpha)
{
	const float EasedAlpha = PGUIAnimation::EaseOutCubic(Alpha);
	const float TargetRootScale = bLastSelected ? 1.05f : 1.0f;
	const float TargetRootOpacity = bLastSelected ? 1.0f : 0.86f;
	const float TargetThumbnailScale = bLastSelected ? 1.04f : 1.0f;
	const float TargetLockOpacity = bLastOwned ? 0.0f : 1.0f;
	const float TargetMarkerOpacity = bLastEquipped ? 1.0f : 0.0f;
	const FVector2D MarkerIntroTranslation = bLastEquipped ? FVector2D(0.0f, -18.0f) : FVector2D::ZeroVector;

	if (RootPanel)
	{
		const float Scale = FMath::Lerp(StartRootScale, TargetRootScale, EasedAlpha);
		RootPanel->SetRenderScale(FVector2D(Scale, Scale));
		RootPanel->SetRenderOpacity(FMath::Lerp(StartRootOpacity, TargetRootOpacity, EasedAlpha));
	}
	if (ThumbnailImage)
	{
		const float Scale = FMath::Lerp(StartThumbnailScale, TargetThumbnailScale, EasedAlpha);
		ThumbnailImage->SetRenderScale(FVector2D(Scale, Scale));
	}
	if (LockImage)
	{
		LockImage->SetVisibility(bLastOwned ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		LockImage->SetRenderOpacity(FMath::Lerp(StartLockOpacity, TargetLockOpacity, EasedAlpha));
	}
	if (CurrentMarker)
	{
		CurrentMarker->SetVisibility(bLastEquipped ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		const FVector2D StartTranslation = bLastEquipped && StartMarkerOpacity <= KINDA_SMALL_NUMBER ? MarkerIntroTranslation : StartMarkerTranslation;
		CurrentMarker->SetRenderOpacity(FMath::Lerp(StartMarkerOpacity, TargetMarkerOpacity, EasedAlpha));
		CurrentMarker->SetRenderTranslation(FMath::Lerp(StartTranslation, FVector2D::ZeroVector, EasedAlpha));
	}
}

void UPGMapShopItemWidget::ApplyFinalState()
{
	if (RootPanel)
	{
		const float TargetScale = bLastSelected ? 1.05f : 1.0f;
		RootPanel->SetRenderScale(FVector2D(TargetScale, TargetScale));
		RootPanel->SetRenderOpacity(bLastSelected ? 1.0f : 0.86f);
	}
	if (ThumbnailImage)
	{
		const float TargetScale = bLastSelected ? 1.04f : 1.0f;
		ThumbnailImage->SetRenderScale(FVector2D(TargetScale, TargetScale));
	}
	if (LockImage)
	{
		LockImage->SetVisibility(bLastOwned ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		LockImage->SetRenderOpacity(bLastOwned ? 0.0f : 1.0f);
	}
	if (CurrentMarker)
	{
		CurrentMarker->SetVisibility(bLastEquipped ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		CurrentMarker->SetRenderOpacity(bLastEquipped ? 1.0f : 0.0f);
		CurrentMarker->SetRenderTranslation(FVector2D::ZeroVector);
	}
}
