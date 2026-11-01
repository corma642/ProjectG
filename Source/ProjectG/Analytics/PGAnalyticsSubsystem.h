// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tutorial/PGTutorialTypes.h"
#include "PGAnalyticsSubsystem.generated.h"

class APGGameModeBase;
class IAnalyticsProvider;
struct FActorsInitializedParams;
struct FAnalyticsEventAttribute;

/**
 * 플레이 지표 이벤트(앱 실행, 런 시작·실패·이어하기, 튜토리얼 단계·되감기·완료, 상점 구매)를 엔진 Analytics 제공자에 기록하는 GameInstance Subsystem입니다.
 * 제공자는 Config/DefaultEngine.ini의 빌드 종류별 섹션([AnalyticsDevelopment]·[AnalyticsDebug]·[Analytics]=Shipping)의 ProviderModuleName이 정합니다.
 * Editor·Development 빌드는 FileLogging 플러그인이 Saved/Analytics/<세션>.analytics 파일에 쓰고, Shipping은 Firebase 제공자 모듈이 들어오기 전까지 비워 두어 이벤트를 버립니다.
 * 게임 코드는 이벤트 이름과 속성만 넘기고 전송·저장은 제공자가 맡습니다(2026-10-11 출시 심사 #29).
 */
UCLASS()
class PROJECTG_API UPGAnalyticsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** 설정된 제공자를 만들어 세션을 시작하고 app_open을 기록합니다. 이후 열리는 게임 월드마다 GameMode·튜토리얼 delegate에 바인딩합니다. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** 세션을 끝내 남은 이벤트를 비웁니다. */
	virtual void Deinitialize() override;

	/** 상점 구매가 저장까지 확정된 직후 UserData가 호출합니다. BalanceReason(CharacterPurchase·MapPurchase)이 상품 종류를 구분합니다. */
	void RecordShopPurchase(FName ItemId, int32 PriceReceipts, int32 NewBalance, FName BalanceReason);

	/** 광고가 끝났을 때 UPGAdsSubsystem이 호출합니다. EventName은 ad_rewarded·ad_interstitial, Placement는 배치 이름, bRewarded는 보상 지급 여부입니다. */
	void RecordAdEvent(const TCHAR* EventName, FName Placement, bool bRewarded);

private:
	void HandleWorldInitializedActors(const FActorsInitializedParams& Params);
	void HandleApplicationWillEnterBackground();

	UFUNCTION()
	void HandleRunStarted();

	UFUNCTION()
	void HandleRunFailed(FName FailureReason);

	UFUNCTION()
	void HandleRunContinueCountdownStarted(float Duration);

	UFUNCTION()
	void HandleRunRewindStarted(FName FailureReason);

	UFUNCTION()
	void HandleTutorialStepChanged(EPGTutorialStep Step);

	UFUNCTION()
	void HandleTutorialCompleted(int32 RewardReceipts);

	/** 제공자가 없으면 버립니다. bFlush는 실패·구매처럼 앱이 곧 꺼질 수 있는 순간에 기록을 바로 디스크/네트워크로 내보내라는 뜻입니다. */
	void RecordEvent(const TCHAR* EventName, TArray<FAnalyticsEventAttribute>&& Attributes, bool bFlush = false);

	TSharedPtr<IAnalyticsProvider> Provider;

	// 현재 게임 월드의 GameMode. 런 실패 속성(점수·거리·적재물·이벤트)을 읽을 때 쓰며 월드 전환 시 사라지므로 약한 참조만 둡니다.
	TWeakObjectPtr<APGGameModeBase> BoundGameMode;

	// 되감기 이벤트에 '어느 단계에서 실패했는지'를 붙이기 위해 마지막 안내 단계를 기억합니다.
	EPGTutorialStep LastTutorialStep = EPGTutorialStep::None;
};
