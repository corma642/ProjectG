// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PGAdsSubsystem.generated.h"

struct FPGAdsSettings;

/** 보상형 광고를 바로 보여 줄 수 있는 상태(IsRewardedAdReady)가 바뀔 때 알립니다. 열려 있는 화면이 광고 버튼을 켜고 끄는 데 씁니다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPGRewardedAdReadyChangedSignature, bool, bReady);

/** 보상형 광고가 끝났을 때 한 번 불립니다. bRewarded는 끝까지 보고 보상 조건을 채웠을 때만 true입니다. */
using FPGRewardedAdFinished = TFunction<void(bool bRewarded)>;

/** 전면 광고가 닫혔거나 보여 주지 못해 흐름을 이어가야 할 때 한 번 불립니다. */
using FPGInterstitialAdFinished = TFunction<void()>;

/**
 * 보상형·전면 광고를 게임 코드에 한 가지 모양으로 제공하는 GameInstance Subsystem입니다(2026-10-11 출시 심사 #41).
 * Android는 Java GoDeliverAds(AdMob + UMP 동의 창)가 광고를 로드·표시하고 결과를 nativeOnAdEvent로 돌려주며,
 * 그 외 플랫폼(에디터·데스크톱)은 설정한 시간 뒤 성공 콜백만 주는 가짜 광고로 UI 흐름을 검증합니다.
 * 배치(이어하기·2배·상점·전면)는 호출하는 화면이 정하고, 이 클래스는 준비 여부·표시·결과 콜백·전면 광고 빈도 규칙만 맡습니다.
 * 모든 콜백은 게임 스레드에서 옵니다. 광고 단위 ID와 빈도는 Project Settings > Go Deliver! Settings > Ads, Android 앱 ID는 DefaultEngine.ini [ProjectG.Ads]입니다.
 */
UCLASS()
class PROJECTG_API UPGAdsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** 설정이 켜져 있으면 Android는 Java 광고 래퍼를 초기화하고(동의 → SDK → 선로드), 그 외 플랫폼은 가짜 광고를 준비 상태로 둡니다. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	virtual void Deinitialize() override;

	/** 광고 기능이 켜져 있고 이 플랫폼에서 광고를 요청할 수 있으면 true입니다. false면 화면은 광고 배치(버튼·티켓) 자체를 접습니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Ads")
	bool IsAdsAvailable() const { return bAdsAvailable; }

	/** 광고 기능이 켜져 있고 보상형 광고가 로드돼 바로 보여 줄 수 있으면 true입니다. 버튼 활성화 판단에 씁니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Ads")
	bool IsRewardedAdReady() const;

	/**
	 * 보상형 광고를 보여 주고 끝나면 OnFinished(bRewarded)를 한 번 부릅니다. 준비가 안 됐거나 다른 광고가 진행 중이면 바로 false로 부릅니다.
	 * Placement는 지표용 이름입니다(Continue·DoubleReceipts·ShopReceipts).
	 */
	void ShowRewardedAd(FName Placement, FPGRewardedAdFinished OnFinished);

	/**
	 * 결과 화면을 떠날 때(다시 도전·홈) 부릅니다. 실패 횟수·최소 간격 규칙을 채우고 전면 광고가 준비됐으면 띄운 뒤 닫힐 때 OnFinished를 부르고 true를 돌려줍니다.
	 * 아니면 아무 것도 하지 않고 false를 돌려주므로 호출자가 바로 진행합니다.
	 */
	bool TryShowInterstitialOnResultExit(FPGInterstitialAdFinished OnFinished);

	/** 전면 광고 빈도 규칙입니다. N번째 실패 이상이고 마지막 전면 광고 뒤 최소 간격이 지났을 때만 true입니다. 테스트하기 쉽게 분리했습니다. */
	static bool ShouldShowInterstitial(int32 FailuresSinceLastAd, double SecondsSinceLastAd, const FPGAdsSettings& Settings);

	/**
	 * Java 또는 가짜 광고가 보내는 이벤트입니다. Kind 0 보상형·1 전면, Event 0 로드됨·1 로드 실패·2 닫힘(RewardAmount > 0이면 보상)·3 표시 실패.
	 * 게임 스레드에서만 부릅니다.
	 */
	void HandleAdEvent(int32 Kind, int32 Event, int32 RewardAmount);

	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Ads")
	FPGRewardedAdReadyChangedSignature OnRewardedAdReadyChanged;

private:
	void StartFakeAd(int32 Kind);
	void BroadcastRewardedReadyIfChanged(bool bWasReady);

	bool bAdsAvailable = false;
	bool bRewardedReady = false;
	bool bInterstitialReady = false;
	FName PendingRewardedPlacement;
	FPGRewardedAdFinished PendingRewardedFinished;
	FPGInterstitialAdFinished PendingInterstitialFinished;

	// 결과 화면을 떠난 횟수와 마지막 전면 광고 시각(FPlatformTime::Seconds). 앱 실행 동안만 유지합니다.
	int32 FailuresSinceInterstitial = 0;
	double LastInterstitialSeconds = -1.0e9;
	FTimerHandle FakeAdTimerHandle;
};
