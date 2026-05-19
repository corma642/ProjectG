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
 * 로컬 PlayerController가 소유하는 Home 흐름 관리자입니다.
 * Home UI, Home/Shop 카메라, Settings/Help/Shop 화면, START 이후 카운트다운 진입을 한곳에서 연결합니다.
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

	// 이 컴포넌트를 소유하는 로컬 플레이어 컨트롤러입니다. UI 생성과 카메라 전환의 기준이 됩니다.
	UPROPERTY(Transient)
	TObjectPtr<APGPlayerController> OwnerPlayerController;

	// Home/Countdown/Running 상태 전환 이벤트를 구독하는 GameMode입니다.
	UPROPERTY(Transient)
	TObjectPtr<APGGameModeBase> BoundGameMode;

	// START/Retry 전에 필수 UI, 사운드, 데이터 에셋 예열 완료를 확인하는 서브시스템입니다.
	UPROPERTY(Transient)
	TObjectPtr<UPGRuntimePreloadSubsystem> RuntimePreloadSubsystem;

	// Home 화면의 누적 영수증 표시를 갱신하는 지갑 서브시스템입니다.
	UPROPERTY(Transient)
	TObjectPtr<UPGPlayerWalletSubsystem> PlayerWalletSubsystem;

	// Shop 프리뷰와 장착 캐릭터 복구를 담당하는 상점 서브시스템입니다.
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

	// 태그 검색 비용을 반복하지 않도록 Home/Shop 카메라를 최초 탐색 후 캐시합니다.
	TWeakObjectPtr<ACameraActor> CachedHomeCameraActor;
	TWeakObjectPtr<ACameraActor> CachedShopCameraActor;

	FPGHomeFlowSettings CachedHomeFlowSettings;
	FPGShopSettings CachedShopSettings;
	bool bIsBoundToGameMode = false;

	// START를 눌렀지만 런타임 예열이 끝나지 않은 경우, 완료 시 자동 시작하기 위한 대기 플래그입니다.
	bool bPendingStartDeliveryAfterPreload = false;

	// Retry URL 옵션으로 들어온 경우 Home UI를 건너뛰고 카운트다운을 요청했는지 추적합니다.
	bool bRetryCountdownRequested = false;
};
