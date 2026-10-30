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
class UPGReceiptShopPackItemWidget;
class UPGReceiptShopWidget;
class UPGRunCountdownWidget;
class UPGScreenTransitionFadeWidget;
class UPGSettingsScreenWidget;
class UPGShopScreenWidget;
class UPGQuitConfirmWidget;
class UPGUserDataSubsystem;
enum class EPGRuntimePreloadGroup : uint8;

/** 전역 fade가 화면을 덮은 시점에 적용할 Home 계열 화면 전환 종류입니다. */
enum class EPGHomeFlowTransition : uint8
{
	None,
	StartupMapRedirect,
	HomeToShop,
	ShopToHome,
	HomeToCountdown,
	ShopToMapLevel,
	HomeToTutorialLevel
};

/** 서로 다른 preload group에서 동시에 대기할 수 있는 Home 요청을 추적하는 내부 bitmask입니다. */
enum class EPGHomePendingPreloadRequest : uint8
{
	None = 0,
	StartDelivery = 1 << 0,
	Shop = 1 << 1,
	ReceiptShop = 1 << 2
};
ENUM_CLASS_FLAGS(EPGHomePendingPreloadRequest);

/**
 * 로컬 PlayerController가 소유하는 Home 흐름 관리자입니다.
 * Home/Shop 카메라, Home/Settings/Shop UI, START 이후 카운트다운 진입, 도움말·첫 START의 튜토리얼 스테이지 진입을 한 흐름에서 연결합니다.
 */
UCLASS(ClassGroup = (ProjectG), meta = (BlueprintSpawnableComponent))
class PROJECTG_API UPGHomeFlowComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 상시 Tick 없이 GameMode·preload·UI delegate로만 화면 흐름을 구동하도록 구성합니다. */
	UPGHomeFlowComponent();

	/** 소유 Controller와 GameMode/Subsystem을 연결하고 현재 런 상태에 맞는 첫 화면을 복구합니다. */
	virtual void BeginPlay() override;

	/** 등록한 delegate와 이 컴포넌트가 생성한 모든 위젯을 수명 종료 전에 해제합니다. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 뒤로가기를 열린 Home 모달 닫기 → Home 종료 확인 순으로 처리하고 처리했으면 true를 반환합니다. 런 중에는 false입니다. */
	bool TryHandleBack();

private:
	UFUNCTION()
	void HandleRunCountdownStarted(float Duration);

	UFUNCTION()
	void HandleRunStarted();

	UFUNCTION()
	void HandleRuntimePreloadCompleted(EPGRuntimePreloadGroup Group);

	UFUNCTION()
	void HandleReceiptBalanceChanged(int32 NewBalance, FName Reason);

	UFUNCTION()
	void HandleBestRunScoreChanged(int32 NewBestScore, FName Reason);

	void BindGameMode();
	void UnbindGameMode();
	void BindRuntimePreloadSubsystem();
	void BindUserDataSubsystem();
	void UnbindUserDataSubsystem();
	void HandleRetryCountdownInitializationReady();
	void RequestStartDeliveryAfterPreload();
	bool TryRedirectToEquippedStartupMap();
	void ShowHomeScreen();
	void SetHomeCameraViewTarget();
	void SetShopCameraViewTarget();
	ACameraActor* FindCameraByTag(FName CameraTag, TWeakObjectPtr<ACameraActor>& CameraCache) const;
	void RestoreShopPreviewRunner(bool bRestoreHomeTransform = false);
	void BlendToRunnerCamera();
	void BlendToHomeCamera();
	void ShowCountdown(float Duration);
	void CleanupWidgets();
	void RemoveHomeScreenWidget();
	void RemoveCountdownWidget();
	void RemoveScreenTransitionFadeWidget();
	void RemoveShopScreenWidget();
	void RemoveReceiptShopWidget();
	void RemoveSettingsScreenWidget();
	void QueuePendingPreloadRequest(EPGHomePendingPreloadRequest Request);
	void ConsumePendingPreloadRequest(EPGHomePendingPreloadRequest Request);
	bool HasPendingPreloadRequest(EPGHomePendingPreloadRequest Request) const;
	bool StartScreenTransition(EPGHomeFlowTransition Transition);
	void ApplyPendingScreenTransition();
	void FinishScreenTransition();
	bool AreShopScreenResourcesReady() const;
	void ShowShopScreen();
	void HandleStartDeliveryRequested();
	void HandleShopRequested();
	void HandleShopCloseRequested();
	void HandleShopMapTravelRequested(FName MapId);
	void HandleReceiptShopRequested();
	void HandleReceiptShopClosed();
	void HandleSettingsRequested();
	void HandleSettingsScreenClosed();
	void HandleHelpRequested();
	void StartTutorialStage();
	void ShowSettingsScreen();
	void ShowReceiptShopScreen();
	void ShowQuitConfirm();
	void RemoveQuitConfirmWidget();
	void HandleQuitConfirmed();
	void HandleQuitCancelled();
	void ApplyHomeInputMode();
	void ApplyGameInputMode();

	// 이 컴포넌트를 소유하는 로컬 플레이어 컨트롤러입니다. UI 생성과 카메라 전환의 기준입니다.
	UPROPERTY(Transient)
	TObjectPtr<APGPlayerController> OwnerPlayerController;

	// Home/Countdown/Running 상태 전환 이벤트를 구독하는 GameMode입니다.
	UPROPERTY(Transient)
	TObjectPtr<APGGameModeBase> BoundGameMode;

	// START/Retry 전에 필수 UI, 사운드, 데이터 에셋 예열 완료를 확인하는 서브시스템입니다.
	UPROPERTY(Transient)
	TObjectPtr<UPGRuntimePreloadSubsystem> RuntimePreloadSubsystem;

	// 누적 영수증, 캐릭터 보유/장착, Shop 프리뷰를 통합 관리하는 유저 데이터 서브시스템입니다.
	UPROPERTY(Transient)
	TObjectPtr<UPGUserDataSubsystem> UserDataSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UPGHomeScreenWidget> HomeScreenWidget;

	UPROPERTY(Transient)
	TObjectPtr<UPGRunCountdownWidget> CountdownWidget;

	UPROPERTY(Transient)
	TObjectPtr<UPGScreenTransitionFadeWidget> ScreenTransitionFadeWidget;

	UPROPERTY(Transient)
	TObjectPtr<UPGShopScreenWidget> ShopScreenWidget;

	UPROPERTY(Transient)
	TObjectPtr<UPGReceiptShopWidget> ReceiptShopWidget;

	UPROPERTY(Transient)
	TObjectPtr<UPGSettingsScreenWidget> SettingsScreenWidget;

	// Home에서 뒤로가기로 띄우는 앱 종료 확인 모달입니다.
	UPROPERTY(Transient)
	TObjectPtr<UPGQuitConfirmWidget> QuitConfirmWidget;

	// 태그 검색 비용을 반복하지 않도록 Home/Shop 카메라를 최초 탐색 후 캐시합니다.
	TWeakObjectPtr<ACameraActor> CachedHomeCameraActor;
	TWeakObjectPtr<ACameraActor> CachedShopCameraActor;

	FPGHomeFlowSettings CachedHomeFlowSettings;
	FPGShopSettings CachedShopSettings;
	bool bIsBoundToGameMode = false;

	// 서로 다른 group의 예열 완료가 역순으로 와도 각 사용자 요청을 잃지 않도록 bitmask로 보관합니다.
	EPGHomePendingPreloadRequest PendingPreloadRequests = EPGHomePendingPreloadRequest::None;

	// StartFlow URL 옵션(Retry·튜토리얼)으로 들어온 경우 Home UI를 건너뛰고 카운트다운을 요청하는지 추적합니다.
	bool bRetryCountdownRequested = false;

	// Retry 카운트다운이 Controller 초기화 이후 다음 Tick부터 시작 가능한지 추적합니다.
	bool bRetryCountdownInitializationReady = false;
	bool bIsScreenTransitionActive = false;
	EPGHomeFlowTransition PendingScreenTransition = EPGHomeFlowTransition::None;
	FName PendingMapTravelLevelName;
};
