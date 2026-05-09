// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Settings/PGGameSettings.h"
#include "PGHomeFlowComponent.generated.h"

class APGGameModeBase;
class APGPlayerController;
class UPGRuntimePreloadSubsystem;
class UPGHomeScreenWidget;
class UPGRunCountdownWidget;
class UPGSettingsScreenWidget;

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

	void BindGameMode();
	void UnbindGameMode();
	void BindRuntimePreloadSubsystem();
	void RequestStartDeliveryAfterPreload();
	void ShowHomeScreen();
	void SetHomeCameraViewTarget();
	void BlendToRunnerCamera();
	void ShowCountdown(float Duration);
	void CleanupWidgets();
	void HandleStartDeliveryRequested();
	void HandleSettingsRequested();
	void HandleSettingsScreenClosed();
	void ShowSettingsScreen();
	void ApplyHomeInputMode();
	void ApplyGameInputMode();

	UPROPERTY(Transient)
	TObjectPtr<APGPlayerController> OwnerPlayerController;

	UPROPERTY(Transient)
	TObjectPtr<APGGameModeBase> BoundGameMode;

	UPROPERTY(Transient)
	TObjectPtr<UPGRuntimePreloadSubsystem> RuntimePreloadSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UPGHomeScreenWidget> HomeScreenWidget;

	UPROPERTY(Transient)
	TObjectPtr<UPGRunCountdownWidget> CountdownWidget;

	UPROPERTY(Transient)
	TObjectPtr<UPGSettingsScreenWidget> SettingsScreenWidget;

	FPGHomeFlowSettings CachedHomeFlowSettings;
	bool bIsBoundToGameMode = false;
	bool bPendingStartDeliveryAfterPreload = false;
	bool bRetryCountdownRequested = false;
};
