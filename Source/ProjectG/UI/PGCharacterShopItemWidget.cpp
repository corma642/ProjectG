// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PGCharacterShopItemWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Widget.h"
#include "Shop/PGCharacterSkinDefinition.h"

namespace
{
float CharacterItemEaseOutCubic(float Alpha)
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	const float InverseAlpha = 1.0f - ClampedAlpha;
	return 1.0f - InverseAlpha * InverseAlpha * InverseAlpha;
}
}

void UPGCharacterShopItemWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (SelectButton)
	{
		SelectButton->OnClicked.AddDynamic(this, &UPGCharacterShopItemWidget::HandleSelectButtonClicked);
	}
}

void UPGCharacterShopItemWidget::NativeDestruct()
{
	if (SelectButton)
	{
		SelectButton->OnClicked.RemoveDynamic(this, &UPGCharacterShopItemWidget::HandleSelectButtonClicked);
	}

	Super::NativeDestruct();
}

void UPGCharacterShopItemWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
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

void UPGCharacterShopItemWidget::SetCharacterSkin(const UPGCharacterSkinDefinition* InSkinDefinition, bool bOwned, bool bEquipped, bool bSelected)
{
	SkinId = InSkinDefinition ? InSkinDefinition->SkinId : NAME_None;

	if (ThumbnailImage)
	{
		ThumbnailImage->SetBrushFromTexture(InSkinDefinition ? InSkinDefinition->Thumbnail.Get() : nullptr);
	}

	StartStateAnimation(bOwned, bEquipped, bSelected);
}

void UPGCharacterShopItemWidget::HandleSelectButtonClicked()
{
	BroadcastSelected();
}

void UPGCharacterShopItemWidget::StartStateAnimation(bool bOwned, bool bEquipped, bool bSelected)
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

void UPGCharacterShopItemWidget::ApplyStateAnimation(float Alpha)
{
	const float EasedAlpha = CharacterItemEaseOutCubic(Alpha);
	const float TargetRootScale = bLastSelected ? 1.05f : 1.0f;
	const float TargetRootOpacity = bLastSelected ? 1.0f : 0.86f;
	const float TargetThumbnailScale = bLastSelected ? 1.04f : 1.0f;
	const float TargetLockOpacity = bLastOwned ? 0.0f : 1.0f;
	const float TargetMarkerOpacity = bLastEquipped ? 1.0f : 0.0f;
	const FVector2D TargetMarkerTranslation = FVector2D::ZeroVector;
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
		CurrentMarker->SetRenderTranslation(FMath::Lerp(StartTranslation, TargetMarkerTranslation, EasedAlpha));
	}
}

void UPGCharacterShopItemWidget::ApplyFinalState()
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

void UPGCharacterShopItemWidget::BroadcastSelected()
{
	if (!SkinId.IsNone())
	{
		OnCharacterSelected.Broadcast(SkinId);
	}
}
