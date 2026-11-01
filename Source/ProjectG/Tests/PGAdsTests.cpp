// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ads/PGAdsSubsystem.h"
#include "Misc/AutomationTest.h"
#include "Settings/PGGameSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPGAdsInterstitialGateTest,
	"ProjectG.Core.Ads.InterstitialGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPGAdsInterstitialGateTest::RunTest(const FString& Parameters)
{
	FPGAdsSettings Settings;
	Settings.bEnableAds = true;
	Settings.InterstitialEveryNthFailure = 3;
	Settings.InterstitialMinIntervalSeconds = 90.0f;

	TestFalse(TEXT("N번째 전에는 뜨지 않는다"), UPGAdsSubsystem::ShouldShowInterstitial(2, 1000.0, Settings));
	TestTrue(TEXT("N번째 실패, 간격 충분"), UPGAdsSubsystem::ShouldShowInterstitial(3, 1000.0, Settings));
	TestTrue(TEXT("광고가 늦게 준비돼 N을 넘겨도 뜬다"), UPGAdsSubsystem::ShouldShowInterstitial(5, 1000.0, Settings));
	TestFalse(TEXT("최소 간격 전에는 뜨지 않는다"), UPGAdsSubsystem::ShouldShowInterstitial(3, 30.0, Settings));

	Settings.InterstitialEveryNthFailure = 0;
	TestFalse(TEXT("N이 0이면 전면 광고 없음"), UPGAdsSubsystem::ShouldShowInterstitial(10, 1000.0, Settings));

	Settings.InterstitialEveryNthFailure = 3;
	Settings.bEnableAds = false;
	TestFalse(TEXT("광고 기능이 꺼지면 뜨지 않는다"), UPGAdsSubsystem::ShouldShowInterstitial(10, 1000.0, Settings));
	return true;
}
