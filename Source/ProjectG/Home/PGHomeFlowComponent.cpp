// Copyright Epic Games, Inc. All Rights Reserved.

#include "Home/PGHomeFlowComponent.h"

#include "Camera/CameraActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameModes/PGGameModeBase.h"
#include "Player/PGPlayerController.h"
#include "UI/PGHomeScreenWidget.h"
#include "UI/PGRunCountdownWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGHomeFlow, Log, All);

UPGHomeFlowComponent::UPGHomeFlowComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPGHomeFlowComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerPlayerController = Cast<APGPlayerController>(GetOwner());
	if (!OwnerPlayerController)
	{
		UE_LOG(LogPGHomeFlow, Warning, TEXT("PGHomeFlowComponent must be owned by APGPlayerController."));
		return;
	}

	if (const UPGGameSettings* Settings = GetDefault<UPGGameSettings>())
	{
		CachedHomeFlowSettings = Settings->HomeFlow;
	}

	BindGameMode();

	if (BoundGameMode && BoundGameMode->GetRunState() == EPGRunState::Home)
	{
		SetHomeCameraViewTarget();
		ShowHomeScreen();
		ApplyHomeInputMode();
	}
	else if (BoundGameMode && BoundGameMode->GetRunState() == EPGRunState::Countdown)
	{
		HandleRunCountdownStarted(CachedHomeFlowSettings.RunStartCountdownDuration);
	}
	else if (BoundGameMode && BoundGameMode->GetRunState() == EPGRunState::Running)
	{
		ApplyGameInputMode();
	}
}

void UPGHomeFlowComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindGameMode();
	CleanupWidgets();
	Super::EndPlay(EndPlayReason);
}

void UPGHomeFlowComponent::BindGameMode()
{
	if (bIsBoundToGameMode || !GetWorld())
	{
		return;
	}

	BoundGameMode = GetWorld()->GetAuthGameMode<APGGameModeBase>();
	if (!BoundGameMode)
	{
		UE_LOG(LogPGHomeFlow, Warning, TEXT("PGHomeFlowComponent could not find APGGameModeBase."));
		return;
	}

	BoundGameMode->OnRunCountdownStarted.AddDynamic(this, &UPGHomeFlowComponent::HandleRunCountdownStarted);
	BoundGameMode->OnRunStarted.AddDynamic(this, &UPGHomeFlowComponent::HandleRunStarted);
	bIsBoundToGameMode = true;
}

void UPGHomeFlowComponent::UnbindGameMode()
{
	if (!BoundGameMode || !bIsBoundToGameMode)
	{
		return;
	}

	BoundGameMode->OnRunCountdownStarted.RemoveDynamic(this, &UPGHomeFlowComponent::HandleRunCountdownStarted);
	BoundGameMode->OnRunStarted.RemoveDynamic(this, &UPGHomeFlowComponent::HandleRunStarted);
	bIsBoundToGameMode = false;
	BoundGameMode = nullptr;
}

void UPGHomeFlowComponent::ShowHomeScreen()
{
	if (!OwnerPlayerController || HomeScreenWidget)
	{
		return;
	}

	UClass* WidgetClass = CachedHomeFlowSettings.HomeScreenWidgetClass.LoadSynchronous();
	if (!WidgetClass)
	{
		UE_LOG(LogPGHomeFlow, Warning, TEXT("HomeScreenWidgetClass is not set. Create BP_HomeScreenWidget and assign it in Go Deliver Settings > Home Flow."));
		return;
	}
	if (!WidgetClass->IsChildOf(UPGHomeScreenWidget::StaticClass()))
	{
		UE_LOG(LogPGHomeFlow, Warning, TEXT("HomeScreenWidgetClass '%s' must inherit UPGHomeScreenWidget."), *GetNameSafe(WidgetClass));
		return;
	}

	HomeScreenWidget = CreateWidget<UPGHomeScreenWidget>(OwnerPlayerController, TSubclassOf<UPGHomeScreenWidget>(WidgetClass));
	if (!HomeScreenWidget)
	{
		return;
	}

	HomeScreenWidget->OnStartDeliveryRequested.AddUObject(this, &UPGHomeFlowComponent::HandleStartDeliveryRequested);
	HomeScreenWidget->AddToViewport(10);
}

void UPGHomeFlowComponent::SetHomeCameraViewTarget()
{
	if (!OwnerPlayerController || !GetWorld())
	{
		return;
	}

	for (TActorIterator<ACameraActor> CameraIt(GetWorld()); CameraIt; ++CameraIt)
	{
		ACameraActor* CameraActor = *CameraIt;
		if (CameraActor && CameraActor->ActorHasTag(CachedHomeFlowSettings.HomeCameraTag))
		{
			OwnerPlayerController->SetViewTarget(CameraActor);
			return;
		}
	}

	UE_LOG(LogPGHomeFlow, Warning, TEXT("Home camera with tag '%s' was not found. Player camera will be used."), *CachedHomeFlowSettings.HomeCameraTag.ToString());
}

void UPGHomeFlowComponent::BlendToRunnerCamera()
{
	if (!OwnerPlayerController)
	{
		return;
	}

	if (APawn* Pawn = OwnerPlayerController->GetPawn())
	{
		OwnerPlayerController->SetViewTargetWithBlend(Pawn, CachedHomeFlowSettings.HomeToRunnerCameraBlendTime);
	}
}

void UPGHomeFlowComponent::ShowCountdown(float Duration)
{
	if (!OwnerPlayerController)
	{
		return;
	}

	if (CountdownWidget)
	{
		CountdownWidget->RemoveFromParent();
		CountdownWidget = nullptr;
	}

	UClass* WidgetClass = CachedHomeFlowSettings.RunCountdownWidgetClass.LoadSynchronous();
	if (!WidgetClass)
	{
		UE_LOG(LogPGHomeFlow, Warning, TEXT("RunCountdownWidgetClass is not set. Create BP_RunCountdownWidget and assign it in Go Deliver Settings > Home Flow."));
		return;
	}
	if (!WidgetClass->IsChildOf(UPGRunCountdownWidget::StaticClass()))
	{
		UE_LOG(LogPGHomeFlow, Warning, TEXT("RunCountdownWidgetClass '%s' must inherit UPGRunCountdownWidget."), *GetNameSafe(WidgetClass));
		return;
	}

	CountdownWidget = CreateWidget<UPGRunCountdownWidget>(OwnerPlayerController, TSubclassOf<UPGRunCountdownWidget>(WidgetClass));
	if (!CountdownWidget)
	{
		return;
	}

	CountdownWidget->AddToViewport(20);
	CountdownWidget->StartCountdown(Duration);
}

void UPGHomeFlowComponent::CleanupWidgets()
{
	if (HomeScreenWidget)
	{
		HomeScreenWidget->OnStartDeliveryRequested.RemoveAll(this);
		HomeScreenWidget->RemoveFromParent();
		HomeScreenWidget = nullptr;
	}

	if (CountdownWidget)
	{
		CountdownWidget->RemoveFromParent();
		CountdownWidget = nullptr;
	}
}

void UPGHomeFlowComponent::HandleStartDeliveryRequested()
{
	if (BoundGameMode)
	{
		BoundGameMode->RequestStartDelivery();
	}
}

void UPGHomeFlowComponent::HandleRunCountdownStarted(float Duration)
{
	if (HomeScreenWidget)
	{
		HomeScreenWidget->PlayExitAnimation(CachedHomeFlowSettings.HomeExitAnimationDuration);
	}

	BlendToRunnerCamera();
	ShowCountdown(Duration);
}

void UPGHomeFlowComponent::HandleRunStarted()
{
	if (CountdownWidget)
	{
		CountdownWidget->RemoveFromParent();
		CountdownWidget = nullptr;
	}

	ApplyGameInputMode();
}

void UPGHomeFlowComponent::ApplyHomeInputMode()
{
	if (!OwnerPlayerController)
	{
		return;
	}

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	OwnerPlayerController->SetInputMode(InputMode);
	OwnerPlayerController->bShowMouseCursor = true;
}

void UPGHomeFlowComponent::ApplyGameInputMode()
{
	if (!OwnerPlayerController)
	{
		return;
	}

	OwnerPlayerController->SetInputMode(FInputModeGameOnly());
	OwnerPlayerController->bShowMouseCursor = false;
}
