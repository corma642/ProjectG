// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Settings/PGGameSettings.h"
#include "PGHomeFlowComponent.generated.h"

class APGGameModeBase;
class APGPlayerController;
class APGRunnerCharacter;
class ACameraActor;
class UPGRuntimePreloadSubsystem;
class UPGHomeScreenWidget;
class UPGPlayerShopSubsystem;
class UPGRunCountdownWidget;
class UPGShopScreenWidget;
class UPGSettingsScreenWidget;
class UPGTutorialScreenWidget;
class UPGPlayerWalletSubsystem;

/**
 * 홈 화면 표시, 홈 카메라 전환, START DELIVERY, 카운트다운 UI를 로컬 플레이어 기준으로 자동 관리합니다.
 */
UCLASS(ClassGroup = (ProjectG), meta = (BlueprintSpawnableComponent))
class PROJECTG_API UPGHomeFlowComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPGHomeFlowComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleRunCountdownStarted(float Duration);

	UFUNCTION()
	void HandleRunStarted();

	UFUNCTION()
	void HandleRuntimePreloadCompleted();

	UFUNCTION()
	void HandleReceiptBalanceChanged(int32 NewBalance, FName Reason);

	void BindGameMode();
	void UnbindGameMode();
	void BindRuntimePreloadSubsystem();
	void BindPlayerWalletSubsystem();
	void BindPlayerShopSubsystem();
	void UnbindPlayerWalletSubsystem();
	void UnbindPlayerShopSubsystem();
	void RequestStartDeliveryAfterPreload();
	void ShowHomeScreen();
	void SetHomeCameraViewTarget();
	void SetShopCameraViewTarget();
	ACameraActor* FindCameraByTag(FName CameraTag, TWeakObjectPtr<ACameraActor>& CameraCache) const;
	APGRunnerCharacter* FindPreviewRunner() const;
	void RestoreShopPreviewRunner(bool bRestoreHomeTransform = false);
	void BlendToRunnerCamera();
	void BlendToHomeCamera();
	void ShowCountdown(float Duration);
	void CleanupWidgets();
	void HandleStartDeliveryRequested();
	void HandleShopRequested();
	void HandleShopScreenClosed();
	void HandleSettingsRequested();
	void HandleSettingsScreenClosed();
	void HandleHelpRequested();
	void HandleTutorialClosed();
	void ShowSettingsScreen();
	void ShowTutorialScreen();
	void ApplyHomeInputMode();
	void ApplyGameInputMode();

	UPROPERTY(Transient)
	TObjectPtr<APGPlayerController> OwnerPlayerController;

	UPROPERTY(Transient)
	TObjectPtr<APGGameModeBase> BoundGameMode;

	UPROPERTY(Transient)
	TObjectPtr<UPGRuntimePreloadSubsystem> RuntimePreloadSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UPGPlayerWalletSubsystem> PlayerWalletSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UPGPlayerShopSubsystem> PlayerShopSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UPGHomeScreenWidget> HomeScreenWidget;

	UPROPERTY(Transient)
	TObjectPtr<UPGRunCountdownWidget> CountdownWidget;

	UPROPERTY(Transient)
	TObjectPtr<UPGShopScreenWidget> ShopScreenWidget;

	UPROPERTY(Transient)
	TObjectPtr<UPGSettingsScreenWidget> SettingsScreenWidget;

	UPROPERTY(Transient)
	TObjectPtr<UPGTutorialScreenWidget> TutorialScreenWidget;

	TWeakObjectPtr<ACameraActor> CachedHomeCameraActor;
	TWeakObjectPtr<ACameraActor> CachedShopCameraActor;

	FPGHomeFlowSettings CachedHomeFlowSettings;
	FPGShopSettings CachedShopSettings;
	bool bIsBoundToGameMode = false;
	bool bPendingStartDeliveryAfterPreload = false;
	bool bRetryCountdownRequested = false;
};
