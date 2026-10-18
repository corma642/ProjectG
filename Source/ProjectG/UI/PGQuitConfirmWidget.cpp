// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PGQuitConfirmWidget.h"

#include "Audio/PGAudioStatics.h"
#include "Components/BackgroundBlur.h"
#include "Components/Button.h"

namespace
{
	// Pause 모달과 같은 배경 블러 세기(px).
	constexpr float QuitBackdropBlurStrength = 4.0f;
}

void UPGQuitConfirmWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ConfirmButton)
	{
		ConfirmButton->OnClicked.AddDynamic(this, &UPGQuitConfirmWidget::HandleConfirmClicked);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked.AddDynamic(this, &UPGQuitConfirmWidget::HandleCancelClicked);
	}
	if (BackdropBlur)
	{
		BackdropBlur->SetBlurStrength(QuitBackdropBlurStrength);
	}
}

void UPGQuitConfirmWidget::NativeDestruct()
{
	if (ConfirmButton)
	{
		ConfirmButton->OnClicked.RemoveDynamic(this, &UPGQuitConfirmWidget::HandleConfirmClicked);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked.RemoveDynamic(this, &UPGQuitConfirmWidget::HandleCancelClicked);
	}

	Super::NativeDestruct();
}

void UPGQuitConfirmWidget::HandleConfirmClicked()
{
	UPGAudioStatics::PlayCommonButtonClick(this);
	OnQuitConfirmed.Broadcast();
}

void UPGQuitConfirmWidget::HandleCancelClicked()
{
	UPGAudioStatics::PlayCommonButtonClick(this);
	OnQuitCancelled.Broadcast();
}
