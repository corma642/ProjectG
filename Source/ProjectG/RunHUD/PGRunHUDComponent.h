// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Settings/PGGameSettings.h"
#include "PGRunHUDComponent.generated.h"

class APGGameModeBase;
class APGPlayerController;
class APGRunnerCharacter;
class UPGCargoStackComponent;
class UPGRunHUDWidget;
class UPGRunPauseWidget;
class UPGRunProgressComponent;
class UPGSettingsScreenWidget;
struct FStreamableHandle;

/**
 * 러닝 중 HUD와 Pause 화면의 생명주기, 데이터 바인딩, Settings 재사용 흐름을 관리하는 로컬 플레이어 컴포넌트입니다.
 * 실제 HUD/Pause 레이아웃은 Widget Blueprint가 담당하고, 이 컴포넌트는 런 상태와 데이터 연결만 담당합니다.
 */
UCLASS(ClassGroup = (ProjectG), meta = (BlueprintSpawnableComponent))
class PROJECTG_API UPGRunHUDComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPGRunHUDComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void HandleRunnerPossessed(APGRunnerCharacter* RunnerCharacter);
	void HandleRunnerUnpossessed();

private:
	UFUNCTION()
	void HandleRunCountdownStarted(float Duration);

	UFUNCTION()
	void HandleRunStarted();

	UFUNCTION()
	void HandleRunFailed(FName FailureReason);

	UFUNCTION()
	void HandleDisplayedDistanceChanged(float DisplayedDistanceMeters);

	UFUNCTION()
	void HandleScoreChanged(int32 Score, FName Reason);

	UFUNCTION()
	void HandleReceiptsChanged(int32 Receipts, FName Reason);

	void HandleCargoListChanged();
	void HandlePauseRequested();
	void HandleResumeRequested();
	void HandlePauseSettingsRequested();
	void HandleSettingsScreenClosed();

	void LoadSettings();
	void StartPreloadingResources();
	void HandleResourcesLoaded();
	void ReleaseResourceLoadHandle();
	void BindGameMode();
	void UnbindGameMode();
	void BindRunProgress(UPGRunProgressComponent* RunProgressComponent);
	void UnbindRunProgress();
	void BindCargoStack(UPGCargoStackComponent* CargoStackComponent);
	void UnbindCargoStack();
	void ShowHUD();
	void CleanupHUD();
	void UpdateHUDInteractionState() const;
	void RefreshHUDValues();
	void RefreshCargoList();
	void ShowPauseScreen();
	void CleanupPauseScreen(bool bResumeGame);
	void ShowSettingsScreen();
	void CleanupSettingsScreen();
	void ApplyGameInputMode() const;
	void ApplyPauseInputMode() const;

	UPROPERTY(Transient)
	TObjectPtr<APGPlayerController> OwnerPlayerController;

	UPROPERTY(Transient)
	TObjectPtr<APGGameModeBase> BoundGameMode;

	UPROPERTY(Transient)
	TObjectPtr<UPGRunProgressComponent> BoundRunProgressComponent;

	UPROPERTY(Transient)
	TObjectPtr<UPGCargoStackComponent> BoundCargoStackComponent;

	UPROPERTY(Transient)
	TObjectPtr<UPGRunHUDWidget> HUDWidget;

	UPROPERTY(Transient)
	TObjectPtr<UPGRunPauseWidget> PauseWidget;

	UPROPERTY(Transient)
	TObjectPtr<UPGSettingsScreenWidget> SettingsScreenWidget;

	TSharedPtr<FStreamableHandle> ResourceLoadHandle;

	FPGRunHUDSettings CachedRunHUDSettings;
	FPGHomeFlowSettings CachedHomeFlowSettings;
	float CargoRefreshElapsed = 0.0f;
	bool bIsBoundToGameMode = false;
	bool bResourcesReady = false;
};
