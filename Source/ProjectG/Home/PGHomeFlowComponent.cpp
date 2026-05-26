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
#include "UI/PGReceiptShopPackItemWidget.h"
#include "UI/PGReceiptShopWidget.h"
#include "UI/PGRunCountdownWidget.h"
#include "UI/PGScreenTransitionFadeWidget.h"
#include "UI/PGCharacterShopItemWidget.h"
#include "UI/PGShopScreenWidget.h"
#include "UI/PGSettingsScreenWidget.h"
#include "UI/PGTutorialScreenWidget.h"
#include "Tutorial/PGTutorialPageSet.h"
#include "User/PGUserDataSubsystem.h"

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
	BindUserDataSubsystem();

	const FString StartFlowOption = GetWorld() ? GetWorld()->URL.GetOption(TEXT("StartFlow="), TEXT("")) : FString();
	bRetryCountdownRequested = StartFlowOption.Equals(TEXT("RetryCountdown"), ESearchCase::IgnoreCase);

		// 레벨 리로드 기반 Retry는 Home UI를 만들지 않고, 필수 리소스 예열이 끝난 뒤 바로 카운트다운으로 진입한다.
	if (BoundGameMode && BoundGameMode->GetRunState() == EPGRunState::Home)
	{
		if (bRetryCountdownRequested)
		{
			RequestStartDeliveryAfterPreload();
			return;
		}

		// 일반 앱 진입은 HomeCamera를 먼저 잡고, UI는 예열 완료 후 표시해 첫 진입 리소스 누락을 피한다.
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
	UnbindUserDataSubsystem();
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

void UPGHomeFlowComponent::BindUserDataSubsystem()
{
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (UserDataSubsystem || !GameInstance)
	{
		return;
	}

	UserDataSubsystem = GameInstance->GetSubsystem<UPGUserDataSubsystem>();
	if (UserDataSubsystem)
	{
		UserDataSubsystem->OnReceiptBalanceChanged.AddDynamic(this, &UPGHomeFlowComponent::HandleReceiptBalanceChanged);
	}
}

void UPGHomeFlowComponent::UnbindUserDataSubsystem()
{
	if (UserDataSubsystem)
	{
		UserDataSubsystem->OnReceiptBalanceChanged.RemoveDynamic(this, &UPGHomeFlowComponent::HandleReceiptBalanceChanged);
		UserDataSubsystem->RestoreEquippedCharacter();
		UserDataSubsystem = nullptr;
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
		// Retry는 이미 레벨 로딩으로 화면 전환이 끝난 상태이므로 추가 페이드 없이 카운트다운으로 들어간다.
		if (bRetryCountdownRequested)
		{
			bPendingStartDeliveryAfterPreload = false;
			RestoreShopPreviewRunner();
			BoundGameMode->RequestStartDelivery();
			return;
		}

		if (HomeScreenWidget)
		{
			HomeScreenWidget->SetHomeButtonsEnabled(false);
		}

		if (!StartScreenTransition(EPGHomeFlowTransition::HomeToCountdown) && HomeScreenWidget)
		{
			HomeScreenWidget->SetHomeButtonsEnabled(true);
		}
		return;
	}

	// 예열 중 START를 누르면 입력만 잠그고, OnPreloadCompleted에서 동일 요청을 다시 처리한다.
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

	// Home은 레벨에 배치된 프리뷰 자세를 기준으로 보여준다. Shop에서 미리 본 스킨도 여기서 장착 스킨으로 복구된다.
	RestoreShopPreviewRunner(true);

	HomeScreenWidget->OnStartDeliveryRequested.AddUObject(this, &UPGHomeFlowComponent::HandleStartDeliveryRequested);
	HomeScreenWidget->OnShopRequested.AddUObject(this, &UPGHomeFlowComponent::HandleShopRequested);
	HomeScreenWidget->OnSettingsRequested.AddUObject(this, &UPGHomeFlowComponent::HandleSettingsRequested);
	HomeScreenWidget->OnHelpRequested.AddUObject(this, &UPGHomeFlowComponent::HandleHelpRequested);
	HomeScreenWidget->OnReceiptShopRequested.AddUObject(this, &UPGHomeFlowComponent::HandleReceiptShopRequested);
	HomeScreenWidget->SetReceiptBalance(UserDataSubsystem ? UserDataSubsystem->GetReceiptBalance() : 0);
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

	// Home/Shop 카메라는 레벨 배치 Actor이므로 태그 검색은 최초 1회만 수행하고 이후 약한 참조를 재사용한다.
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
	if (!UserDataSubsystem)
	{
		return;
	}

	APGRunnerCharacter* PreviewRunner = BoundGameMode ? BoundGameMode->GetPlayableRunnerCharacter() : nullptr;
	if (bRestoreHomeTransform && PreviewRunner)
	{
		// Home으로 돌아올 때만 배치 회전을 복구한다. Shop 닫기처럼 Home 상태의 연장인 경우 위치/회전은 건드리지 않는다.
		PreviewRunner->RestoreHomePreviewTransform();
	}

	UserDataSubsystem->RefreshShopCatalog();
	UserDataSubsystem->SetPreviewRunner(PreviewRunner);
	UserDataSubsystem->RestoreEquippedCharacter();
}

void UPGHomeFlowComponent::BlendToRunnerCamera()
{
	if (!OwnerPlayerController)
	{
		return;
	}

	if (APGRunnerCharacter* RunnerCharacter = BoundGameMode ? BoundGameMode->GetPlayableRunnerCharacter() : nullptr)
	{
		// GameMode가 START 시점에 Possess한 단일 러너를 ViewTarget으로 사용해 Home/Shop 프리뷰 Actor와 런 Actor가 갈라지지 않게 한다.
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
		HomeScreenWidget->OnReceiptShopRequested.RemoveAll(this);
		HomeScreenWidget->RemoveFromParent();
		HomeScreenWidget = nullptr;
	}

	if (CountdownWidget)
	{
		CountdownWidget->RemoveFromParent();
		CountdownWidget = nullptr;
	}

	if (ScreenTransitionFadeWidget)
	{
		ScreenTransitionFadeWidget->OnFadeCovered.RemoveAll(this);
		ScreenTransitionFadeWidget->OnFadeFinished.RemoveAll(this);
		ScreenTransitionFadeWidget->RemoveFromParent();
		ScreenTransitionFadeWidget = nullptr;
	}

	if (ShopScreenWidget)
	{
		ShopScreenWidget->OnShopCloseRequested.RemoveAll(this);
		ShopScreenWidget->OnReceiptShopRequested.RemoveAll(this);
		ShopScreenWidget->RemoveFromParent();
		ShopScreenWidget = nullptr;
	}

	if (ReceiptShopWidget)
	{
		ReceiptShopWidget->OnReceiptShopClosed.RemoveAll(this);
		ReceiptShopWidget->RemoveFromParent();
		ReceiptShopWidget = nullptr;
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

bool UPGHomeFlowComponent::StartScreenTransition(EPGHomeFlowTransition Transition)
{
	if (Transition == EPGHomeFlowTransition::None || bIsScreenTransitionActive)
	{
		return false;
	}

	PendingScreenTransition = Transition;
	bIsScreenTransitionActive = true;

	UClass* WidgetClass = CachedHomeFlowSettings.ScreenTransitionFadeWidgetClass.Get();
	if (!WidgetClass)
	{
		// 페이드 위젯은 선택적 폴리싱 리소스다. 미설정 상태에서는 기존 즉시 전환 흐름을 유지한다.
		ApplyPendingScreenTransition();
		FinishScreenTransition();
		return true;
	}
	if (!WidgetClass->IsChildOf(UPGScreenTransitionFadeWidget::StaticClass()))
	{
		UE_LOG(LogPGHomeFlow, Warning, TEXT("ScreenTransitionFadeWidgetClass '%s' must inherit UPGScreenTransitionFadeWidget."), *GetNameSafe(WidgetClass));
		ApplyPendingScreenTransition();
		FinishScreenTransition();
		return true;
	}

	ScreenTransitionFadeWidget = CreateWidget<UPGScreenTransitionFadeWidget>(OwnerPlayerController, TSubclassOf<UPGScreenTransitionFadeWidget>(WidgetClass));
	if (!ScreenTransitionFadeWidget)
	{
		ApplyPendingScreenTransition();
		FinishScreenTransition();
		return true;
	}

	ScreenTransitionFadeWidget->OnFadeCovered.AddUObject(this, &UPGHomeFlowComponent::ApplyPendingScreenTransition);
	ScreenTransitionFadeWidget->OnFadeFinished.AddUObject(this, &UPGHomeFlowComponent::FinishScreenTransition);
	ScreenTransitionFadeWidget->AddToViewport(500);
	ScreenTransitionFadeWidget->PlayCoverThenReveal(
		CachedHomeFlowSettings.TransitionFadeInDuration,
		CachedHomeFlowSettings.TransitionHoldDuration,
		CachedHomeFlowSettings.TransitionFadeOutDuration,
		CachedHomeFlowSettings.TransitionMaxOpacity);

	return true;
}

void UPGHomeFlowComponent::ApplyPendingScreenTransition()
{
	switch (PendingScreenTransition)
	{
	case EPGHomeFlowTransition::HomeToShop:
		ShowShopScreen();
		break;
	case EPGHomeFlowTransition::ShopToHome:
		if (ShopScreenWidget)
		{
			ShopScreenWidget->OnShopCloseRequested.RemoveAll(this);
			ShopScreenWidget->OnReceiptShopRequested.RemoveAll(this);
			ShopScreenWidget->RemoveFromParent();
			ShopScreenWidget = nullptr;
		}
		if (UserDataSubsystem)
		{
			RestoreShopPreviewRunner();
		}
		BlendToHomeCamera();
		if (HomeScreenWidget)
		{
			HomeScreenWidget->SetRenderOpacity(1.0f);
		}
		break;
	case EPGHomeFlowTransition::HomeToCountdown:
		bPendingStartDeliveryAfterPreload = false;
		RestoreShopPreviewRunner();
		if (BoundGameMode)
		{
			BoundGameMode->RequestStartDelivery();
		}
		break;
	default:
		break;
	}
}

void UPGHomeFlowComponent::FinishScreenTransition()
{
	if (ScreenTransitionFadeWidget)
	{
		ScreenTransitionFadeWidget->OnFadeCovered.RemoveAll(this);
		ScreenTransitionFadeWidget->OnFadeFinished.RemoveAll(this);
		ScreenTransitionFadeWidget = nullptr;
	}

	if (PendingScreenTransition == EPGHomeFlowTransition::ShopToHome && HomeScreenWidget)
	{
		HomeScreenWidget->SetHomeButtonsEnabled(true);
	}
	else if (PendingScreenTransition == EPGHomeFlowTransition::HomeToShop && !ShopScreenWidget && HomeScreenWidget)
	{
		HomeScreenWidget->SetRenderOpacity(1.0f);
		HomeScreenWidget->SetHomeButtonsEnabled(true);
	}
	else if (PendingScreenTransition == EPGHomeFlowTransition::HomeToCountdown && HomeScreenWidget && BoundGameMode && BoundGameMode->GetRunState() == EPGRunState::Home)
	{
		HomeScreenWidget->SetHomeButtonsEnabled(true);
	}

	PendingScreenTransition = EPGHomeFlowTransition::None;
	bIsScreenTransitionActive = false;
}

bool UPGHomeFlowComponent::AreShopScreenResourcesReady() const
{
	const UClass* WidgetClass = CachedShopSettings.ShopScreenWidgetClass.Get();
	const UClass* ItemWidgetClass = CachedShopSettings.CharacterShopItemWidgetClass.Get();
	if (!WidgetClass)
	{
		UE_LOG(LogPGHomeFlow, Warning, TEXT("ShopScreenWidgetClass is not set. Create WBP_ShopScreen and assign it in Go Deliver Settings > Shop."));
		return false;
	}
	if (!WidgetClass->IsChildOf(UPGShopScreenWidget::StaticClass()))
	{
		UE_LOG(LogPGHomeFlow, Warning, TEXT("ShopScreenWidgetClass '%s' must inherit UPGShopScreenWidget."), *GetNameSafe(WidgetClass));
		return false;
	}
	if (!ItemWidgetClass || !ItemWidgetClass->IsChildOf(UPGCharacterShopItemWidget::StaticClass()))
	{
		UE_LOG(LogPGHomeFlow, Warning, TEXT("CharacterShopItemWidgetClass must inherit UPGCharacterShopItemWidget."));
		return false;
	}

	return true;
}

void UPGHomeFlowComponent::ShowShopScreen()
{
	if (!OwnerPlayerController || !UserDataSubsystem || ShopScreenWidget || !AreShopScreenResourcesReady())
	{
		return;
	}

	UserDataSubsystem->RefreshShopCatalog();
	UserDataSubsystem->SetPreviewRunner(BoundGameMode ? BoundGameMode->GetPlayableRunnerCharacter() : nullptr);

	UClass* WidgetClass = CachedShopSettings.ShopScreenWidgetClass.Get();
	UClass* ItemWidgetClass = CachedShopSettings.CharacterShopItemWidgetClass.Get();
	ShopScreenWidget = CreateWidget<UPGShopScreenWidget>(OwnerPlayerController, TSubclassOf<UPGShopScreenWidget>(WidgetClass));
	if (!ShopScreenWidget)
	{
		return;
	}

	if (HomeScreenWidget)
	{
		HomeScreenWidget->SetRenderOpacity(0.0f);
	}

	SetShopCameraViewTarget();
	ShopScreenWidget->OnShopCloseRequested.AddUObject(this, &UPGHomeFlowComponent::HandleShopCloseRequested);
	ShopScreenWidget->OnReceiptShopRequested.AddUObject(this, &UPGHomeFlowComponent::HandleReceiptShopRequested);
	ShopScreenWidget->AddToViewport(30);
	ShopScreenWidget->InitializeShop(UserDataSubsystem, TSubclassOf<UPGCharacterShopItemWidget>(ItemWidgetClass));
	ShopScreenWidget->PlayOpenAnimation();
}

void UPGHomeFlowComponent::HandleStartDeliveryRequested()
{
	if (SettingsScreenWidget || TutorialScreenWidget || ShopScreenWidget || ReceiptShopWidget || bIsScreenTransitionActive)
	{
		// Home 위에 다른 전면 UI가 올라와 있거나 전환 중이면 화면 소유권을 유지하기 위해 START 입력을 무시한다.
		return;
	}

	RequestStartDeliveryAfterPreload();
}

void UPGHomeFlowComponent::HandleShopRequested()
{
	if (SettingsScreenWidget || TutorialScreenWidget || ShopScreenWidget || ReceiptShopWidget || bIsScreenTransitionActive || !BoundGameMode || BoundGameMode->GetRunState() != EPGRunState::Home)
	{
		return;
	}

	if (!OwnerPlayerController || !UserDataSubsystem || !AreShopScreenResourcesReady())
	{
		return;
	}

	if (HomeScreenWidget)
	{
		HomeScreenWidget->SetHomeButtonsEnabled(false);
	}

	if (!StartScreenTransition(EPGHomeFlowTransition::HomeToShop) && HomeScreenWidget)
	{
		HomeScreenWidget->SetHomeButtonsEnabled(true);
	}
}

void UPGHomeFlowComponent::HandleShopCloseRequested()
{
	if (bIsScreenTransitionActive)
	{
		return;
	}

	if (ShopScreenWidget)
	{
		ShopScreenWidget->SetShopInteractionEnabled(false);
	}

	if (!StartScreenTransition(EPGHomeFlowTransition::ShopToHome))
	{
		if (HomeScreenWidget)
		{
			HomeScreenWidget->SetHomeButtonsEnabled(true);
		}
	}
}

void UPGHomeFlowComponent::HandleReceiptShopRequested()
{
	if (SettingsScreenWidget || TutorialScreenWidget || ReceiptShopWidget || bIsScreenTransitionActive || !BoundGameMode || BoundGameMode->GetRunState() != EPGRunState::Home)
	{
		return;
	}

	ShowReceiptShopScreen();
}

void UPGHomeFlowComponent::HandleReceiptShopClosed()
{
	if (ReceiptShopWidget)
	{
		ReceiptShopWidget->OnReceiptShopClosed.RemoveAll(this);
		ReceiptShopWidget = nullptr;
	}

	if (ShopScreenWidget)
	{
		ShopScreenWidget->SetShopInteractionEnabled(true);
	}
	else if (HomeScreenWidget)
	{
		HomeScreenWidget->SetHomeButtonsEnabled(true);
	}
}

void UPGHomeFlowComponent::HandleSettingsRequested()
{
	if (TutorialScreenWidget || ShopScreenWidget || ReceiptShopWidget || bIsScreenTransitionActive)
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
	if (SettingsScreenWidget || ShopScreenWidget || ReceiptShopWidget || bIsScreenTransitionActive)
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
	if (!OwnerPlayerController || SettingsScreenWidget || TutorialScreenWidget || ReceiptShopWidget)
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
	if (!OwnerPlayerController || TutorialScreenWidget || ReceiptShopWidget)
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

void UPGHomeFlowComponent::ShowReceiptShopScreen()
{
	if (!OwnerPlayerController || ReceiptShopWidget || !UserDataSubsystem)
	{
		return;
	}

	UClass* WidgetClass = CachedShopSettings.ReceiptShopWidgetClass.Get();
	UClass* ItemWidgetClass = CachedShopSettings.ReceiptShopPackItemWidgetClass.Get();
	if (!WidgetClass)
	{
		UE_LOG(LogPGHomeFlow, Warning, TEXT("ReceiptShopWidgetClass is not set. Create WBP_ReceiptShop and assign it in Go Deliver Settings > Shop."));
		return;
	}
	if (!WidgetClass->IsChildOf(UPGReceiptShopWidget::StaticClass()))
	{
		UE_LOG(LogPGHomeFlow, Warning, TEXT("ReceiptShopWidgetClass '%s' must inherit UPGReceiptShopWidget."), *GetNameSafe(WidgetClass));
		return;
	}
	if (!ItemWidgetClass || !ItemWidgetClass->IsChildOf(UPGReceiptShopPackItemWidget::StaticClass()))
	{
		UE_LOG(LogPGHomeFlow, Warning, TEXT("ReceiptShopPackItemWidgetClass must inherit UPGReceiptShopPackItemWidget."));
		return;
	}

	ReceiptShopWidget = CreateWidget<UPGReceiptShopWidget>(OwnerPlayerController, TSubclassOf<UPGReceiptShopWidget>(WidgetClass));
	if (!ReceiptShopWidget)
	{
		return;
	}

	if (ShopScreenWidget)
	{
		ShopScreenWidget->SetShopInteractionEnabled(false);
	}
	else if (HomeScreenWidget)
	{
		HomeScreenWidget->SetHomeButtonsEnabled(false);
	}

	ReceiptShopWidget->OnReceiptShopClosed.AddUObject(this, &UPGHomeFlowComponent::HandleReceiptShopClosed);
	ReceiptShopWidget->AddToViewport(30);
	ReceiptShopWidget->InitializeReceiptShop(UserDataSubsystem, TSubclassOf<UPGReceiptShopPackItemWidget>(ItemWidgetClass), CachedShopSettings.ReceiptPacks);
	ReceiptShopWidget->PlayOpenAnimation();
}

void UPGHomeFlowComponent::HandleRunCountdownStarted(float Duration)
{
	// 카운트다운이 시작되면 Home 계열 모달은 모두 닫고, 러너 카메라와 카운트다운 UI만 남긴다.
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
		ShopScreenWidget->OnShopCloseRequested.RemoveAll(this);
		ShopScreenWidget->OnReceiptShopRequested.RemoveAll(this);
		ShopScreenWidget->RemoveFromParent();
		ShopScreenWidget = nullptr;
	}

	if (ReceiptShopWidget)
	{
		ReceiptShopWidget->OnReceiptShopClosed.RemoveAll(this);
		ReceiptShopWidget->RemoveFromParent();
		ReceiptShopWidget = nullptr;
	}

	if (HomeScreenWidget)
	{
		if (bIsScreenTransitionActive && PendingScreenTransition == EPGHomeFlowTransition::HomeToCountdown)
		{
			// 카메라 전환 구간은 전역 Fade가 화면을 덮은 뒤에 교체한다.
			// Home 퇴장 애니메이션을 함께 보여주면 Fade와 중복되어 전환 타이밍이 어색해진다.
			HomeScreenWidget->OnStartDeliveryRequested.RemoveAll(this);
			HomeScreenWidget->OnShopRequested.RemoveAll(this);
			HomeScreenWidget->OnSettingsRequested.RemoveAll(this);
			HomeScreenWidget->OnHelpRequested.RemoveAll(this);
			HomeScreenWidget->OnReceiptShopRequested.RemoveAll(this);
			HomeScreenWidget->RemoveFromParent();
			HomeScreenWidget = nullptr;
		}
		else
		{
			HomeScreenWidget->PlayExitAnimation(CachedHomeFlowSettings.HomeExitAnimationDuration);
		}
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
