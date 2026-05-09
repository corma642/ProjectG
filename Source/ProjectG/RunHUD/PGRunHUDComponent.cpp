// Copyright Epic Games, Inc. All Rights Reserved.

#include "RunHUD/PGRunHUDComponent.h"

#include "Blueprint/UserWidget.h"
#include "Cargo/PGCargoStackComponent.h"
#include "Character/PGRunnerCharacter.h"
#include "GameFramework/PlayerController.h"
#include "GameModes/PGGameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Loading/PGRuntimePreloadSubsystem.h"
#include "Player/PGPlayerController.h"
#include "RunProgress/PGRunProgressComponent.h"
#include "UI/PGCargoDeliveryItemWidget.h"
#include "UI/PGRunHUDWidget.h"
#include "UI/PGRunPauseWidget.h"
#include "UI/PGSettingsScreenWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGRunHUDComponent, Log, All);

UPGRunHUDComponent::UPGRunHUDComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UPGRunHUDComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerPlayerController = Cast<APGPlayerController>(GetOwner());
	if (!OwnerPlayerController)
	{
		UE_LOG(LogPGRunHUDComponent, Warning, TEXT("PGRunHUDComponent must be owned by APGPlayerController."));
		return;
	}

	LoadSettings();
	BindGameMode();
	HandleRunnerPossessed(Cast<APGRunnerCharacter>(OwnerPlayerController->GetPawn()));

	if (BoundGameMode && (BoundGameMode->GetRunState() == EPGRunState::Countdown || BoundGameMode->GetRunState() == EPGRunState::Running))
	{
		ShowHUD();
	}
}

void UPGRunHUDComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindGameMode();
	HandleRunnerUnpossessed();
	CleanupSettingsScreen();
	CleanupPauseScreen(false);
	CleanupHUD();

	Super::EndPlay(EndPlayReason);
}

void UPGRunHUDComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!HUDWidget || !BoundCargoStackComponent)
	{
		return;
	}

	CargoRefreshElapsed += DeltaTime;
	const float RefreshInterval = FMath::Max(0.03f, CachedRunHUDSettings.CargoListRefreshInterval);
	if (CargoRefreshElapsed >= RefreshInterval)
	{
		CargoRefreshElapsed = 0.0f;
		RefreshCargoList();
	}
}

void UPGRunHUDComponent::HandleRunnerPossessed(APGRunnerCharacter* RunnerCharacter)
{
	BindRunProgress(RunnerCharacter ? RunnerCharacter->GetRunProgressComponent() : nullptr);
	BindCargoStack(RunnerCharacter ? RunnerCharacter->GetCargoStackComponent() : nullptr);
}

void UPGRunHUDComponent::HandleRunnerUnpossessed()
{
	UnbindRunProgress();
	UnbindCargoStack();
}

void UPGRunHUDComponent::HandleRunStarted()
{
	ShowHUD();
	UpdateHUDInteractionState();
	RefreshHUDValues();
	RefreshCargoList();
}

void UPGRunHUDComponent::HandleRunCountdownStarted(float Duration)
{
	ShowHUD();
	UpdateHUDInteractionState();
	RefreshHUDValues();
	RefreshCargoList();
}

void UPGRunHUDComponent::HandleRunFailed(FName FailureReason)
{
	CleanupSettingsScreen();
	CleanupPauseScreen(false);
	RefreshHUDValues();
	RefreshCargoList();
	UpdateHUDInteractionState();
	SetComponentTickEnabled(false);
	CargoRefreshElapsed = 0.0f;
}

void UPGRunHUDComponent::HandleDisplayedDistanceChanged(float DisplayedDistanceMeters)
{
	if (HUDWidget)
	{
		HUDWidget->SetDistanceMeters(DisplayedDistanceMeters);
	}
}

void UPGRunHUDComponent::HandleScoreChanged(int32 Score, FName Reason)
{
	if (HUDWidget)
	{
		HUDWidget->SetScore(Score);
	}
}

void UPGRunHUDComponent::HandleReceiptsChanged(int32 Receipts, FName Reason)
{
	if (HUDWidget)
	{
		HUDWidget->SetReceipts(Receipts);
	}
}

void UPGRunHUDComponent::HandleCargoListChanged()
{
	RefreshCargoList();
}

void UPGRunHUDComponent::HandlePauseRequested()
{
	ShowPauseScreen();
}

void UPGRunHUDComponent::HandleResumeRequested()
{
	CleanupPauseScreen(true);
}

void UPGRunHUDComponent::HandlePauseSettingsRequested()
{
	if (PauseWidget)
	{
		PauseWidget->SetPauseButtonsEnabled(false);
	}
	ShowSettingsScreen();
}

void UPGRunHUDComponent::HandleSettingsScreenClosed()
{
	if (SettingsScreenWidget)
	{
		SettingsScreenWidget->OnSettingsScreenClosed.RemoveAll(this);
		SettingsScreenWidget = nullptr;
	}
	if (PauseWidget)
	{
		PauseWidget->SetPauseButtonsEnabled(true);
	}
}

void UPGRunHUDComponent::LoadSettings()
{
	if (const UPGGameSettings* Settings = GetDefault<UPGGameSettings>())
	{
		CachedRunHUDSettings = Settings->RunHUD;
		CachedHomeFlowSettings = Settings->HomeFlow;
	}
}

bool UPGRunHUDComponent::AreRuntimeResourcesReady() const
{
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UPGRuntimePreloadSubsystem* PreloadSubsystem = GameInstance ? GameInstance->GetSubsystem<UPGRuntimePreloadSubsystem>() : nullptr;
	return !PreloadSubsystem || PreloadSubsystem->IsReady();
}

void UPGRunHUDComponent::BindGameMode()
{
	if (bIsBoundToGameMode || !GetWorld())
	{
		return;
	}

	BoundGameMode = GetWorld()->GetAuthGameMode<APGGameModeBase>();
	if (!BoundGameMode)
	{
		return;
	}

	BoundGameMode->OnRunCountdownStarted.AddDynamic(this, &UPGRunHUDComponent::HandleRunCountdownStarted);
	BoundGameMode->OnRunStarted.AddDynamic(this, &UPGRunHUDComponent::HandleRunStarted);
	BoundGameMode->OnRunFailed.AddDynamic(this, &UPGRunHUDComponent::HandleRunFailed);
	bIsBoundToGameMode = true;
}

void UPGRunHUDComponent::UnbindGameMode()
{
	if (BoundGameMode && bIsBoundToGameMode)
	{
		BoundGameMode->OnRunCountdownStarted.RemoveDynamic(this, &UPGRunHUDComponent::HandleRunCountdownStarted);
		BoundGameMode->OnRunStarted.RemoveDynamic(this, &UPGRunHUDComponent::HandleRunStarted);
		BoundGameMode->OnRunFailed.RemoveDynamic(this, &UPGRunHUDComponent::HandleRunFailed);
	}

	BoundGameMode = nullptr;
	bIsBoundToGameMode = false;
}

void UPGRunHUDComponent::BindRunProgress(UPGRunProgressComponent* RunProgressComponent)
{
	if (BoundRunProgressComponent == RunProgressComponent)
	{
		return;
	}

	UnbindRunProgress();
	BoundRunProgressComponent = RunProgressComponent;
	if (!BoundRunProgressComponent)
	{
		return;
	}

	BoundRunProgressComponent->OnDisplayedDistanceChanged.AddDynamic(this, &UPGRunHUDComponent::HandleDisplayedDistanceChanged);
	BoundRunProgressComponent->OnScoreChanged.AddDynamic(this, &UPGRunHUDComponent::HandleScoreChanged);
	BoundRunProgressComponent->OnReceiptsChanged.AddDynamic(this, &UPGRunHUDComponent::HandleReceiptsChanged);
}

void UPGRunHUDComponent::UnbindRunProgress()
{
	if (BoundRunProgressComponent)
	{
		BoundRunProgressComponent->OnDisplayedDistanceChanged.RemoveDynamic(this, &UPGRunHUDComponent::HandleDisplayedDistanceChanged);
		BoundRunProgressComponent->OnScoreChanged.RemoveDynamic(this, &UPGRunHUDComponent::HandleScoreChanged);
		BoundRunProgressComponent->OnReceiptsChanged.RemoveDynamic(this, &UPGRunHUDComponent::HandleReceiptsChanged);
	}
	BoundRunProgressComponent = nullptr;
}

void UPGRunHUDComponent::BindCargoStack(UPGCargoStackComponent* CargoStackComponent)
{
	if (BoundCargoStackComponent == CargoStackComponent)
	{
		return;
	}

	UnbindCargoStack();
	BoundCargoStackComponent = CargoStackComponent;
	if (BoundCargoStackComponent)
	{
		BoundCargoStackComponent->OnCargoListChanged.AddUObject(this, &UPGRunHUDComponent::HandleCargoListChanged);
	}
}

void UPGRunHUDComponent::UnbindCargoStack()
{
	if (BoundCargoStackComponent)
	{
		BoundCargoStackComponent->OnCargoListChanged.RemoveAll(this);
	}
	BoundCargoStackComponent = nullptr;
}

void UPGRunHUDComponent::ShowHUD()
{
	if (!OwnerPlayerController)
	{
		return;
	}

	if (HUDWidget)
	{
		RefreshHUDValues();
		RefreshCargoList();
		UpdateHUDInteractionState();
		SetComponentTickEnabled(BoundGameMode && BoundGameMode->GetRunState() != EPGRunState::Failed);
		return;
	}

	if (!AreRuntimeResourcesReady())
	{
		return;
	}

	UClass* WidgetClass = CachedRunHUDSettings.RunHUDWidgetClass.Get();
	UClass* CargoItemClass = CachedRunHUDSettings.CargoDeliveryItemWidgetClass.Get();
	if (!WidgetClass || !WidgetClass->IsChildOf(UPGRunHUDWidget::StaticClass()))
	{
		UE_LOG(LogPGRunHUDComponent, Warning, TEXT("RunHUDWidgetClass must be assigned and inherit UPGRunHUDWidget."));
		return;
	}
	if (!CargoItemClass || !CargoItemClass->IsChildOf(UPGCargoDeliveryItemWidget::StaticClass()))
	{
		UE_LOG(LogPGRunHUDComponent, Warning, TEXT("CargoDeliveryItemWidgetClass must be assigned and inherit UPGCargoDeliveryItemWidget."));
		return;
	}

	HUDWidget = CreateWidget<UPGRunHUDWidget>(OwnerPlayerController, TSubclassOf<UPGRunHUDWidget>(WidgetClass));
	if (!HUDWidget)
	{
		return;
	}

	HUDWidget->SetCargoItemWidgetClass(TSubclassOf<UPGCargoDeliveryItemWidget>(CargoItemClass));
	HUDWidget->OnPauseRequested.AddUObject(this, &UPGRunHUDComponent::HandlePauseRequested);
	HUDWidget->AddToViewport(15);
	HUDWidget->PlayEnterAnimation();
	RefreshHUDValues();
	RefreshCargoList();
	UpdateHUDInteractionState();
	SetComponentTickEnabled(true);
}

void UPGRunHUDComponent::CleanupHUD()
{
	SetComponentTickEnabled(false);
	CargoRefreshElapsed = 0.0f;

	if (HUDWidget)
	{
		HUDWidget->OnPauseRequested.RemoveAll(this);
		HUDWidget->PlayExitAnimation();
		HUDWidget = nullptr;
	}
}

void UPGRunHUDComponent::UpdateHUDInteractionState() const
{
	if (!HUDWidget)
	{
		return;
	}

	const bool bCanPause = BoundGameMode && BoundGameMode->GetRunState() == EPGRunState::Running;
	HUDWidget->SetPauseButtonEnabled(bCanPause);
}

void UPGRunHUDComponent::RefreshHUDValues()
{
	if (!HUDWidget || !BoundRunProgressComponent)
	{
		return;
	}

	HUDWidget->SetScore(BoundRunProgressComponent->GetScore());
	HUDWidget->SetDistanceMeters(BoundRunProgressComponent->GetDisplayedDistanceMeters());
	HUDWidget->SetReceipts(BoundRunProgressComponent->GetReceipts());
}

void UPGRunHUDComponent::RefreshCargoList()
{
	if (HUDWidget && BoundCargoStackComponent)
	{
		HUDWidget->RefreshCargoList(BoundCargoStackComponent->GetActiveCargoInfos());
	}
}

void UPGRunHUDComponent::ShowPauseScreen()
{
	if (!OwnerPlayerController || PauseWidget || !AreRuntimeResourcesReady())
	{
		return;
	}

	if (BoundGameMode && BoundGameMode->GetRunState() != EPGRunState::Running)
	{
		return;
	}

	UClass* WidgetClass = CachedRunHUDSettings.RunPauseWidgetClass.Get();
	if (!WidgetClass || !WidgetClass->IsChildOf(UPGRunPauseWidget::StaticClass()))
	{
		UE_LOG(LogPGRunHUDComponent, Warning, TEXT("RunPauseWidgetClass must be assigned and inherit UPGRunPauseWidget."));
		return;
	}

	PauseWidget = CreateWidget<UPGRunPauseWidget>(OwnerPlayerController, TSubclassOf<UPGRunPauseWidget>(WidgetClass));
	if (!PauseWidget)
	{
		return;
	}

	PauseWidget->OnResumeRequested.AddUObject(this, &UPGRunHUDComponent::HandleResumeRequested);
	PauseWidget->OnSettingsRequested.AddUObject(this, &UPGRunHUDComponent::HandlePauseSettingsRequested);
	PauseWidget->AddToViewport(60);
	PauseWidget->PlayOpenAnimation(CachedRunHUDSettings.PauseBackdropBlurStrength, CachedRunHUDSettings.PauseBackdropDimAlpha);
	UGameplayStatics::SetGamePaused(GetWorld(), true);
	ApplyPauseInputMode();
}

void UPGRunHUDComponent::CleanupPauseScreen(bool bResumeGame)
{
	CleanupSettingsScreen();

	if (PauseWidget)
	{
		PauseWidget->OnResumeRequested.RemoveAll(this);
		PauseWidget->OnSettingsRequested.RemoveAll(this);
		PauseWidget->RemoveFromParent();
		PauseWidget = nullptr;
	}

	if (bResumeGame)
	{
		UGameplayStatics::SetGamePaused(GetWorld(), false);
		ApplyGameInputMode();
	}
}

void UPGRunHUDComponent::ShowSettingsScreen()
{
	if (!OwnerPlayerController || SettingsScreenWidget)
	{
		return;
	}

	UClass* WidgetClass = CachedHomeFlowSettings.SettingsScreenWidgetClass.Get();
	if (!WidgetClass || !WidgetClass->IsChildOf(UPGSettingsScreenWidget::StaticClass()))
	{
		UE_LOG(LogPGRunHUDComponent, Warning, TEXT("SettingsScreenWidgetClass must be assigned and inherit UPGSettingsScreenWidget."));
		if (PauseWidget)
		{
			PauseWidget->SetPauseButtonsEnabled(true);
		}
		return;
	}

	SettingsScreenWidget = CreateWidget<UPGSettingsScreenWidget>(OwnerPlayerController, TSubclassOf<UPGSettingsScreenWidget>(WidgetClass));
	if (!SettingsScreenWidget)
	{
		if (PauseWidget)
		{
			PauseWidget->SetPauseButtonsEnabled(true);
		}
		return;
	}

	SettingsScreenWidget->OnSettingsScreenClosed.AddUObject(this, &UPGRunHUDComponent::HandleSettingsScreenClosed);
	SettingsScreenWidget->AddToViewport(70);
	SettingsScreenWidget->PlayOpenAnimation();
}

void UPGRunHUDComponent::CleanupSettingsScreen()
{
	if (SettingsScreenWidget)
	{
		SettingsScreenWidget->OnSettingsScreenClosed.RemoveAll(this);
		SettingsScreenWidget->RemoveFromParent();
		SettingsScreenWidget = nullptr;
	}
}

void UPGRunHUDComponent::ApplyGameInputMode() const
{
	if (!OwnerPlayerController)
	{
		return;
	}

	OwnerPlayerController->SetInputMode(FInputModeGameOnly());
	OwnerPlayerController->SetShowMouseCursor(false);
}

void UPGRunHUDComponent::ApplyPauseInputMode() const
{
	if (!OwnerPlayerController)
	{
		return;
	}

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	OwnerPlayerController->SetInputMode(InputMode);
	OwnerPlayerController->SetShowMouseCursor(true);
}
