// Copyright Epic Games, Inc. All Rights Reserved.

#include "Debug/PGFPSDisplayComponent.h"

#include "Blueprint/UserWidget.h"
#include "Loading/PGRuntimePreloadSubsystem.h"
#include "Player/PGPlayerController.h"
#include "Settings/PGGameSettings.h"
#include "Settings/PGGameUserSettings.h"
#include "UI/PGFPSDisplayWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGFPSDisplay, Log, All);

UPGFPSDisplayComponent::UPGFPSDisplayComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPGFPSDisplayComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerPlayerController = Cast<APGPlayerController>(GetOwner());

	if (UPGGameUserSettings* UserSettings = Cast<UPGGameUserSettings>(UGameUserSettings::GetGameUserSettings()))
	{
		UserSettings->OnShowFPSDisplayChanged.AddUObject(this, &UPGFPSDisplayComponent::HandleShowFPSDisplayChanged);
	}

	if (UPGRuntimePreloadSubsystem* PreloadSubsystem = GetRuntimePreloadSubsystem())
	{
		PreloadSubsystem->OnPreloadCompleted.AddUniqueDynamic(this, &UPGFPSDisplayComponent::HandleRuntimePreloadCompleted);
	}

	RefreshDisplay();
}

void UPGFPSDisplayComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UPGGameUserSettings* UserSettings = Cast<UPGGameUserSettings>(UGameUserSettings::GetGameUserSettings()))
	{
		UserSettings->OnShowFPSDisplayChanged.RemoveAll(this);
	}

	if (UPGRuntimePreloadSubsystem* PreloadSubsystem = GetRuntimePreloadSubsystem())
	{
		PreloadSubsystem->OnPreloadCompleted.RemoveDynamic(this, &UPGFPSDisplayComponent::HandleRuntimePreloadCompleted);
	}

	CleanupFPSDisplay();
	OwnerPlayerController = nullptr;

	Super::EndPlay(EndPlayReason);
}

void UPGFPSDisplayComponent::HandleRuntimePreloadCompleted(EPGRuntimePreloadGroup Group)
{
	if (Group == EPGRuntimePreloadGroup::Home)
	{
		RefreshDisplay();
	}
}

void UPGFPSDisplayComponent::HandleShowFPSDisplayChanged(bool bShow)
{
	RefreshDisplay();
}

void UPGFPSDisplayComponent::RefreshDisplay()
{
	const UPGGameUserSettings* UserSettings = Cast<UPGGameUserSettings>(UGameUserSettings::GetGameUserSettings());
	if (!UserSettings || !UserSettings->GetShowFPSDisplay())
	{
		CleanupFPSDisplay();
		return;
	}

	ShowFPSDisplay();
}

void UPGFPSDisplayComponent::ShowFPSDisplay()
{
	if (!OwnerPlayerController || FPSDisplayWidget)
	{
		return;
	}

	if (const UPGRuntimePreloadSubsystem* PreloadSubsystem = GetRuntimePreloadSubsystem())
	{
		if (!PreloadSubsystem->IsGroupReady(EPGRuntimePreloadGroup::Home))
		{
			return;
		}
	}

	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	UClass* WidgetClass = Settings ? Settings->DebugUI.FPSDisplayWidgetClass.Get() : nullptr;
	if (!WidgetClass || !WidgetClass->IsChildOf(UPGFPSDisplayWidget::StaticClass()))
	{
		UE_LOG(LogPGFPSDisplay, Warning, TEXT("FPSDisplayWidgetClass must be assigned and inherit UPGFPSDisplayWidget."));
		return;
	}

	FPSDisplayWidget = CreateWidget<UPGFPSDisplayWidget>(OwnerPlayerController, TSubclassOf<UPGFPSDisplayWidget>(WidgetClass));
	if (!FPSDisplayWidget)
	{
		return;
	}

	FPSDisplayWidget->AddToViewport(1000);
}

void UPGFPSDisplayComponent::CleanupFPSDisplay()
{
	if (FPSDisplayWidget)
	{
		FPSDisplayWidget->RemoveFromParent();
		FPSDisplayWidget = nullptr;
	}
}

UPGRuntimePreloadSubsystem* UPGFPSDisplayComponent::GetRuntimePreloadSubsystem() const
{
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UPGRuntimePreloadSubsystem>() : nullptr;
}
