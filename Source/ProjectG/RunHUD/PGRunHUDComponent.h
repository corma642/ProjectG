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

/**
 * 로컬 PlayerController가 소유하는 러닝 HUD 관리자입니다.
 * 카운트다운부터 실패 결과 화면 뒤까지 HUD를 유지하고, Pause/Settings와 적재물 리스트 갱신을 연결합니다.
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

	// START 시점에 실제 러너가 Possess되면 점수/거리/적재물 델리게이트를 바인딩합니다.
	void HandleRunnerPossessed(APGRunnerCharacter* RunnerCharacter);

	// 러너 Possess가 풀리거나 레벨 종료 시 HUD 데이터 바인딩을 정리합니다.
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
	void HandleCargoDelivered(int32 CargoInstanceId, int32 Score, int32 Receipts, FVector DeliveryLocation);
	void HandlePauseRequested();
	void HandleResumeRequested();
	void HandlePauseSettingsRequested();
	void HandlePauseHomeRequested();
	void HandleSettingsScreenClosed();

	void LoadSettings();
	bool AreRuntimeResourcesReady() const;
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

	// UI 생성, 입력 모드 전환, 일시정지 처리를 수행할 로컬 컨트롤러입니다.
	UPROPERTY(Transient)
	TObjectPtr<APGPlayerController> OwnerPlayerController;

	// 런 상태 이벤트를 구독하는 GameMode입니다.
	UPROPERTY(Transient)
	TObjectPtr<APGGameModeBase> BoundGameMode;

	// HUD의 점수/거리/영수증 텍스트와 연결되는 현재 러너의 진행 컴포넌트입니다.
	UPROPERTY(Transient)
	TObjectPtr<UPGRunProgressComponent> BoundRunProgressComponent;

	// HUD 하단 배달 적재물 리스트와 완료 인디케이터가 읽는 적재물 컴포넌트입니다.
	UPROPERTY(Transient)
	TObjectPtr<UPGCargoStackComponent> BoundCargoStackComponent;

	UPROPERTY(Transient)
	TObjectPtr<UPGRunHUDWidget> HUDWidget;

	UPROPERTY(Transient)
	TObjectPtr<UPGRunPauseWidget> PauseWidget;

	UPROPERTY(Transient)
	TObjectPtr<UPGSettingsScreenWidget> SettingsScreenWidget;

	FPGRunHUDSettings CachedRunHUDSettings;
	FPGHomeFlowSettings CachedHomeFlowSettings;

	// 적재물 남은 시간/진행률은 매 프레임이 아니라 설정된 간격으로만 갱신합니다.
	float CargoRefreshElapsed = 0.0f;
	bool bIsBoundToGameMode = false;
};
