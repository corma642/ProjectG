// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PGTutorialScreenWidget.h"

#include "Audio/PGAudioStatics.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Widget.h"
#include "Engine/Texture2D.h"
#include "Tutorial/PGTutorialPageSet.h"
#include "UI/PGUIAnimationUtils.h"

void UPGTutorialScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (PrevButton)
	{
		PrevButton->OnClicked.AddDynamic(this, &UPGTutorialScreenWidget::HandlePrevClicked);
	}
	if (NextButton)
	{
		NextButton->OnClicked.AddDynamic(this, &UPGTutorialScreenWidget::HandleNextClicked);
	}
	if (CloseButton)
	{
		CloseButton->OnClicked.AddDynamic(this, &UPGTutorialScreenWidget::HandleCloseClicked);
	}

	if (IncomingPageImage)
	{
		IncomingPageImage->SetVisibility(ESlateVisibility::Collapsed);
		IncomingPageImage->SetRenderOpacity(0.0f);
	}
	UpdateButtonState();
}

void UPGTutorialScreenWidget::NativeDestruct()
{
	if (PrevButton)
	{
		PrevButton->OnClicked.RemoveDynamic(this, &UPGTutorialScreenWidget::HandlePrevClicked);
	}
	if (NextButton)
	{
		NextButton->OnClicked.RemoveDynamic(this, &UPGTutorialScreenWidget::HandleNextClicked);
	}
	if (CloseButton)
	{
		CloseButton->OnClicked.RemoveDynamic(this, &UPGTutorialScreenWidget::HandleCloseClicked);
	}

	Super::NativeDestruct();
}

void UPGTutorialScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bIsTransitioning)
	{
		return;
	}

	TransitionElapsed += InDeltaTime;
	const float Alpha = TransitionDuration > KINDA_SMALL_NUMBER
		? FMath::Clamp(TransitionElapsed / TransitionDuration, 0.0f, 1.0f)
		: 1.0f;
	ApplyTransitionState(PGUIAnimation::EaseOutCubic(Alpha));

	if (Alpha >= 1.0f)
	{
		FinishPageTransition();
	}
}

void UPGTutorialScreenWidget::InitializeTutorial(UPGTutorialPageSet* InPageSet)
{
	PageSet = InPageSet;
	CurrentPageIndex = 0;
	TargetPageIndex = INDEX_NONE;
	bIsTransitioning = false;

	ApplyPageToImage(CurrentPageImage, CurrentPageIndex);
	if (CurrentPageImage)
	{
		CurrentPageImage->SetRenderTranslation(FVector2D::ZeroVector);
		CurrentPageImage->SetRenderScale(FVector2D::UnitVector);
		CurrentPageImage->SetRenderOpacity(1.0f);
	}
	if (IncomingPageImage)
	{
		IncomingPageImage->SetVisibility(ESlateVisibility::Collapsed);
		IncomingPageImage->SetRenderOpacity(0.0f);
	}

	UpdateButtonState();
}

void UPGTutorialScreenWidget::HandlePrevClicked()
{
	UPGAudioStatics::PlayCommonButtonClick(this);
	StartPageTransition(CurrentPageIndex - 1, -1);
}

void UPGTutorialScreenWidget::HandleNextClicked()
{
	UPGAudioStatics::PlayCommonButtonClick(this);
	StartPageTransition(CurrentPageIndex + 1, 1);
}

void UPGTutorialScreenWidget::HandleCloseClicked()
{
	UPGAudioStatics::PlayCommonButtonClick(this);
	OnTutorialClosed.Broadcast();
	RemoveFromParent();
}

void UPGTutorialScreenWidget::StartPageTransition(int32 NewPageIndex, int32 Direction)
{
	const int32 PageCount = PageSet ? PageSet->Pages.Num() : 0;
	if (bIsTransitioning || NewPageIndex < 0 || NewPageIndex >= PageCount || !IncomingPageImage)
	{
		return;
	}

	TargetPageIndex = NewPageIndex;
	TransitionDirection = Direction >= 0 ? 1 : -1;
	TransitionElapsed = 0.0f;
	bIsTransitioning = true;

	ApplyPageToImage(IncomingPageImage, TargetPageIndex);
	IncomingPageImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	ApplyTransitionState(0.0f);
	SetNavigationButtonsEnabled(false);
}

void UPGTutorialScreenWidget::ApplyPageToImage(UImage* Image, int32 PageIndex) const
{
	if (!Image)
	{
		return;
	}

	UTexture2D* PageTexture = nullptr;
	if (PageSet && PageSet->Pages.IsValidIndex(PageIndex))
	{
		PageTexture = PageSet->Pages[PageIndex].Get();
	}

	Image->SetBrushFromTexture(PageTexture, true);
	Image->SetVisibility(PageTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UPGTutorialScreenWidget::ApplyTransitionState(float Alpha)
{
	const float TravelDistance = GetPageTravelDistance();
	const float Direction = static_cast<float>(TransitionDirection);

	if (CurrentPageImage)
	{
		CurrentPageImage->SetRenderTranslation(FVector2D(-Direction * TravelDistance * Alpha, 0.0f));
		CurrentPageImage->SetRenderOpacity(1.0f - Alpha);
		const float CurrentScale = FMath::Lerp(1.0f, 0.94f, Alpha);
		CurrentPageImage->SetRenderScale(FVector2D(CurrentScale, CurrentScale));
	}

	if (IncomingPageImage)
	{
		IncomingPageImage->SetRenderTranslation(FVector2D(Direction * TravelDistance * (1.0f - Alpha), 0.0f));
		IncomingPageImage->SetRenderOpacity(Alpha);
		const float IncomingScale = FMath::Lerp(0.94f, 1.0f, Alpha);
		IncomingPageImage->SetRenderScale(FVector2D(IncomingScale, IncomingScale));
	}
}

void UPGTutorialScreenWidget::FinishPageTransition()
{
	CurrentPageIndex = TargetPageIndex;
	TargetPageIndex = INDEX_NONE;
	bIsTransitioning = false;

	ApplyPageToImage(CurrentPageImage, CurrentPageIndex);
	if (CurrentPageImage)
	{
		CurrentPageImage->SetRenderTranslation(FVector2D::ZeroVector);
		CurrentPageImage->SetRenderScale(FVector2D::UnitVector);
		CurrentPageImage->SetRenderOpacity(1.0f);
	}
	if (IncomingPageImage)
	{
		IncomingPageImage->SetVisibility(ESlateVisibility::Collapsed);
		IncomingPageImage->SetRenderTranslation(FVector2D::ZeroVector);
		IncomingPageImage->SetRenderScale(FVector2D::UnitVector);
		IncomingPageImage->SetRenderOpacity(0.0f);
	}

	UpdateButtonState();
}

void UPGTutorialScreenWidget::UpdateButtonState() const
{
	const int32 PageCount = PageSet ? PageSet->Pages.Num() : 0;
	const bool bHasPages = PageCount > 0;

	if (PrevButton)
	{
		ApplyButtonEnabledState(PrevButton, !bIsTransitioning && bHasPages && CurrentPageIndex > 0);
	}
	if (NextButton)
	{
		ApplyButtonEnabledState(NextButton, !bIsTransitioning && bHasPages && CurrentPageIndex < PageCount - 1);
	}
	if (CloseButton)
	{
		ApplyButtonEnabledState(CloseButton, !bIsTransitioning);
	}
}

void UPGTutorialScreenWidget::SetNavigationButtonsEnabled(bool bEnabled) const
{
	if (PrevButton)
	{
		ApplyButtonEnabledState(PrevButton, bEnabled);
	}
	if (NextButton)
	{
		ApplyButtonEnabledState(NextButton, bEnabled);
	}
	if (CloseButton)
	{
		ApplyButtonEnabledState(CloseButton, bEnabled);
	}
}

void UPGTutorialScreenWidget::ApplyButtonEnabledState(UButton* Button, bool bEnabled) const
{
	if (!Button)
	{
		return;
	}

	Button->SetIsEnabled(bEnabled);
	Button->SetRenderOpacity(bEnabled ? 1.0f : 0.35f);
}

float UPGTutorialScreenWidget::GetPageTravelDistance() const
{
	if (RootPanel)
	{
		const float Width = RootPanel->GetCachedGeometry().GetLocalSize().X;
		if (Width > KINDA_SMALL_NUMBER)
		{
			return Width * 1.12f;
		}
	}

	return 900.0f;
}

void UPGTutorialScreenWidget::RequestClose()
{
	HandleCloseClicked();
}
