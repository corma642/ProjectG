// Copyright Epic Games, Inc. All Rights Reserved.

#include "Home/PGHomeFlowComponent.h"

#include "Camera/CameraActor.h"
#include "Character/PGRunnerCharacter.h"
#include "Engine/World.h"
#include "Engine/EngineBaseTypes.h"
#include "EngineUtils.h"
#include "GameModes/PGGameModeBase.h"
#include "Loading/PGRuntimePreloadSubsystem.h"
#include "Player/PGPlayerController.h"
#include "UI/PGHomeScreenWidget.h"
#include "UI/PGRunCountdownWidget.h"
#include "UI/PGCharacterShopItemWidget.h"
#include "UI/PGShopScreenWidget.h"
#include "UI/PGSettingsScreenWidget.h"
#include "UI/PGTutorialScreenWidget.h"
#include "Tutorial/PGTutorialPageSet.h"
#include "Shop/PGPlayerShopSubsystem.h"
#include "Wallet/PGPlayerWalletSubsystem.h"

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
		CachedShopSettings = Settings->Shop;
	}

	BindGameMode();
	BindRuntimePreloadSubsystem();
	BindPlayerWalletSubsystem();
	BindPlayerShopSubsystem();

	const FString StartFlowOption = GetWorld() ? GetWorld()->URL.GetOption(TEXT("StartFlow="), TEXT("")) : FString();
	bRetryCountdownRequested = StartFlowOption.Equals(TEXT("RetryCountdown"), ESearchCase::IgnoreCase);

	if (BoundGameMode && BoundGameMode->GetRunState() == EPGRunState::Home)
	{
		if (bRetryCountdownRequested)
		{
			RequestStartDeliveryAfterPreload();
			return;
		}

		SetHomeCameraViewTarget();
		if (!RuntimePreloadSubsystem || RuntimePreloadSubsystem->IsReady())
		{
			ShowHomeScreen();
		}
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
	if (RuntimePreloadSubsystem)
	{
		RuntimePreloadSubsystem->OnPreloadCompleted.RemoveDynamic(this, &UPGHomeFlowComponent::HandleRuntimePreloadCompleted);
		RuntimePreloadSubsystem = nullptr;
	}
	UnbindGameMode();
	UnbindPlayerWalletSubsystem();
	UnbindPlayerShopSubsystem();
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

void UPGHomeFlowComponent::BindRuntimePreloadSubsystem()
{
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (RuntimePreloadSubsystem || !GameInstance)
	{
		return;
	}

	RuntimePreloadSubsystem = GameInstance->GetSubsystem<UPGRuntimePreloadSubsystem>();
	if (!RuntimePreloadSubsystem)
	{
		UE_LOG(LogPGHomeFlow, Warning, TEXT("RuntimePreloadSubsystem is unavailable. Home flow will use already loaded resources only."));
		return;
	}

	RuntimePreloadSubsystem->OnPreloadCompleted.AddDynamic(this, &UPGHomeFlowComponent::HandleRuntimePreloadCompleted);
	RuntimePreloadSubsystem->StartPreload();
}

void UPGHomeFlowComponent::BindPlayerWalletSubsystem()
{
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (PlayerWalletSubsystem || !GameInstance)
	{
		return;
	}

	PlayerWalletSubsystem = GameInstance->GetSubsystem<UPGPlayerWalletSubsystem>();
	if (PlayerWalletSubsystem)
	{
		PlayerWalletSubsystem->OnReceiptBalanceChanged.AddDynamic(this, &UPGHomeFlowComponent::HandleReceiptBalanceChanged);
	}
}

void UPGHomeFlowComponent::BindPlayerShopSubsystem()
{
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (PlayerShopSubsystem || !GameInstance)
	{
		return;
	}

	PlayerShopSubsystem = GameInstance->GetSubsystem<UPGPlayerShopSubsystem>();
}

void UPGHomeFlowComponent::UnbindPlayerWalletSubsystem()
{
	if (PlayerWalletSubsystem)
	{
		PlayerWalletSubsystem->OnReceiptBalanceChanged.RemoveDynamic(this, &UPGHomeFlowComponent::HandleReceiptBalanceChanged);
		PlayerWalletSubsystem = nullptr;
	}
}

void UPGHomeFlowComponent::UnbindPlayerShopSubsystem()
{
	if (PlayerShopSubsystem)
	{
		PlayerShopSubsystem->RestoreEquippedCharacter();
		PlayerShopSubsystem = nullptr;
	}
}

void UPGHomeFlowComponent::RequestStartDeliveryAfterPreload()
{
	if (!BoundGameMode)
	{
		return;
	}

	if (!RuntimePreloadSubsystem || RuntimePreloadSubsystem->IsReady())
	{
		bPendingStartDeliveryAfterPreload = false;
		RestoreShopPreviewRunner();
		BoundGameMode->RequestStartDelivery();
		if (HomeScreenWidget && BoundGameMode->GetRunState() == EPGRunState::Home)
		{
			HomeScreenWidget->SetHomeButtonsEnabled(true);
		}
		return;
	}

	bPendingStartDeliveryAfterPreload = true;
	RuntimePreloadSubsystem->StartPreload();
	if (HomeScreenWidget)
	{
		HomeScreenWidget->SetHomeButtonsEnabled(false);
	}
}

void UPGHomeFlowComponent::ShowHomeScreen()
{
	if (!OwnerPlayerController || HomeScreenWidget)
	{
		return;
	}

	UClass* WidgetClass = CachedHomeFlowSettings.HomeScreenWidgetClass.Get();
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

	RestoreShopPreviewRunner(true);

	HomeScreenWidget->OnStartDeliveryRequested.AddUObject(this, &UPGHomeFlowComponent::HandleStartDeliveryRequested);
	HomeScreenWidget->OnShopRequested.AddUObject(this, &UPGHomeFlowComponent::HandleShopRequested);
	HomeScreenWidget->OnSettingsRequested.AddUObject(this, &UPGHomeFlowComponent::HandleSettingsRequested);
	HomeScreenWidget->OnHelpRequested.AddUObject(this, &UPGHomeFlowComponent::HandleHelpRequested);
	HomeScreenWidget->SetReceiptBalance(PlayerWalletSubsystem ? PlayerWalletSubsystem->GetReceiptBalance() : 0);
	HomeScreenWidget->AddToViewport(10);
}

void UPGHomeFlowComponent::SetHomeCameraViewTarget()
{
	if (!OwnerPlayerController || !GetWorld())
	{
		return;
	}

	if (ACameraActor* CameraActor = FindCameraByTag(CachedHomeFlowSettings.HomeCameraTag, CachedHomeCameraActor))
	{
		OwnerPlayerController->SetViewTarget(CameraActor);
		return;
	}

	UE_LOG(LogPGHomeFlow, Warning, TEXT("Home camera with tag '%s' was not found. Player camera will be used."), *CachedHomeFlowSettings.HomeCameraTag.ToString());
}

void UPGHomeFlowComponent::SetShopCameraViewTarget()
{
	if (!OwnerPlayerController || !GetWorld())
	{
		return;
	}

	if (ACameraActor* CameraActor = FindCameraByTag(CachedShopSettings.ShopCameraTag, CachedShopCameraActor))
	{
		OwnerPlayerController->SetViewTargetWithBlend(CameraActor, CachedShopSettings.HomeToShopCameraBlendTime);
		return;
	}

	UE_LOG(LogPGHomeFlow, Warning, TEXT("Shop camera with tag '%s' was not found. Current camera will be used."), *CachedShopSettings.ShopCameraTag.ToString());
}

ACameraActor* UPGHomeFlowComponent::FindCameraByTag(FName CameraTag, TWeakObjectPtr<ACameraActor>& CameraCache) const
{
	if (CameraCache.IsValid())
	{
		return CameraCache.Get();
	}

	if (CameraTag.IsNone() || !GetWorld())
	{
		return nullptr;
	}

	for (TActorIterator<ACameraActor> CameraIt(GetWorld()); CameraIt; ++CameraIt)
	{
		ACameraActor* CameraActor = *CameraIt;
		if (CameraActor && CameraActor->ActorHasTag(CameraTag))
		{
			CameraCache = CameraActor;
			return CameraActor;
		}
	}

	return nullptr;
}

void UPGHomeFlowComponent::RestoreShopPreviewRunner(bool bRestoreHomeTransform)
{
	if (!PlayerShopSubsystem)
	{
		return;
	}

	APGRunnerCharacter* PreviewRunner = BoundGameMode ? BoundGameMode->GetPlayableRunnerCharacter() : nullptr;
	if (bRestoreHomeTransform && PreviewRunner)
	{
		PreviewRunner->RestoreHomePreviewTransform();
	}

	PlayerShopSubsystem->RefreshShopCatalog();
	PlayerShopSubsystem->SetPreviewRunner(PreviewRunner);
	PlayerShopSubsystem->RestoreEquippedCharacter();
}

void UPGHomeFlowComponent::BlendToRunnerCamera()
{
	if (!OwnerPlayerController)
	{
		return;
	}

	if (APGRunnerCharacter* RunnerCharacter = BoundGameMode ? BoundGameMode->GetPlayableRunnerCharacter() : nullptr)
	{
		OwnerPlayerController->SetViewTargetWithBlend(RunnerCharacter, CachedHomeFlowSettings.HomeToRunnerCameraBlendTime);
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

	UClass* WidgetClass = CachedHomeFlowSettings.RunCountdownWidgetClass.Get();
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
		HomeScreenWidget->OnShopRequested.RemoveAll(this);
		HomeScreenWidget->OnSettingsRequested.RemoveAll(this);
		HomeScreenWidget->OnHelpRequested.RemoveAll(this);
		HomeScreenWidget->RemoveFromParent();
		HomeScreenWidget = nullptr;
	}

	if (CountdownWidget)
	{
		CountdownWidget->RemoveFromParent();
		CountdownWidget = nullptr;
	}

	if (ShopScreenWidget)
	{
		ShopScreenWidget->OnShopScreenClosed.RemoveAll(this);
		ShopScreenWidget->RemoveFromParent();
		ShopScreenWidget = nullptr;
	}

	if (SettingsScreenWidget)
	{
		SettingsScreenWidget->OnSettingsScreenClosed.RemoveAll(this);
		SettingsScreenWidget->RemoveFromParent();
		SettingsScreenWidget = nullptr;
	}

	if (TutorialScreenWidget)
	{
		TutorialScreenWidget->OnTutorialClosed.RemoveAll(this);
		TutorialScreenWidget->RemoveFromParent();
		TutorialScreenWidget = nullptr;
	}
}

void UPGHomeFlowComponent::BlendToHomeCamera()
{
	if (!OwnerPlayerController || !GetWorld())
	{
		return;
	}

	if (ACameraActor* CameraActor = FindCameraByTag(CachedHomeFlowSettings.HomeCameraTag, CachedHomeCameraActor))
	{
		OwnerPlayerController->SetViewTargetWithBlend(CameraActor, CachedShopSettings.ShopToHomeCameraBlendTime);
		return;
	}

	SetHomeCameraViewTarget();
}

void UPGHomeFlowComponent::HandleStartDeliveryRequested()
{
	if (SettingsScreenWidget || TutorialScreenWidget || ShopScreenWidget)
	{
		return;
	}

	RequestStartDeliveryAfterPreload();
}

void UPGHomeFlowComponent::HandleShopRequested()
{
	if (SettingsScreenWidget || TutorialScreenWidget || ShopScreenWidget || !BoundGameMode || BoundGameMode->GetRunState() != EPGRunState::Home)
	{
		return;
	}

	if (!OwnerPlayerController || !PlayerShopSubsystem || !PlayerWalletSubsystem)
	{
		return;
	}
	PlayerShopSubsystem->RefreshShopCatalog();
	PlayerShopSubsystem->SetPreviewRunner(BoundGameMode ? BoundGameMode->GetPlayableRunnerCharacter() : nullptr);

	UClass* WidgetClass = CachedShopSettings.ShopScreenWidgetClass.Get();
	UClass* ItemWidgetClass = CachedShopSettings.CharacterShopItemWidgetClass.Get();
	if (!WidgetClass)
	{
		UE_LOG(LogPGHomeFlow, Warning, TEXT("ShopScreenWidgetClass is not set. Create WBP_ShopScreen and assign it in Go Deliver Settings > Shop."));
		return;
	}
	if (!WidgetClass->IsChildOf(UPGShopScreenWidget::StaticClass()))
	{
		UE_LOG(LogPGHomeFlow, Warning, TEXT("ShopScreenWidgetClass '%s' must inherit UPGShopScreenWidget."), *GetNameSafe(WidgetClass));
		return;
	}
	if (!ItemWidgetClass || !ItemWidgetClass->IsChildOf(UPGCharacterShopItemWidget::StaticClass()))
	{
		UE_LOG(LogPGHomeFlow, Warning, TEXT("CharacterShopItemWidgetClass must inherit UPGCharacterShopItemWidget."));
		return;
	}

	ShopScreenWidget = CreateWidget<UPGShopScreenWidget>(OwnerPlayerController, TSubclassOf<UPGShopScreenWidget>(WidgetClass));
	if (!ShopScreenWidget)
	{
		return;
	}

	if (HomeScreenWidget)
	{
		HomeScreenWidget->SetHomeButtonsEnabled(false);
		HomeScreenWidget->SetRenderOpacity(0.0f);
	}

	SetShopCameraViewTarget();
	ShopScreenWidget->OnShopScreenClosed.AddUObject(this, &UPGHomeFlowComponent::HandleShopScreenClosed);
	ShopScreenWidget->AddToViewport(30);
	ShopScreenWidget->InitializeShop(PlayerShopSubsystem, PlayerWalletSubsystem, TSubclassOf<UPGCharacterShopItemWidget>(ItemWidgetClass));
	ShopScreenWidget->PlayOpenAnimation();
}

void UPGHomeFlowComponent::HandleShopScreenClosed()
{
	if (ShopScreenWidget)
	{
		ShopScreenWidget->OnShopScreenClosed.RemoveAll(this);
		ShopScreenWidget = nullptr;
	}

	if (PlayerShopSubsystem)
	{
		RestoreShopPreviewRunner();
	}

	BlendToHomeCamera();
	if (HomeScreenWidget)
	{
		HomeScreenWidget->SetRenderOpacity(1.0f);
		HomeScreenWidget->SetHomeButtonsEnabled(true);
	}
}

void UPGHomeFlowComponent::HandleSettingsRequested()
{
	if (TutorialScreenWidget || ShopScreenWidget)
	{
		return;
	}

	ShowSettingsScreen();
}

void UPGHomeFlowComponent::HandleSettingsScreenClosed()
{
	if (SettingsScreenWidget)
	{
		SettingsScreenWidget->OnSettingsScreenClosed.RemoveAll(this);
		SettingsScreenWidget = nullptr;
	}
}

void UPGHomeFlowComponent::HandleHelpRequested()
{
	if (SettingsScreenWidget || ShopScreenWidget)
	{
		return;
	}

	ShowTutorialScreen();
}

void UPGHomeFlowComponent::HandleTutorialClosed()
{
	if (TutorialScreenWidget)
	{
		TutorialScreenWidget->OnTutorialClosed.RemoveAll(this);
		TutorialScreenWidget = nullptr;
	}

	if (HomeScreenWidget)
	{
		HomeScreenWidget->SetHomeButtonsEnabled(true);
	}
}

void UPGHomeFlowComponent::ShowSettingsScreen()
{
	if (!OwnerPlayerController || SettingsScreenWidget || TutorialScreenWidget)
	{
		return;
	}

	UClass* WidgetClass = CachedHomeFlowSettings.SettingsScreenWidgetClass.Get();
	if (!WidgetClass)
	{
		UE_LOG(LogPGHomeFlow, Warning, TEXT("SettingsScreenWidgetClass is not set. Create WBP_SettingsScreen and assign it in Go Deliver Settings > Home Flow."));
		return;
	}
	if (!WidgetClass->IsChildOf(UPGSettingsScreenWidget::StaticClass()))
	{
		UE_LOG(LogPGHomeFlow, Warning, TEXT("SettingsScreenWidgetClass '%s' must inherit UPGSettingsScreenWidget."), *GetNameSafe(WidgetClass));
		return;
	}

	SettingsScreenWidget = CreateWidget<UPGSettingsScreenWidget>(OwnerPlayerController, TSubclassOf<UPGSettingsScreenWidget>(WidgetClass));
	if (!SettingsScreenWidget)
	{
		return;
	}

	SettingsScreenWidget->OnSettingsScreenClosed.AddUObject(this, &UPGHomeFlowComponent::HandleSettingsScreenClosed);
	SettingsScreenWidget->AddToViewport(30);
	SettingsScreenWidget->PlayOpenAnimation();
}

void UPGHomeFlowComponent::ShowTutorialScreen()
{
	if (!OwnerPlayerController || TutorialScreenWidget)
	{
		return;
	}

	UClass* WidgetClass = CachedHomeFlowSettings.TutorialScreenWidgetClass.Get();
	if (!WidgetClass)
	{
		UE_LOG(LogPGHomeFlow, Warning, TEXT("TutorialScreenWidgetClass is not set. Create WBP_TutorialScreen and assign it in Go Deliver Settings > Home Flow."));
		return;
	}
	if (!WidgetClass->IsChildOf(UPGTutorialScreenWidget::StaticClass()))
	{
		UE_LOG(LogPGHomeFlow, Warning, TEXT("TutorialScreenWidgetClass '%s' must inherit UPGTutorialScreenWidget."), *GetNameSafe(WidgetClass));
		return;
	}

	UPGTutorialPageSet* PageSet = CachedHomeFlowSettings.TutorialPageSet.Get();
	if (!PageSet)
	{
		UE_LOG(LogPGHomeFlow, Warning, TEXT("TutorialPageSet is not set. Create DA_TutorialPageSet and assign it in Go Deliver Settings > Home Flow."));
		return;
	}

	TutorialScreenWidget = CreateWidget<UPGTutorialScreenWidget>(OwnerPlayerController, TSubclassOf<UPGTutorialScreenWidget>(WidgetClass));
	if (!TutorialScreenWidget)
	{
		return;
	}

	if (HomeScreenWidget)
	{
		HomeScreenWidget->SetHomeButtonsEnabled(false);
	}

	TutorialScreenWidget->OnTutorialClosed.AddUObject(this, &UPGHomeFlowComponent::HandleTutorialClosed);
	TutorialScreenWidget->AddToViewport(30);
	TutorialScreenWidget->InitializeTutorial(PageSet);
}

void UPGHomeFlowComponent::HandleRunCountdownStarted(float Duration)
{
	if (SettingsScreenWidget)
	{
		SettingsScreenWidget->OnSettingsScreenClosed.RemoveAll(this);
		SettingsScreenWidget->RemoveFromParent();
		SettingsScreenWidget = nullptr;
	}

	if (TutorialScreenWidget)
	{
		TutorialScreenWidget->OnTutorialClosed.RemoveAll(this);
		TutorialScreenWidget->RemoveFromParent();
		TutorialScreenWidget = nullptr;
	}

	if (ShopScreenWidget)
	{
		ShopScreenWidget->OnShopScreenClosed.RemoveAll(this);
		ShopScreenWidget->RemoveFromParent();
		ShopScreenWidget = nullptr;
	}

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

void UPGHomeFlowComponent::HandleRuntimePreloadCompleted()
{
	if (!BoundGameMode || BoundGameMode->GetRunState() != EPGRunState::Home)
	{
		return;
	}

	if (bPendingStartDeliveryAfterPreload || bRetryCountdownRequested)
	{
		RequestStartDeliveryAfterPreload();
		return;
	}

	ShowHomeScreen();
}

void UPGHomeFlowComponent::HandleReceiptBalanceChanged(int32 NewBalance, FName Reason)
{
	if (HomeScreenWidget)
	{
		HomeScreenWidget->SetReceiptBalance(NewBalance);
	}
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
