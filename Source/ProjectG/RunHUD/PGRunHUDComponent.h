// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Cargo/PGCargoStackComponent.h"
#include "Components/ActorComponent.h"
#include "Settings/PGGameSettings.h"
#include "UI/PGRunGuideWidget.h"
#include "PGRunHUDComponent.generated.h"

class APGGameModeBase;
class APGPlayerController;
class APGRunnerCharacter;
class UPGCargoStackComponent;
class UPGRunHUDWidget;
class UPGRunPauseWidget;
class UPGRunProgressComponent;
class UPGSettingsScreenWidget;
class UPGUserDataSubsystem;
class UMaterialInstanceDynamic;

/**
 * 로컬 PlayerController가 소유하는 러닝 HUD 관리자입니다.
 * 카운트다운부터 실패 결과 화면 뒤까지 HUD를 유지하고, Pause/Settings와 적재물 리스트 갱신을 연결합니다.
 */
UCLASS(ClassGroup = (ProjectG), meta = (BlueprintSpawnableComponent))
class PROJECTG_API UPGRunHUDComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 상시 Tick을 끄고 런 중 필요한 HUD 갱신에서만 동적으로 활성화하도록 구성합니다. */
	UPGRunHUDComponent();

	/** Controller, GameMode와 이미 Possess된 러너를 찾아 HUD 데이터 흐름을 연결합니다. */
	virtual void BeginPlay() override;

	/** Pause/Settings 모달, HUD, delegate 참조를 수명 종료 전에 정리합니다. 일시정지 해제는 하지 않습니다. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 뒤로가기를 Pause 위 Settings 닫기 → Pause 해제 → Running·Countdown 일시정지 순으로 처리하고 처리했으면 true를 반환합니다. */
	bool TryHandleBack();

	/** 활성 Cargo 카드의 남은 시간처럼 이벤트만으로 갱신할 수 없는 HUD 상태를 제한 주기로 새로 고칩니다. */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** START 시점에 Possess된 러너의 점수·거리·적재물 delegate를 바인딩하고 HUD 값을 동기화합니다. */
	void HandleRunnerPossessed(APGRunnerCharacter* RunnerCharacter);

	/** 러너 Possess가 풀리거나 레벨 종료될 때 HUD 데이터 delegate와 캐시를 정리합니다. */
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
	void HandleRunnerJumpStarted();
	void HandlePauseRequested();
	void HandleApplicationDeactivated();
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
	void BindRunner(APGRunnerCharacter* RunnerCharacter);
	void UnbindRunner();
	void ShowHUD();
	void CleanupHUD();
	UPGUserDataSubsystem* GetUserDataSubsystem() const;
	void ShowRunGuideIfNeeded();
	void AdvanceRunGuide(EPGRunGuideStep NextStep);
	void CleanupRunGuide(bool bFadeOut);
	void UpdateHUDInteractionState() const;
	void RefreshHUDValues();
	void RefreshCargoList();
	void UpdateCargoDangerWarning();
	UMaterialInstanceDynamic* ResolveCargoDangerRing();
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

	// viewport에 추가한 러닝 HUD를 실패·Home 전환까지 생존시킵니다.
	UPROPERTY(Transient)
	TObjectPtr<UPGRunHUDWidget> HUDWidget;

	// 게임 일시정지와 입력 모드 복구를 함께 소유하는 Pause 모달입니다.
	UPROPERTY(Transient)
	TObjectPtr<UPGRunPauseWidget> PauseWidget;

	// Pause 위에 중첩되는 Settings 모달이며 닫힘 후 Pause 입력을 복구합니다.
	UPROPERTY(Transient)
	TObjectPtr<UPGSettingsScreenWidget> SettingsScreenWidget;

	// 첫 런 가이드 오버레이입니다. 저장된 완료 기록이 없을 때만 GO 시점에 만들고 완료·실패·Home 전환 시 제거합니다.
	UPROPERTY(Transient)
	TObjectPtr<UPGRunGuideWidget> RunGuideWidget;

	// 가이드 단계 판정(드래그 입력량, 점프 시작)에 쓰는 현재 러너입니다. GameMode가 소유하므로 약한 참조만 둡니다.
	TWeakObjectPtr<APGRunnerCharacter> BoundRunnerCharacter;

	// 앱 비활성화(전화·알림·홈 버튼·화면 꺼짐) 자동 일시정지 바인딩입니다.
	FDelegateHandle ApplicationDeactivatedHandle;

	// 현재 가이드가 기다리는 조작 단계입니다. None이면 가이드가 없습니다.
	EPGRunGuideStep RunGuideStep = EPGRunGuideStep::None;

	FPGRunHUDSettings CachedRunHUDSettings;
	FPGHomeFlowSettings CachedHomeFlowSettings;

	// 주기 갱신마다 재사용하는 활성 적재물 스냅샷 버퍼입니다. 매번 새 배열을 만들지 않기 위해 멤버로 둡니다.
	UPROPERTY(Transient)
	TArray<FPGActiveCargoInfo> ActiveCargoInfoBuffer;

	// 적재물 남은 시간/진행률은 매 프레임이 아니라 설정된 간격으로만 갱신합니다.
	float CargoRefreshElapsed = 0.0f;
	// Cargo > CargoDangerWarningRatio 복사본입니다. 손실 임박 경고를 켤 바구니 테두리 대비 이탈 비율(0~1)입니다.
	float CargoDangerWarningRatio = 1.0f;
	bool bIsCargoDangerActive = false;

	// 바구니 테두리 경고 링(BP_Bowl의 CargoDangerRing 컴포넌트)의 동적 머티리얼입니다. 바구니는 런마다 새로 생성되므로 약한 참조로 두고 사라지면 다시 찾습니다.
	TWeakObjectPtr<UMaterialInstanceDynamic> CargoDangerRingMaterial;
	bool bIsBoundToGameMode = false;
};
